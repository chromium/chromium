// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_CHILD_PROCESS_LOGGER_H_
#define CHROME_CHROME_CLEANER_TEST_CHILD_PROCESS_LOGGER_H_

#include "base/files/scoped_temp_dir.h"
#include "base/process/launch.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace chrome_cleaner {

// Redirects stdin and stdout of a child process to a temp file. Tests that
// spawn children can add the contents of the temp file to the test output.
//
// Note that if the child process sets up ScopedLogging, ERROR and FATAL level
// logs will still be captured by this, but other logs will start going to the
// ScopedLogging log file. test_main.cc sets up ScopedLogging so usually this
// class will capture:
//
// 1. All log lines from before the ScopedLogging constructor.
// 2. ERROR and FATAL log lines from after that.
// 3. stdout and stderr output that doesn't go through the logging system.
// 4. Stack traces from any crashes.
//
// This should be all that's needed to diagnose errors in tests.
class ChildProcessLogger {
 public:
  ChildProcessLogger();
  ~ChildProcessLogger();

  // Creates a temp file for child processes to log to. Logs an error and
  // returns false on failure.
  bool Initialize();

  // Updates |options| to direct the child stdout and stderr to the temp file.
  // For use with base::LaunchProcess and base::SpawnMultiProcessTestChild.
  void UpdateLaunchOptions(base::LaunchOptions* options) const;

  // Updates |policy| to direct the child stdout and stderr to the temp file.
  // For use with sandbox::BrokerServices::SpawnTarget.
  void UpdateSandboxPolicy(sandbox::TargetPolicy* policy) const;

  // Writes every line in the temp file using LOG(ERROR) so that all lines are
  // captured in the test suite output. The class-level comment above describes
  // which log lines from the child will be captured.
  void DumpLogs() const;

 private:
  ChildProcessLogger(const ChildProcessLogger& other) = delete;
  ChildProcessLogger& operator=(const ChildProcessLogger& other) = delete;

  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file_name_;
  base::win::ScopedHandle child_stdout_handle_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_CHILD_PROCESS_LOGGER_H_
