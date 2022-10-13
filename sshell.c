// sshell.c
// Colton Perazzo and András Necz 
// ECS 150, University of California, Davis

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CMDLINE_MAX 512
#define ARGS_MAX 17
#define SPACE_CHAR 32
#define PIPE_MAX 3
#define COMMANDS_MAX 4

#define OUTPUT_KEY 0
#define INPUT_KEY 1

struct command_struct {
        int cmd_id;
        int total_cmds;

        char *full_cmd;
        char *program;
        char *args[ARGS_MAX];

        char *output_file; 
        bool has_output_file;

        char *input_file;
        bool has_input_file;

        int number_of_args;
};

struct stack {
        int top_stack;
};

char *get_program_name(char *cmd) {
        char *temp_prog = calloc(strlen(cmd)+1, sizeof(char));
        strcpy(temp_prog, cmd);
        char *prog = strtok(temp_prog, " ");
        return prog;
}

bool check_if_too_many_pipes(char *cmd) {
        int pipes = 0;
        int i;
        for (i = 0; i < strlen(cmd); i++) {
                if (cmd[i] == '|') {
                        pipes++;
                }
        }
        if (pipes > PIPE_MAX) {
                return true;
        }
        return false;
}

bool check_if_too_many_args(struct command_struct cmd_struct) {
        if (cmd_struct.number_of_args > ARGS_MAX - 1) {
                return true;
        }
        return false;
}

bool check_if_invalid_command(struct command_struct cmd_struct) {
        if (cmd_struct.program[0] == '>' || cmd_struct.program[0] == '|') {
                return true;
        } else {
                if (cmd_struct.full_cmd[strlen(cmd_struct.full_cmd)-1] == '|') {
                        return true;
                }
        }
        return false;
}

bool check_if_missing_inputoutput_file(struct command_struct cmd_struct, int mode) {
        bool missing_file = false;
        switch (mode) {
                case OUTPUT_KEY:
                        if (cmd_struct.has_output_file) {
                                if (cmd_struct.full_cmd[strlen(cmd_struct.full_cmd)-1] == '>') {
                                        missing_file = true;
                                } else {
                                        if(cmd_struct.output_file == NULL) {missing_file = true;} 
                                }
                        }
                default:
                        if (cmd_struct.has_input_file) {
                                if (cmd_struct.full_cmd[strlen(cmd_struct.full_cmd)-1] == '<') {
                                        missing_file = true;
                                } else {
                                        if (cmd_struct.input_file == NULL) {missing_file = true;}
                                }
                        }
        }
        return missing_file;
}

bool check_if_inputouput_file_is_null(struct command_struct cmd_struct, int mode) {
        bool null_file = false;
        switch (mode) {
                case OUTPUT_KEY:
                       if (cmd_struct.has_output_file) {
                                char *includes_redirect = strchr(cmd_struct.output_file, '/');
                                if (includes_redirect) {
                                        null_file = true;
                                }
                        } 
                default:
                        if (cmd_struct.has_input_file) {
                                if(access(cmd_struct.input_file, F_OK) != 0) {
                                        null_file = true;
                                }
                        }
                        
        }
        return null_file;
}

bool check_if_piping_inputpout_is_mislocated(struct command_struct cmd_struct, int mode) {
        bool mislocated = false;
        switch (mode) {
                case OUTPUT_KEY:
                        if (cmd_struct.has_output_file) {
                                if (cmd_struct.cmd_id != cmd_struct.total_cmds) {
                                        mislocated = true;
                                }
                        }
                default:
                        if (cmd_struct.has_input_file) {
                                if (cmd_struct.cmd_id != 0) {
                                        mislocated = true;
                                }
                        }

        }
        return mislocated;
}

