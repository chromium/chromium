// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MAC_INSTALL_TEST_UTIL_H_
#define CHROME_INSTALLER_MAC_INSTALL_TEST_UTIL_H_

#include <concepts>
#include <iosfwd>
#include <limits>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/environment.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace base {
class CommandLine;
class FilePath;
class Process;
}  // namespace base

namespace installer::mac::test {

// The Chrome update script tests need to distinguish between processes that
// exited normally and processes that were terminated with a signal. Since
// base::Process::WaitForExitWithTimeout doesn't preserve signal details, it
// is reimplemented here as WaitForProcessExit, which reports its results
// via a std::variant type, ProcessStatus.

struct ProcessNotValid {
  std::string error = "";
  friend bool operator==(const ProcessNotValid&,
                         const ProcessNotValid&) = default;
};

struct ProcessStillRunning {
  friend bool operator==(const ProcessStillRunning&,
                         const ProcessStillRunning&) = default;
};

struct ProcessTerminatedWithSignal {
  int signal = 0;
  friend bool operator==(const ProcessTerminatedWithSignal&,
                         const ProcessTerminatedWithSignal&) = default;
};

struct ProcessStoppedWithSignal {
  int signal = 0;
  friend bool operator==(const ProcessStoppedWithSignal&,
                         const ProcessStoppedWithSignal&) = default;
};

struct ProcessExitedWithValue {
  int exit_code = std::numeric_limits<int>::max();
  friend bool operator==(const ProcessExitedWithValue&,
                         const ProcessExitedWithValue&) = default;
};

using ProcessStatus = std::variant<ProcessNotValid,
                                   ProcessStillRunning,
                                   ProcessTerminatedWithSignal,
                                   ProcessStoppedWithSignal,
                                   ProcessExitedWithValue>;

// Allows direct equality comparison between a ProcessStatus and any of its
// alternative types (such as `if (status == ProcessStoppedWithSignal{9})` or
// `ASSERT_EQ(status, ProcessExitedWithValue{0})`).
//
// This is implemented as a template to prevent comparison between instances of
// different alternative types; at least one argument must be a ProcessStatus.
template <typename T>
  requires std::convertible_to<T, ProcessStatus> &&
           (!std::same_as<T, ProcessStatus>)
bool operator==(const ProcessStatus& status, const T& value) {
  const T* ptr = std::get_if<T>(&status);
  return ptr && *ptr == value;
}

std::ostream& operator<<(std::ostream& os, const ProcessNotValid&);
std::ostream& operator<<(std::ostream& os, const ProcessStillRunning&);
std::ostream& operator<<(std::ostream& os,
                         const ProcessTerminatedWithSignal& s);
std::ostream& operator<<(std::ostream& os, const ProcessStoppedWithSignal& s);
std::ostream& operator<<(std::ostream& os, const ProcessExitedWithValue& e);
std::ostream& operator<<(std::ostream& os, const ProcessStatus& status);

// Waits for `process` to exit or change status within `timeout`.
// Uses `waitpid` on `process.Handle()`.
ProcessStatus WaitForProcessExit(const base::Process& process,
                                 base::TimeDelta timeout);

struct ExecutionRecord {
  ProcessStatus status = ProcessNotValid{"uninitialized"};
  base::ProcessId pid = 0;
  std::string combined_output = "";
};

// Run an executable with the provided command line, environment, and working
// directory, killing it if it does not finish within the provided timeout.
// The process is the leader of a new process group. stdout and stderr are
// captured and merged into `.combined_output` in the
// returned `ExecutionRecord`.
//
// If the process times out, the returned `.status` is `ProcessStillRunning{}`
// even though it is killed before this function returns. A `.status` of
// `ProcessTerminatedWithSignal{SIGKILL}` means the process was SIGKILLed by
// something other than this timeout.
//
// If the process cannot be started, `.status` is `ProcessNotValid` with a
// descriptive error and `.pid` is meaningless.
ExecutionRecord RunWithTimeout(const base::CommandLine& cmd,
                               const base::EnvironmentMap& env,
                               const base::FilePath& working_dir,
                               base::TimeDelta timeout);

using Replacements = std::vector<std::pair<std::string, std::string>>;

// Construct a string that is a copy of `s` with all instances of any `first`
// string from `rs` replaced with its corresponding `second` string.
//
// Replacements are processed in order of `rs`.
std::string ReplaceAll(std::string_view s, Replacements rs);

}  // namespace installer::mac::test

#endif  // CHROME_INSTALLER_MAC_INSTALL_TEST_UTIL_H_
