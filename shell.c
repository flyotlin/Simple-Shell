#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

const int MAX_ARG_SIZE = 10;
const int MAX_PATH_SIZE = 250;

void shell_loop();
void show_prompt();
char *read_line();
char **parse_line(char *, int *);
void execute_command(char **, int *);

int main() 
{
        shell_loop();
        return 0;
}

void shell_loop() 
{
        while (1) {
                char *line;
                char **tokens_list;
                show_prompt();
                line = read_line();

                size_t argc = 0;
                tokens_list = parse_line(line, &argc);

                execute_command(tokens_list, &argc);

                free(line);
                free(tokens_list);
        }
}

void show_prompt() 
{
        char cwd[MAX_PATH_SIZE];
        getcwd(cwd, sizeof(cwd));
        printf("flyotlin:%s$ ", cwd);
}

char *read_line() 
{
        size_t DEFAULT_SIZE = 5;
        size_t position = 0;
        char* line = malloc(DEFAULT_SIZE * sizeof(char));

        int ch = getchar();
        while (ch != 10) {
                // 還能塞入line裡面
                if (position < DEFAULT_SIZE) {
                        line[position++] = ch;
                } else {
                        DEFAULT_SIZE *= 2;
                        line = realloc(line, DEFAULT_SIZE * sizeof(char));
                        line[position++] = ch;
                }
                ch = getchar();
        }
        line[position++] = '\0';    // 應該要check有沒有out of DEFAULT_SIZE
        return line;
}

char **parse_line(char *line, int *argc) 
{
        char *SHELL_DELIM = " ";      // the Delimiter of the parsed string
        char *token;                  // holding the parsed single string
        
        size_t TOKENS_LIST_SIZE = 5; 
        char **tokens_list = malloc(TOKENS_LIST_SIZE * sizeof(char *));     // store char*(8byte) on heap
        size_t position = 0;

        token = strtok(line, SHELL_DELIM);     // why required char *, pass str, not &str

        while (token != NULL) {
                if (position < TOKENS_LIST_SIZE-1) {
                        tokens_list[position++] = token;
                } else {
                        TOKENS_LIST_SIZE *= 2;
                        tokens_list = realloc(tokens_list, TOKENS_LIST_SIZE * sizeof(char *));
                        tokens_list[position++] = token;
                }
                token = strtok(NULL, SHELL_DELIM);
                *argc += 1;        // add the number count of argc
        }
        tokens_list[position++] = '\0';
        return tokens_list;
}

void execute_command(char **argv, int *argc)
{
        // handling CHANGE DIRECTORY
        if (strcmp(argv[0], "cd") == 0) {
                chdir(argv[1]);
                return;
        }
        int isLast = 0;         // whether is the last command

        int fd[2];      // pipe file descriptor
        int fd_original_stdout = dup(STDOUT_FILENO);  
        int fd_original_stdin = dup(STDIN_FILENO);
        
        // originally STDIN, later save the result of the child process
        int fd_result = STDIN_FILENO;

        if (pipe(fd) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
        }
        size_t pid = fork();
        int exitStatus;
        if (pid == 0) {
                // prepare 
                char *arg[MAX_ARG_SIZE];
                int argIdx = 0, j = 0;
                while (1) {
                        if (j >= *(argc)) {
                                isLast = 1;
                                break;
                        }
                        if (strcmp(argv[j], "|") == 0)
                                break;
                        if (strcmp(argv[j], ">") == 0)
                                break;
                        if (strcmp(argv[j], "<") == 0) {
                                int fd_file = open(argv[j + 1], O_RDWR, 0664);
                                fd_result = dup(fd_file);
                                j += 2;
                        } else {
                                arg[argIdx] = argv[j];
                                argIdx += 1;
                                j += 1;
                        }
                }
                arg[argIdx] = NULL;

                close(fd[0]);
                dup2(fd_result, STDIN_FILENO);
                if (isLast == 0) {
                        dup2(fd[1], STDOUT_FILENO);
                } else {
                        dup2(fd_original_stdout, STDOUT_FILENO);
                }
                execvp(arg[0], arg);
                dup2(fd_original_stdout, STDOUT_FILENO);
                close(fd[1]);
                exitStatus = EXIT_SUCCESS;
        } else if (pid > 0) {
                wait(&exitStatus);
                fd_result = fd[0];
                close(fd[1]);
        }

        // other process
        for (int i = 0; i < (*argc); i++) {
                if (strcmp(argv[i], "|") == 0) {
                        // 不能在fork後才pipe
                        if (pipe(fd) < 0) {
                                perror("pipe");
                                exit(EXIT_FAILURE);
                        }
                        size_t pid = fork();
                        int exitStatus;
                        
                        if (pid == 0) {
                                char *arg[MAX_ARG_SIZE];
                                int argIdx = 0, j = i + 1;
                                while (1) {
                                        if (j >= *(argc)) {
                                                isLast = 1;
                                                break;
                                        }
                                        if (strcmp(argv[j], "|") == 0) {
                                                break;
                                        }
                                        if (strcmp(argv[j], ">") == 0)
                                                break;
                                        if (strcmp(argv[j], "<") == 0) {
                                                int fd_file = open(argv[j + 1], O_RDWR, 0664);
                                                fd_result = dup(fd_file);
                                                j += 2;
                                        } else {
                                                arg[argIdx] = argv[j];
                                                argIdx += 1;
                                                j += 1;
                                        }
                                }
                                arg[argIdx] = NULL;

                                close(fd[0]);
                                dup2(fd_result, STDIN_FILENO);
                                if (isLast == 0) {
                                        dup2(fd[1], STDOUT_FILENO);
                                } else {
                                        dup2(fd_original_stdout, STDOUT_FILENO);
                                }
                                execvp(arg[0], arg);
                                dup2(fd_original_stdout, STDOUT_FILENO);
                                dup2(fd_original_stdin, STDIN_FILENO);
                                close(fd[1]);
                                exitStatus = EXIT_SUCCESS;
                        } else if (pid > 0) {
                                wait(&exitStatus);
                                fd_result = fd[0];
                                close(fd[1]);
                        }
                }
                else if (strcmp(argv[i], ">") == 0) {
                        if (pipe(fd) < 0) {
                                perror("pipe");
                                exit(EXIT_FAILURE);
                        }
                        size_t pid = fork();
                        int exitStatus;

                        if (pid == 0) {
                                int fd_newfile = open(argv[i+1], O_CREAT | O_RDWR, 0664);
                                char *buf = malloc(sizeof(char) * 10000);
                                int num_read = read(fd_result, buf, O_NONBLOCK);
                                write(fd_newfile, buf, num_read);

                                dup2(fd[1], STDOUT_FILENO);
                                char *arg[3] = {"cat", NULL, NULL};
                                arg[1] = argv[i+1];
                                execvp(arg[0], arg);

                                dup2(fd_original_stdout, STDOUT_FILENO);
                                close(fd[1]);

                                free(buf);
                                exitStatus = EXIT_SUCCESS;
                        } else if (pid > 0) {
                                wait(&exitStatus);
                                fd_result = fd[0];
                                close(fd[1]);
                        }
                }
        }      
}

/*
 * Test Data:
 * $ ls -al | head -3 < test.data | nl > output
 */