#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#include <sys/queue.h>
#include <sys/types.h>


#include <pthread.h>

static LIST_HEAD(listhead, entry) head;

#define THREAD_NUM 10

struct listhead *headp = NULL;
int num_entries = 0;

FILE* fps[THREAD_NUM];

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;  // lock을 초기화


struct entry {
	char name[BUFSIZ];
	int frequency;
	LIST_ENTRY(entry) entries;
};


void* word_count(void* tid);

int main(int argc, char** argv)
{
	if (argc != 2) {
		fprintf(stderr, "%s: not enough input\n", argv[0]);
		exit(1);
	}

  int i;
  for(i = 0; i < THREAD_NUM; i ++) {
    fps[i] = fopen(argv[1], "r");
  }
	

	LIST_INIT(&head);


  pthread_t threads[THREAD_NUM];
  int thr_id[THREAD_NUM];
  for(i = 0; i < THREAD_NUM; i ++) {
    thr_id[i] = i;
  }

  int status;

  for(i = 0; i < THREAD_NUM; i ++) {
    if(pthread_create(&threads[i], NULL, word_count, (void *) &thr_id[i]) < 0) {
      fprintf(stderr, "err\n");
      exit(0);
    }
  }
  
  for(i = 0; i < THREAD_NUM; i ++) {
    pthread_join(threads[i], (void **) &status);  
  }
  
  // Print the counting result very very slow way.
  int max_frequency = 0;

  for (struct entry* np = head.lh_first; np != NULL; np = np->entries.le_next) {
    if (max_frequency < np->frequency) {
      max_frequency = np->frequency;
    }
  }

  for (int it = max_frequency; it > 0; --it) {
    for (struct entry* np = head.lh_first; np != NULL; np = np->entries.le_next) {
      if (np->frequency == it) {
        printf("%s %d\n", np->name, np->frequency);
      }
    }
  }

  // Release
  while (head.lh_first != NULL) {
    LIST_REMOVE(head.lh_first, entries);
  }

  for(i = 0; i < THREAD_NUM; i ++) {
    fclose(fps[i]);
  }

  return 0;
}



void* word_count(void* tid)
{
  int i = 0;
  int t_num = *(int *) tid;
  if(t_num != 0) usleep(100000);

  while (1) {
    // TODO i % thread개수 가 tid면 실행 아니면 리턴!
    char buf[4096];   

    if(fgets(buf, 4096, fps[t_num]) == NULL) {
      // 끝까지 가면 리턴
      // fprintf(stderr, "done %d\n", t_num);
      return 0;
    }

    if(i % THREAD_NUM != t_num) {
      i++;
      continue;
    }


    // Preprocess: change all non-alnums into white-space,
    //             alnums to lower-case.
    int line_length = strlen(buf);

    // 특수기호는 공백으로 변경
    for (int it = 0; it < line_length; ++it) {
      if (!isalnum(buf[it])) {
        buf[it] = ' ';
      } else {
        buf[it] = tolower(buf[it]);
      }
    }
    pthread_mutex_lock(&lock);

    // Tokenization
    const char* WHITE_SPACE =" \t\n";
    char* tok = strtok(buf, WHITE_SPACE);

    if (tok == NULL || strcmp(tok, "") == 0) {
      // fprintf(stderr, "%d out\n", t_num);
      i++;
      pthread_mutex_unlock(&lock);
      continue;
    }

    do {
      if (num_entries == 0) {
        struct entry* e = malloc(sizeof(struct entry));

        strncpy(e->name, tok, strlen(tok));
        e->frequency = 1;

        LIST_INSERT_HEAD(&head, e, entries);
        num_entries++;

        continue;
      } else if (num_entries == 1) {
        int cmp = strcmp(tok, head.lh_first->name);

        if (cmp == 0) {
          head.lh_first->frequency++;
        } else if (cmp > 0) {
          struct entry* e = malloc(sizeof(struct entry));

          strncpy(e->name, tok, strlen(tok));
          e->frequency = 1;


          LIST_INSERT_AFTER(head.lh_first, e, entries);
          num_entries++;
        } else if (cmp < 0) {
          struct entry* e = malloc(sizeof(struct entry));

          strncpy(e->name, tok, strlen(tok));
          e->frequency = 1;

          LIST_INSERT_BEFORE(head.lh_first, e, entries);
          num_entries++;
        }

        continue;
      }

      // Reduce: actual word-counting
      struct entry* np = head.lh_first;
      struct entry* final_np = NULL;

      int last_cmp = strcmp(tok, np->name);

      if (last_cmp < 0) {
        struct entry* e = malloc(sizeof(struct entry));

        strncpy(e->name, tok, strlen(tok));
        e->frequency = 1;

        LIST_INSERT_HEAD(&head, e, entries);
        num_entries++;
    
        continue;

      } else if (last_cmp == 0) {
        np->frequency++;

        continue;
      }

      for (np = np->entries.le_next; np != NULL; np = np->entries.le_next) {
        int cmp = strcmp(tok, np->name);

        if (cmp == 0) {
          np->frequency++;

          break;
        } else if (last_cmp * cmp < 0) { // sign-crossing occurred
          struct entry* e = malloc(sizeof(struct entry));

          strncpy(e->name, tok, strlen(tok));
          e->frequency = 1;

          LIST_INSERT_BEFORE(np, e, entries);
          num_entries++;

          break;
        }

        if (np->entries.le_next == NULL) {
          final_np = np;
        } else {
          last_cmp = cmp;
        }
      }

      if (!np && final_np) {
        struct entry* e = malloc(sizeof(struct entry));

        strncpy(e->name, tok, strlen(tok));
        e->frequency = 1;

        LIST_INSERT_AFTER(final_np, e, entries);
        num_entries++;
      }
    } while (tok = strtok(NULL, WHITE_SPACE));

    i++;
    // fprintf(stderr, "%d out\n", t_num);
    pthread_mutex_unlock(&lock);
  }

}