bool sanity_check_cmd(struct command_struct cmd_struct) {
        bool can_cmd_run = true;
        if (check_if_invalid_command(cmd_struct)) {
                fprintf(stderr, "Error: missing command\n");
                return false;
        } else {
                if (cmd_struct.has_output_file) {
                        if (check_if_missing_inputoutput_file(cmd_struct, OUTPUT_KEY)) { 
                                fprintf(stderr, "Error: no output file\n");
                                return false;
                        }
                        if(check_if_piping_inputpout_is_mislocated(cmd_struct, OUTPUT_KEY)) {
                                fprintf(stderr, "Error: mislocated output redirection\n");
                                return false;
                        }
                        if (check_if_inputouput_file_is_null(cmd_struct, OUTPUT_KEY)) {
                                fprintf(stderr, "Error: cannot open output file\n");
                                return false;
                        }
                }
                if (cmd_struct.has_input_file) {
                        if (check_if_missing_inputoutput_file(cmd_struct, INPUT_KEY)) {
                                fprintf(stderr, "Error: no input file\n");
                                return false;
                        }
                        if (check_if_piping_inputpout_is_mislocated(cmd_struct, INPUT_KEY)) {
                                fprintf(stderr, "Error: mislocated input redirection\n");
                                return false;
                        }
                        if (check_if_inputouput_file_is_null(cmd_struct, INPUT_KEY)) {
                                fprintf(stderr, "Error: cannot open input file\n");
                                return false;
                        }
                }
                if (check_if_too_many_args(cmd_struct)) {
                        fprintf(stderr, "Error: too many process arguments\n");
                        return false;
                }

        }
        return true;
}

struct command_struct parse_single_cmd(char *cmd, int num, int total) {
        char *prog = get_program_name(cmd);
        struct command_struct new_cmd;
        new_cmd.cmd_id = num;
        new_cmd.total_cmds = total;
        new_cmd.full_cmd = cmd;
        new_cmd.program = prog;
        new_cmd.args[0] = prog;
        new_cmd.number_of_args = 1;
        new_cmd.has_output_file = false;
        new_cmd.has_input_file = false;
        char* has_output_file = strchr(cmd, '>');
        if (has_output_file) {
                new_cmd.has_output_file = true;
                char *temp_cmd = calloc(strlen(cmd)+1, sizeof(char));
                strcpy(temp_cmd, cmd);
                char *split_at_output = strchr(temp_cmd, '>')+1;
                new_cmd.output_file = strtok(split_at_output, " ");
        }
        char *has_input_file = strchr(cmd, '<');
        if (has_input_file) {
                new_cmd.has_input_file = true;
                char *temp_cmd = calloc(strlen(cmd)+1, sizeof(char));
                strcpy(temp_cmd, cmd);
                char *split_at_output = strchr(temp_cmd, '<')+1;
                new_cmd.input_file = strtok(split_at_output, " ");
        }
        if (strlen(prog) == strlen(cmd)) { new_cmd.args[1] = NULL; }
        else {
                char *temp_cmd = calloc(strlen(cmd)+1, sizeof(char));
                strcpy(temp_cmd, cmd);
                char *cmd_arg = strtok(temp_cmd, " ");
                while (cmd_arg != NULL) {
                        bool can_add_arg = true;
                        if (!strcmp(cmd_arg, prog)) {
                                can_add_arg = false;
                        } else if (!strcmp(cmd_arg, ">")) {
                                can_add_arg = false;
                        } else if (!strcmp(cmd_arg, "|")) {
                                can_add_arg = false;
                        } else {
                                if (new_cmd.has_output_file) {
                                        if (new_cmd.output_file != NULL) {
                                                if (strcmp(cmd_arg, new_cmd.output_file) == 0) {
                                                        can_add_arg = false;
                                                } 
                                        }
                                }
                        }
                        if (can_add_arg) {
                                if (new_cmd.number_of_args > (ARGS_MAX - 1)) {
                                        break;
                                }
                                char* has_output_file = strchr(cmd_arg, '>');
                                if (has_output_file) {
                                        char *temp_cmd_output = calloc(strlen(cmd_arg)+1, sizeof(char));
                                        strcpy(temp_cmd_output, cmd_arg);
                                        char *cmd_arg_output = strtok(temp_cmd_output, ">");
                                        if (strcmp(cmd_arg_output, new_cmd.output_file)) {
                                                new_cmd.args[new_cmd.number_of_args] = cmd_arg_output;
                                                new_cmd.number_of_args = new_cmd.number_of_args + 1;
                                        }
                                } else {
                                        new_cmd.args[new_cmd.number_of_args] = cmd_arg;
                                        new_cmd.number_of_args = new_cmd.number_of_args + 1;
                                }
                        }
                        cmd_arg = strtok(NULL, " ");
                }
                new_cmd.args[new_cmd.number_of_args] = NULL;
        }
        return new_cmd;
}

