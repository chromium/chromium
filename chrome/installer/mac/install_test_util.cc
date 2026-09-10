// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mac/install_test_util.h"

#include <poll.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <ostream>
#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/safe_strerror.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer::mac::test {

std::ostream& operator<<(std::ostream& os, const ProcessNotValid& e) {
  return os << "ProcessNotValid(error=" << e.error << ")";
}

std::ostream& operator<<(std::ostream& os, const ProcessStillRunning&) {
  return os << "ProcessStillRunning";
}

std::ostream& operator<<(std::ostream& os,
                         const ProcessTerminatedWithSignal& s) {
  return os << "ProcessTerminatedWithSignal(signal=" << s.signal << ")";
}

std::ostream& operator<<(std::ostream& os, const ProcessStoppedWithSignal& s) {
  return os << "ProcessStoppedWithSignal(signal=" << s.signal << ")";
}

std::ostream& operator<<(std::ostream& os, const ProcessExitedWithValue& e) {
  return os << "ProcessExitedWithValue(exit_code=" << e.exit_code << ")";
}

std::ostream& operator<<(std::ostream& os, const ProcessStatus& status) {
  std::visit([&os](const auto& v) { os << v; }, status);
  return os;
}

// Based on base/process/process_posix.cc (WaitpidWithTimeout and
// Process::WaitForExitWithTimeoutImpl), reimplemented here because the
// base::Process version loses details about signals.
ProcessStatus WaitForProcessExit(const base::Process& process,
                                 base::TimeDelta timeout) {
  if (!process.IsValid()) {
    return ProcessNotValid{"WaitForProcessExit: !process.IsValid()"};
  }

  int status = 0;
  pid_t ret_pid = 0;

  if (timeout == base::TimeDelta::Max()) {
    ret_pid = HANDLE_EINTR(waitpid(process.Handle(), &status, WUNTRACED));
  } else {
    ret_pid =
        HANDLE_EINTR(waitpid(process.Handle(), &status, WNOHANG | WUNTRACED));
    static constexpr int64_t kMaxSleepInMicroseconds = 1 << 18;  // ~256 ms.
    int64_t sleep_usecs = 1 << 10;                               // ~1 ms.
    int double_sleep_time = 0;

    const base::LiveTicks wakeup_time = base::LiveTicks::Now() + timeout;
    while (ret_pid == 0) {
      const base::LiveTicks now = base::LiveTicks::Now();
      if (now > wakeup_time) {
        break;
      }

      const int64_t sleep_time_usecs =
          std::min((wakeup_time - now).InMicroseconds(), sleep_usecs);
      struct timespec ts = {.tv_sec = sleep_time_usecs / 1000000,
                            .tv_nsec = sleep_time_usecs * 1000};
      nanosleep(&ts, nullptr);
      ret_pid =
          HANDLE_EINTR(waitpid(process.Handle(), &status, WNOHANG | WUNTRACED));

      if ((sleep_usecs < kMaxSleepInMicroseconds) &&
          (double_sleep_time++ % 4 == 0)) {
        sleep_usecs = base::checked_cast<uint32_t>(
            std::min(sleep_usecs * 2, kMaxSleepInMicroseconds));
      }
    }
  }

  if (ret_pid <= 0) {
    return ProcessStillRunning{};
  }
  if (WIFEXITED(status)) {
    return ProcessExitedWithValue{WEXITSTATUS(status)};
  }
  if (WIFSIGNALED(status)) {
    return ProcessTerminatedWithSignal{WTERMSIG(status)};
  }
  if (WIFSTOPPED(status)) {
    return ProcessStoppedWithSignal{WSTOPSIG(status)};
  }
  NOTREACHED() << "No identifiable status from waitpid";
}

ExecutionRecord RunWithTimeout(const base::CommandLine& cmd,
                               const base::EnvironmentMap& env,
                               const base::FilePath& working_dir,
                               base::TimeDelta timeout) {
  base::ScopedFD read_fd, write_fd;
  {
    int pipefds[2] = {};
    if (pipe(pipefds) != 0) {
      return {.status = ProcessNotValid(
                  base::StrCat({"RunWithTimeout: could not create pipe: ",
                                base::safe_strerror(errno)}))};
    }
    read_fd.reset(pipefds[0]);
    write_fd.reset(pipefds[1]);
  }

  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(write_fd.get(), STDOUT_FILENO);
  options.fds_to_remap.emplace_back(write_fd.get(), STDERR_FILENO);
  options.current_directory = working_dir;
  options.clear_environment = true;
  options.environment = env;
  options.new_process_group = true;
  const base::Process proc = base::LaunchProcess(cmd, options);
  if (!proc.IsValid()) {
    return {.status = ProcessNotValid("RunWithTimeout: LaunchProcess failed")};
  }
  write_fd.reset();

  const base::Time deadline = base::Time::Now() + timeout;

  static constexpr size_t kBufferSize = 1024;
  std::string output;
  base::CheckedNumeric<size_t> total_bytes_read = 0;
  ssize_t read_this_pass = 0;
  do {
    struct pollfd fds[1] = {{.fd = read_fd.get(), .events = POLLIN}};
    int timeout_remaining_ms =
        static_cast<int>((deadline - base::Time::Now()).InMilliseconds());
    if (timeout_remaining_ms < 0 || poll(fds, 1, timeout_remaining_ms) != 1) {
      break;
    }
    base::CheckedNumeric<size_t> new_size =
        base::CheckedNumeric<size_t>(output.size()) +
        base::CheckedNumeric<size_t>(kBufferSize);
    if (!new_size.IsValid() || !total_bytes_read.IsValid()) {
      // Ignore the rest of the output.
      break;
    }
    output.resize(new_size.ValueOrDie());
    read_this_pass = HANDLE_EINTR(read(
        read_fd.get(), &(output)[total_bytes_read.ValueOrDie()], kBufferSize));
    if (read_this_pass >= 0) {
      total_bytes_read += base::CheckedNumeric<size_t>(read_this_pass);
      if (!total_bytes_read.IsValid()) {
        // Ignore the rest of the output.
        break;
      }
      output.resize(total_bytes_read.ValueOrDie());
    }
  } while (read_this_pass > 0);

  base::TimeDelta remain =
      std::max(deadline - base::Time::Now(), base::TimeDelta());
  ProcessStatus result = WaitForProcessExit(proc, remain);
  if (result == ProcessStillRunning{}) {
    proc.Terminate(1, false);
  }
  return {result, proc.Pid(), output};
}

std::string ReplaceAll(std::string_view s, Replacements rs) {
  std::string ret{s};
  for (const std::pair<std::string, std::string>& r : rs) {
    base::ReplaceSubstringsAfterOffset(&ret, 0, r.first, r.second);
  }
  return ret;
}

}  // namespace installer::mac::test
