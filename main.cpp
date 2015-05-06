
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tinycthread.h"
#include "es_timer.h"


#ifdef WIN32 // Windows
# include <winsock2.h>
# include <ws2tcpip.h>
#pragma comment (lib, "Ws2_32.lib")
#else // Assume Linux
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <sys/fcntl.h>
# include <netdb.h>
# include <string.h>
# include <stdlib.h>
# include <unistd.h>
# include <errno.h>
#define SOCKET int
#define SOCKET_ERROR ‐1
#define WSAGetLastError() (errno)
#define closesocket(s) close(s)
#define ioctlsocket ioctl
#define WSAEWOULDBLOCK EWOULDBLOCK
// There are other WSAExxxx constants which may also need to be #define'd to the Exxxx versions.
#define Sleep(x) usleep((x)*1000)
#endif

#define max_number 100

int client_ref;
int server_soc;
int global_thread_number;
int global_client_number;
sockaddr client_sockaddr1[max_number];
int client_soc1[max_number];

struct queue{
	thrd_t *client_soc2;
	int front;
	int rear;
};

struct queue q;

struct threadpool_t{
	mtx_t lock;
	cnd_t notify;
	thrd_t *threads;
	threadpool_task_t *queue;
	int thread_count;
	int queue_size;
	int head;
	int head;
	int count;
	int shutdown;
	int started;
};

typedef struct {
	void (*function)(void*);
	void *argument;
} threadpool_task_t;

static void *threadpool_thread(void *threadpool) {
	threadpool_t *pool = (threadpool *) threadpool;
	threadpool_task_t task;
	for (;;) {
		mtx_lock(&(pool.lock));
		while ((pool.count==0)&&(!pool.shutdown)) {
			cnd_wait(&(pool.notify, &(pool.lock)));
		}
		if ((pool->shutdown == immediate_shutdown) || ((pool->shutdown == graceful_shutdown) && (pool->count == 0))) {
            break;
        }
        task.function = pool.queue[pool.head].function;
        task.function = pool.queue[pool.head].argument;
        pool.head += 1;
        pool.head = (pool.head == pool.queue_size) ? 0 : pool.head;
        pool.count -= 1;
        mtx_unlock(&(pool.lock));
        (*(task.function))(task.argument);
	}
	pool.started--;
	mtx_unlock(&(pool.lock));
	thrd_exit(NULL);
	return(NULL);
}

threadpool_t *threadpool_create(int thread_count, int queue_size, int flags) {
	threadpool_t *pool;
	int i;
	if ((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL) {
		goto err;
	}
	pool.thread_count = 0;
	pool.queue_size = queue_size;
	pool.head = pool.tail = pool.count = 0;
	pool.shutdown = pool.started = 0;
	pool.threads = (thrd_t *)malloc(sizeof(thrd_t) * thread_count);
	pool.queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_size);
	mtx_init(&(pool.lock));
	cnd_init(&(pool.notify));
	for (i=0; i<thread_count; i++) {
		thrd_create(&(pool.threads[i]), NULL, threadpool_thread, (void*)pool);
		pool.thread_count++;
		pool.started++;
	}
	return pool;
}

mtx_t gMutex;
cnd_t gCond;

void enqueue(SOCKET soc) {
	q.rear = (q.rear + 1) % max_number;
	if (q.front == q.rear + 1) {
		exit(1);
	} else {
		q.client_soc2[q.rear] = soc;
	}
}

SOCKET dequeue(void) {
	if (q.front == q.rear) {
		exit(1);
	} else {
		q.front = (q.front + 1)%max_number;
		return (q.client_soc2[q.front]);
	}
}

int tcp_recv_send_thread1(void *aArg) {
	int iresult;
	int seq = *(int*)&aArg;
	char *buf = new char[102400];
	char buffer[102400];
	long recv_size = recv(client_soc1[seq], buffer, 8192, 0);;
	if (recv_size < 0) {
		printf("recv() error: %d\n", errno);
		return 0;
	}
	char *test = strtok(buffer, "\n");
	char part[50][50];
	char *temp;
	int i = 0;
	if ((temp = strtok(buffer, " "))) {
		strcpy(part[i++], temp);
		while ((temp = strtok(NULL, " "))) {
			strcpy(part[i++], temp);
		}
	}
	char *path = part[1];
	memmove(path, path + 1, strlen(path));
	if (!*path) {
		path = "index.html";
	}
	char eachline[1000];
	FILE *fileptr = fopen(path, "r");
	if (!fileptr) {
		printf("fopen() error\n");
		fileptr = fopen("404error.html", "r");
	}
	while (fgets(eachline, 1000, fileptr) != NULL) {
		iresult = send(client_soc1[seq], eachline, strlen(eachline), 0);
		if (iresult == -1) {
			printf("send() error: %d\n", errno);
		}
	}
	fclose(fileptr);
	close(client_soc1[seq]);
	return 0;
}

