// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "components/policy/test_support/fake_dmserver.h"

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::CommandLine::Init(argc, argv);

  std::string policy_blob_path, client_state_path;
  absl::optional<std::string> log_path;
  base::ScopedFD startup_pipe;
  int min_log_level;
  bool log_to_console;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  fakedms::ParseFlags(*command_line, policy_blob_path, client_state_path,
                      log_path, startup_pipe, log_to_console, min_log_level);
  fakedms::InitLogging(log_path, log_to_console, min_log_level);

  base::RunLoop run_loop;
  fakedms::FakeDMServer policy_test_server(policy_blob_path, client_state_path,
                                           run_loop.QuitClosure());
  if (!policy_test_server.Start()) {
    return 1;
  }
  if (startup_pipe.is_valid()) {
    if (!policy_test_server.WriteURLToPipe(std::move(startup_pipe))) {
      return 1;
    }
  }
  run_loop.Run();
  return 0;
}
