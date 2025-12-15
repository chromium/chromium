// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mac/install_test_util.h"

#include <poll.h>

#include <string>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/numerics/checked_math.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer::mac::test {

void AssertExecutableCompletes(const base::CommandLine& cmd,
                               const base::EnvironmentMap& env,
                               const base::FilePath& working_dir,
                               const base::TimeDelta& timeout,
                               std::string* output,
                               int* exit_code) {
  base::ScopedFD read_fd, write_fd;
  {
    int pipefds[2] = {};
    ASSERT_EQ(pipe(pipefds), 0);
    read_fd.reset(pipefds[0]);
    write_fd.reset(pipefds[1]);
  }

  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(write_fd.get(), STDOUT_FILENO);
  options.fds_to_remap.emplace_back(write_fd.get(), STDERR_FILENO);
  options.current_directory = working_dir;
  options.clear_environment = true;
  options.environment = env;
  const base::Process proc = base::LaunchProcess(cmd, options);
  ASSERT_TRUE(proc.IsValid());
  write_fd.reset();

  const base::Time deadline = base::Time::Now() + timeout;

  static constexpr size_t kBufferSize = 1024;
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
        base::CheckedNumeric<size_t>(output->size()) +
        base::CheckedNumeric<size_t>(kBufferSize);
    if (!new_size.IsValid() || !total_bytes_read.IsValid()) {
      // Ignore the rest of the output.
      break;
    }
    output->resize(new_size.ValueOrDie());
    read_this_pass = HANDLE_EINTR(read(
        read_fd.get(), &(*output)[total_bytes_read.ValueOrDie()], kBufferSize));
    if (read_this_pass >= 0) {
      total_bytes_read += base::CheckedNumeric<size_t>(read_this_pass);
      if (!total_bytes_read.IsValid()) {
        // Ignore the rest of the output.
        break;
      }
      output->resize(total_bytes_read.ValueOrDie());
    }
  } while (read_this_pass > 0);

  if (proc.WaitForExitWithTimeout(
          std::max(deadline - base::Time::Now(), base::TimeDelta()),
          exit_code)) {
    return;
  }
  proc.Terminate(1, false);
  FAIL() << "installer::mac::test::AssertExecutableCompletes timed out.";
}

}  // namespace installer::mac::test