int tcp_recv_send_thread2(void *aArg) {
	int seq;
	while (1) {
		mtx_lock(&gMutex);
		cnd_wait(&gCond, &gMutex);
		global_client_number--;
		SOCKET client_local_soc = dequeue();
		mtx_unlock(&gMutex);
		int iresult;
		char *buf = new char[102400];
		char buffer[102400];
		long recv_size;
		recv_size = recv(client_local_soc, buffer, 8192, 0);
		if (recv_size < 0) {
			printf("recv() error: %d\n", errno);
			return 0;
		}
		char *test = strtok(buffer, "\n");
		char part[50][50];
		char *temp;
		int i = 0;
		if ((temp = strtok(buffer, " "))) {
			strcpy(part[i++], temp);
			while ((temp = strtok(NULL, " "))) {
				strcpy(part[i++], temp);
			}
		}
		char *path = part[1];
		memmove(path, path + 1, strlen(path));
		if (!*path) {
			path = "index.html";
		}
		FILE *fileptr;
		char eachline[1000];

		fileptr = fopen(path, "r");
		if (!fileptr) {
			printf("fopen() error\n");
			fileptr = fopen("404error.html", "r");
		}

		while (fgets(eachline, 1000, fileptr) != NULL) {
			iresult = send(client_local_soc, eachline, strlen(eachline), 0);
			if (iresult == -1) {
				printf("send() error: %d\n", errno);
			}
		}
		fclose(fileptr);
		close(client_local_soc);
	}
	return 0;
}

int tcp_accept_thread(void *aArg) {
	global_thread_number++;
	int seq;
	while (1) {
		mtx_lock(&gMutex);
		int addrlen = sizeof(struct sockaddr);
		client_soc1[client_ref] = accept(server_soc, &client_sockaddr1[client_ref], (socklen_t*)&addrlen);
		if (client_soc1[client_ref] == -1) {
			printf("accept() error: %d\n", errno);
			break;
		}
		seq = client_ref;
		client_ref++;
		mtx_unlock(&gMutex);
		thrd_t t;
		thrd_create(&t, tcp_recv_send_thread1, (void*)seq);
	}
	return 0;
}

int main(int argc, char* argv[]) {
	mtx_init(&gMutex, mtx_plain);
	cnd_init(&gCond);

	char *port = "12345";
	char *mode = "p";  // 'o' => on demand, 'p' => thread pool
	int max_thread_number = atol("5");

	int i;

	q.client_soc2 = new thrd_t[100];
	q.front = -1;
	q.rear = -1;

	int iresult;

	client_ref = 0;
	global_thread_number = 0;
	global_client_number = 0;

	#ifdef WIN32
	WSADATA wsadata;
	iresult = WSAStartup(MAKEWORD(2,2), &wsadata);

	if (iresult != 0) {
		printf("WSAStartup failed. error code: %d.\n", WSAGetLastError());
		getchar();
		return 0;
	}
	#endif

	server_soc = socket(AF_INET, SOCK_STREAM, 0);
	if (server_soc == -1) {
		printf("socket() error:%d\n", errno);
		getchar();
		return 0;
	}

	struct addrinfo hints, *result = NULL, *ptr = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	hints.ai_protocol = IPPROTO_TCP;

	iresult = getaddrinfo("localhost", port, &hints, &result);
	if (iresult != 0) {
		printf("getaddrinfo failed: %d\n", iresult);
		getchar();
		return 0;
	}

	sockaddr tempsockaddr;
	tempsockaddr = *result->ai_addr;
	freeaddrinfo(result);

	iresult = bind(server_soc, &tempsockaddr, sizeof(struct sockaddr_in));
	if (iresult != 0) {
		printf("bind() failed: %d\n", errno);
		getchar();
		return 0;
	}

	iresult = listen(server_soc, 5);
	if (iresult != 0) {
		printf("listen() error: %d\n", errno);
	}

	int seq;
	if (mode == "o") {
		while (1) {
			int addrlen = sizeof(struct sockaddr);
			client_soc1[client_ref] = accept(server_soc, &client_sockaddr1[client_ref], (socklen_t*)&addrlen);
			if (client_soc1[client_ref] == -1) {
				printf("accept() error: %d\n", errno);
				break;
			}
			seq = client_ref;
			client_ref++;
			thrd_t t;
			thrd_create(&t, tcp_recv_send_thread1, (void*)seq);
		}
	} else if (mode == "p") {
		thrd_t *handles = new thrd_t[max_thread_number];
		for (i = 0; i < max_thread_number; i++) {
			thrd_create(&handles[i], tcp_recv_send_thread2, (void*)0);
		}
		while (1) {
			SOCKET tempsoc;
			int addrlen = sizeof(struct sockaddr);
			tempsoc = accept(server_soc, &client_sockaddr1[client_ref], (socklen_t*)&addrlen);
			mtx_lock(&gMutex);
			if (tempsoc == -1) {
				printf("accept() error: %d\n", errno);
				continue;
			}
			enqueue(tempsoc);
			global_client_number++;
			mtx_unlock(&gMutex);
			cnd_signal(&gCond);
		}
	}
	mtx_destroy(&gMutex);
	cnd_destroy(&gCond);
	return 0;
}