// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/child_log_process.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"

namespace chromecast {

constexpr char kChildLogProcess[] = "child_log_process";
constexpr char kChildLogProcessArgs[] = "child_log_process_args";

void ForkAndRunLogProcess(std::string log_process_path,
                          std::string log_process_args) {
  if (log_process_args.size() > 2 &&
      (log_process_args[0] == '\"' && log_process_args.back() == '\"')) {
    log_process_args = log_process_args.substr(1, log_process_args.size() - 2);
  }
  std::vector<std::string> raw_args = base::SplitString(
      log_process_args, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<const char*> argv = {log_process_path.c_str()};
  for (const auto& arg : raw_args) {
    argv.push_back(arg.c_str());
  }
  argv.push_back(nullptr);

  int pipefds[2];
  pid_t cpid;
  if (pipe(pipefds) == -1) {
    fprintf(stderr, "Could not create pipes for logging process: %s\n",
            strerror(errno));
    fflush(stderr);
    return;
  }
  cpid = fork();
  if (cpid == -1) {
    fprintf(stderr, "Could not fork new process for logging process: %s\n",
            strerror(errno));
    fflush(stderr);
    close(pipefds[0]);
    close(pipefds[1]);
    return;
  }

  if (cpid == 0) {
    // child
    close(pipefds[1]);  // writable end
    dup2(pipefds[0], fileno(stdin));
    close(pipefds[0]);
    int ret =
        execv(log_process_path.c_str(), const_cast<char* const*>(&argv[0]));
    if (ret == -1) {
      fprintf(stderr, "Could not start process: %s error: %s\n",
              log_process_path.c_str(), strerror(errno));
      _exit(1);
    }
  } else {
    // parent
    close(pipefds[0]);  // readable end

    int original_stderr = dup(fileno(stderr));
    int original_stdout = dup(fileno(stdout));
    if (dup2(pipefds[1], fileno(stdout)) == -1 ||
        dup2(pipefds[1], fileno(stderr)) == -1) {
      dup2(original_stderr, fileno(stderr));
      dup2(original_stdout, fileno(stdout));
      fprintf(stderr,
              "Failed to map stderr and stdout to writable pipe for logging\n");
      fflush(stderr);
    } else {
      printf("Log client initialized.\n");
      close(original_stderr);
      close(original_stdout);
    }
    close(pipefds[1]);  // stderr now references the pipe, or dup2 failed.
                        // regardless we can close our old fd.
  }
}

void ForkAndRunLogProcessIfSpecified(const int argc, const char* const* argv) {
  base::CommandLine::Init(argc, argv);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kChildLogProcess)) {
    ForkAndRunLogProcess(
        command_line->GetSwitchValueASCII(kChildLogProcess),
        command_line->GetSwitchValueASCII(kChildLogProcessArgs));
  }
}

}  // namespace chromecast
