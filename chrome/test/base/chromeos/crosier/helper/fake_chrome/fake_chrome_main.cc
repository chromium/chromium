// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"

// Fake chrome is a stub used by test_sudo_helper.py script to start
// session_manager daemon. It is pass to the daemon via "--chorme-command"
// switch. Session manager daemon manages its lifetime as if it is
// chrome. It is needed because chromeos_integration_tests test cases acts as
// chrome but they run in processes managed by the test launcher and could not
// be managed by the session_manager daemon.
int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);

  logging::InitLogging(logging::LoggingSettings());

  LOG(INFO) << "Fake chrome starting.";

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::RunLoop run_loop;
  run_loop.Run();

  LOG(INFO) << "Fake chrome exiting.";

  return 0;
}
