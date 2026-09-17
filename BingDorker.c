// Extractor - Bing website crawler. 

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <string.h>
#include <pcreposix.h>

#pragma comment(lib, "ws2_32.lib")

#define NUM_THREADS 10
#define MAX_LINE_LENGTH 1000 
#define MAX_LINES 1000
#define MAX_LEN 256

#define URL_REGEX "https?://[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_./?&=-]*)?"
#define MAXLIMIT 256

struct MemoryStruct {
	char* memory;
	size_t size;
};

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp);
char* get_page_source(char* str); 
void extract_urls_curl(const char* text);

void extract_urls_curl(const char* text) {

	regex_t regex;
	regmatch_t match;

	// Compile regular expression
	if (regcomp(&regex, URL_REGEX, REG_EXTENDED) != 0) {
		printf("Could not compile regex\n");
		return;
	}

	const char* cursor = text;

	// Loop through the text to find all matches
	while (regexec(&regex, cursor, 1, &match, 0) == 0) {
		// Calculate match length
		int start = match.rm_so;
		int end = match.rm_eo;
		int len = end - start;

		// Print the extracted URL
		char url[500];

		//strncpy(url, "%.*s", len, cursor + start);
		strncpy_s(url, sizeof(url), (cursor + start), len);
		if (strstr(url, "bing") == NULL &&
			strstr(url, "microsoft") == NULL &&
			strstr(url, "wikipedia") == NULL &&
			strstr(url, "live.com") == NULL &&
			strstr(url, "w3.org") == NULL) {
			printf("Found URL: %s\n", url);
		}
		// Move the cursor past the current match
		cursor += end;
	}

	// Free compiled regular expression memory
	regfree(&regex);
}

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
	size_t realsize = size * nmemb;
	struct MemoryStruct* mem = (struct MemoryStruct*)userp;

	char* ptr = realloc(mem->memory, mem->size + realsize + 1);
	if (!ptr) {
		// Out of memory!
		printf("Not enough memory (realloc returned NULL)\n");
		return 0;
	}

	mem->memory = ptr;
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0; // Null-terminate

	return realsize;
}


char* get_page_source(char* str) {

	CURL* curl_handle;
	CURLcode res;

	struct MemoryStruct chunk;
	chunk.memory = malloc(1);  // Initialized to be reallocated by callback
	chunk.size = 0;            // No data at this point

	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();

	if (curl_handle) {
		curl_easy_setopt(curl_handle, CURLOPT_URL, str);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&chunk);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

		res = curl_easy_perform(curl_handle);

		if (res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}
		else {
			// Page source is now stored in chunk.memory variable
			return chunk.memory;
		}

		curl_easy_cleanup(curl_handle);
		free(chunk.memory);
	}

	curl_global_cleanup();
}

DWORD WINAPI WorkerThread(LPVOID lpParam) {

	char bing_url[500] = "https://www.bing.com/search?q=";
	char* info = (char*)lpParam;

	errno_t result = strcat_s(bing_url, sizeof(bing_url), info);
	errno_t result2 = strcat_s(bing_url, sizeof(bing_url), "&first=");
	char* page_src = NULL;

	for (int i = 0; i < 77; i += 11) {

		char result[1000];
		snprintf(result, sizeof(result), "%s%d", bing_url, i);
		 
		page_src = get_page_source(result);
		extract_urls_curl(page_src);
		//extract_urls_curl(page_src);

	}

}

void banner() {

   puts(

	   "--------------------------------------\n"
	   " Author: Hearsecurity\n"                
	   " Tool: BingDorker\n"                    
	   " Language: C \n"                         
	   " Github: https://github.com/hearsecurity \n"
	   "--------------------------------------------\n");

}
int main(int argc, char *argv[]) {

	if (argc < 2) {
		banner(); 
		printf("Usage: %s <wordlist>\n", argv[0]); 
		exit(0); 
	}

	FILE* file = NULL; 
	fopen_s(&file, argv[1], "r");
	
	if (file == NULL) {
		perror("Error opening file");
		return EXIT_FAILURE;
	}

	int line_count = 0;
	int threads = 0;
	char buffer[MAX_LINE_LENGTH];


	while (fgets(buffer, sizeof(buffer), file) != NULL) {
		threads++;
	}

	fclose(file);

	char lines[MAX_LINES][100];

	fopen_s(&file, argv[1], "r");
	while (line_count < MAX_LINES && fgets(lines[line_count], MAX_LEN, file)) {

		lines[line_count][strcspn(lines[line_count], "\n")] = '\0';
		line_count++;
	}
	fclose(file);

	HANDLE thread_handles[MAX_LINES];
	DWORD thread_ids[MAX_LINES];

	int thread_arg = 0;
	int counter = 0;

	while (counter < threads) {

		thread_handles[counter] = CreateThread(
			NULL,
			0,
			WorkerThread,    
			&lines[counter], 
			0,
			&thread_ids[counter]);

		counter++;
	}

	WaitForMultipleObjects(4, thread_handles, TRUE, INFINITE);

	for (int i = 0; i < threads; i++) {
		CloseHandle(thread_handles[i]);
	}

	return 0; 
}