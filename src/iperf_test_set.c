//#include <sys/types.h>
//#include <sys/ipc.h>
//#include <sys/shm.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


#include "iperf_test_set.h"
#include "iperf_api.h"


int
ts_parse_args(struct test_unit* tu)
{
	cJSON* options = cJSON_GetObjectItemCaseSensitive(tu->json_test_case, "options");
	char *str = options->valuestring;

	/*printf("options : %s\n", str);*/

	char **argvs = NULL;	//array of args
	char *tmp = strtok(str, " ");
	int count = 1;

	while (tmp)
	{
		argvs = realloc(argvs, sizeof(char *) * ++count);

		if (argvs == NULL)
			return -1;
		argvs[count - 1] = tmp;

		tmp = strtok(NULL, " ");
	}

	tu->argcs = count;
	tu->argvs = argvs;
	return 0;
}

int 
ts_run_test(struct test_unit* tu, struct iperf_test* main_test)
{
	struct iperf_test *child_test;
	child_test = iperf_new_test();

	if (!child_test)
		iperf_errexit(NULL, "create new test error - %s", iperf_strerror(i_errno));
	iperf_defaults(child_test);	/* sets defaults */

	iperf_set_test_role(child_test, 'c'); 
	iperf_set_test_server_hostname(child_test, main_test->server_hostname);
	iperf_set_test_server_port(child_test, main_test->server_port);

	if (main_test->json_output)
		iperf_set_test_json_output(child_test, 1);

	if (main_test->debug)
		child_test->debug = 1;
	
	if (!main_test->json_output)
		printf("Test %s started \n", tu->test_name); //add name

	if (ts_parse_args(tu))
		return -1;

	iperf_parse_arguments(child_test, tu->argcs, tu->argvs);

	if (iperf_run_client(child_test) < 0)
		iperf_errexit(child_test, "error - %s", iperf_strerror(i_errno));

	tu->current_test = child_test;

	if (!main_test->json_output)
		printf("Test %s finished. \n \n", tu->test_name);

	return 0;
}

int 
ts_run_bulk_test(struct iperf_test* test)
{
	struct test_set* t_set = ts_new_test_set(test->test_set_file);
	int i;

	if(!test->json_output)
		printf("Test count : %d \n", t_set->test_count);


	for (i = 0; i < t_set->test_count; ++i)
	{
		if (ts_run_test(t_set->suite[i], test))
			return -1;
	}

	ts_free_test_set(t_set);

	return 0; //add correct completion of the test to the errors(?)
}

struct test_set *
ts_new_test_set(char* path)
{
	struct test_set* t_set = malloc(sizeof(struct test_set));
	int i;
	long size = 0;
	char *str;
	FILE * inputFile = fopen(path, "r");
	cJSON *json;
	cJSON *node;

	if (!inputFile)
	{
		printf("File is not exist");
		return NULL;
	}
	else
	{
		fseek(inputFile, 0, SEEK_END);
		size = ftell(inputFile);
		fseek(inputFile, 0, SEEK_SET);
	}

	//creating json file
	str = malloc(size + 1);
	fread(str, size, 1, inputFile);
	str[size] = '\n';

	json = cJSON_Parse(str);
	t_set->json_file = json;

	fclose(inputFile);
	free(str);


	//test counting
	node = json->child;

	i = 0;

	while (node && cJSON_GetObjectItem(node, "options"))
	{
		++i;
		node = node->next;
	}

	t_set->test_count = i;


	//if (test->debug)
	//	printf("%s\n", cJSON_Print(json));

	//parsing
	t_set->suite = malloc(sizeof(struct test_unit*) * i);

	node = json->child;

	for (i = 0; i < t_set->test_count; ++i)
	{
		struct test_unit* unit = malloc(sizeof(struct test_unit));
		unit->id = i;
		unit->test_name = node->string;
		unit->json_test_case = node;
		t_set->suite[i] = unit;
		node = node->next;
	}

	return t_set;
}

int 
ts_free_test_set(struct test_set* t_set)
{
	int i;
	struct test_unit * tmp_unit;

	/*delete argvs*/

	for (i = 0; i < t_set->test_count; ++i)
	{
		tmp_unit = t_set->suite[i];
		iperf_free_test(tmp_unit->current_test);

		free(tmp_unit->argvs);

		free(tmp_unit);
	}

	free(t_set->suite);
	cJSON_Delete(t_set->json_file);
	free(t_set);

	return 0;
}