int file_name_errors(int err) {

}

void pwd_execution() {
        //gotta deal with error handling
        char buf[256];
        //printf("%s\n", getcwd(buf, sizeof(buf)));
        /*
        if (chdir("/tmp") != 0)
                perror("chdir() error()");
        else {
                if (getcwd(cwd, sizeof(cwd)) == NULL)
                        perror("getcwd() error");
                else
                        printf("current working directory is: %s\n", cwd);
        }
        */
}

void cd_execution(const char *filename) {
        //int ret = chdir(filename[1]);
	//fprintf(stderr, "+ completed  '%s': [%d]\n", filename, WEXITSTATUS(ret));
	//int error_code = errno;
	
	
	//switch (error_code) {
		//case 2:
			//fprintf(stderr, "Error: command not found\n");  
	//}
	//exit(1);
}

int main(void) {
        char cmd[CMDLINE_MAX];
        while (1) {
                char *new_line;
                printf("sshell@ucd$ ");
                fflush(stdout);
                fgets(cmd, CMDLINE_MAX, stdin);
                if (!isatty(STDIN_FILENO)) {
                        printf("%s", cmd);
                        fflush(stdout);
                }
                new_line = strchr(cmd, '\n');
                if (new_line)
                        *new_line = '\0';
                if (!strcmp(cmd, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
                        break;
                }
                //cd in else, since it has 2 arguments
                /*else if (!strcmp(cmd, "cd")) { 
                        const char dot[256] = "..";
                        //printf("dot = %s\n", dot);
                        cd_execution(dot);
                }*/
                else if (!strcmp(cmd, "pwd")) {
                        pwd_execution();
                } 
                else if (!strcmp(cmd, "pushd")) {

                } 
                else if (!strcmp(cmd, "popd")) {

                } 
                else if (!strcmp(cmd, "dirs")) {

                } 
                else {
                        char* has_multiple_commands = strchr(cmd, '|');
                        if (has_multiple_commands) {
                                if (cmd[0] != '|') {
                                        if (!check_if_too_many_pipes(cmd)) {
                                                char *temp_cmd = calloc(strlen(cmd)+1, sizeof(char));
                                                strcpy(temp_cmd, cmd);
                                                char *cmd_arg = strtok(temp_cmd, "|");
                                                struct command_struct commands[COMMANDS_MAX];
                                                char *command_strings[COMMANDS_MAX];
                                                int number_of_commands = 0;
                                                while (cmd_arg != NULL) {
                                                        if (cmd_arg[0] == SPACE_CHAR) {cmd_arg++;}
                                                        command_strings[number_of_commands] = cmd_arg;
                                                        number_of_commands++;
                                                        cmd_arg = strtok(NULL, "|");
                                                }
                                                int cmd_s;
                                                bool invalid_command = false;
                                                for(cmd_s = 0; cmd_s < number_of_commands; cmd_s++) {
                                                        struct command_struct cmd_to_run = parse_single_cmd(command_strings[cmd_s], cmd_s, number_of_commands-1);
                                                        commands[cmd_s] = cmd_to_run;
                                                        bool can_run = sanity_check_cmd(cmd_to_run);
                                                        if (!can_run) {
                                                                invalid_command = true;
                                                        } else {
                                                                if (cmd[strlen(cmd)-1] == '|') {
                                                                        invalid_command = true;
                                                                        fprintf(stderr, "Error: missing command\n");
                                                                }
                                                        }
                                                }
                                                if (!invalid_command) {
                                                        size_t cmd_size = number_of_commands;
                                                        pid_t *shared_return_values = mmap(NULL, cmd_size,
                                                                PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED,
                                                                -1, 0);
                                                        pid_t pid;
                                                        pid = fork();
                                                        if (pid > 0) {
                                                                int return_value;
                                                                waitpid(pid, &return_value, 0);
                                                                if (number_of_commands > 2) {
                                                                        fprintf(stderr, "+ completed '%s' [%d][%d][%d]\n", cmd, WEXITSTATUS(shared_return_values[2]), WEXITSTATUS(shared_return_values[1]), WEXITSTATUS(return_value));
                                                                } else { fprintf(stderr, "+ completed '%s' [%d][%d]\n", cmd, WEXITSTATUS(shared_return_values[1]), WEXITSTATUS(return_value));}
                                                        } else if (pid == 0) {
                                                                int cmd_n, in_pipe;
                                                                int fd[2];
                                                                in_pipe = STDIN_FILENO;
                                                                for (cmd_n = 0; cmd_n < number_of_commands - 1; cmd_n++) {
                                                                        pipe(fd);
                                                                        pid_t child_pid;
                                                                        child_pid = fork();
                                                                        if (child_pid > 0) {
                                                                                int return_value;
                                                                                waitpid(child_pid, &return_value, 0);
                                                                                shared_return_values[cmd_n] = return_value;
                                                                        } else if (child_pid == 0) {
                                                                                if (in_pipe != STDIN_FILENO) {
                                                                                        dup2(in_pipe, STDIN_FILENO);
                                                                                        close(in_pipe);
                                                                                }
                                                                                dup2(fd[1], STDOUT_FILENO);
                                                                                close(fd[1]);
                                                                                execvp(commands[cmd_n].program, commands[cmd_n].args);
                                                                                exit(1); 
                                                                        }
                                                                        close(in_pipe);
                                                                        close(fd[1]);
                                                                        in_pipe = fd[0];
                                                                }
                                                                if (in_pipe != STDIN_FILENO) {
                                                                        dup2(in_pipe, STDIN_FILENO);
                                                                        close(in_pipe);
                                                                }
                                                                if (commands[cmd_n].has_output_file) {
                                                                        int fd_write;
                                                                        fd_write = open(commands[cmd_n].output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                                                                        dup2(fd_write, STDOUT_FILENO);
                                                                        close(fd_write);
                                                                } else { 
                                                                        int stdout = dup(STDOUT_FILENO);
                                                                        dup2(stdout, STDOUT_FILENO);
                                                                }
                                                                execvp(commands[cmd_n].program, commands[cmd_n].args);
                                                                exit(1);
                                                        }
                                                }
                                        } else {
                                                fprintf(stderr, "Error: too many pipes\n");  
                                        }
                                } else {
                                        fprintf(stderr, "Error: missing command\n");
                                } 
                        } else {
                                struct command_struct cmd_to_run = parse_single_cmd(cmd, 0, 0);
                                bool can_run = sanity_check_cmd(cmd_to_run);
                                if (can_run) {
                                        if (!strcmp(cmd_to_run.program, "cd")) {
                                                printf("about to execute cd\n"); 
						cd_execution(cmd);
                                                continue;
                                        }
                                        pid_t pid;
                                        pid = fork();
                                        //printf("pid: %i\n",pid);
                                        if (pid > 0) { // parent
                                                //printf("%i\n", pid);
                                                int return_value;
                                                waitpid(pid, &return_value, 0);
                                                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, WEXITSTATUS(return_value));
                                        } else if (pid == 0) { // child
                                                if (cmd_to_run.has_output_file) {
                                                        int fd;
                                                        fd = open(cmd_to_run.output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                                                        dup2(fd, STDOUT_FILENO);
                                                        close(fd);
                                                } else { 
                                                        int stdout = dup(STDOUT_FILENO);
                                                        dup2(stdout, STDOUT_FILENO);
                                                }

                                                execvp(cmd_to_run.program, cmd_to_run.args);
                                                int error_code = errno;
                                                switch (error_code) {
                                                        case 2:
                                                        fprintf(stderr, "Error: command not found\n");  
                                                }
                                                exit(1);
                                        } else { printf("Error: fork cannot be created\n"); }
                                }
                        }
                }
        }
        return EXIT_SUCCESS;
}
