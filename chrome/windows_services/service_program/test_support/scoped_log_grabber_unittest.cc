// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/test_support/scoped_log_grabber.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/test/multiprocess_test.h"
#include "chrome/windows_services/service_program/logging_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace {

MULTIPROCESS_TEST_MAIN(WriteToLog) {
  InitializeLogging(*base::CommandLine::ForCurrentProcess());

  // Emitting a log message should go to the parent.
  LOG(ERROR) << __func__;
  return ERROR_SUCCESS;
}

}  // namespace

// Tests that log message from a child process are emitting to this process's
// stderr.
TEST(ScopedLogGrabberTest, TestLogging) {
  // Create a pipe and temporarily replace this proc's stderr with its write
  // end.
  HANDLE read_handle;
  HANDLE write_handle;
  ASSERT_TRUE(::CreatePipe(&read_handle, &write_handle,
                           /*lpPipeAttributes=*/nullptr, /*nSize=*/0));
  base::File read_pipe(std::exchange(read_handle, nullptr));
  base::File write_pipe(std::exchange(write_handle, nullptr));

  const HANDLE original_stderr = ::GetStdHandle(STD_ERROR_HANDLE);
  ASSERT_NE(original_stderr, INVALID_HANDLE_VALUE);
  absl::Cleanup restore_stderr = [&original_stderr] {
    ::SetStdHandle(STD_ERROR_HANDLE, original_stderr);
  };
  ASSERT_TRUE(::SetStdHandle(STD_ERROR_HANDLE, write_pipe.GetPlatformFile()));

  // Prepare to capture logs from the child process.
  ScopedLogGrabber log_grabber;

  // Launch the child process with the logging options.
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  log_grabber.AddLoggingSwitches(command_line);
  base::LaunchOptions launch_options;
  launch_options.start_hidden = true;
  base::Process child_process = base::SpawnMultiProcessTestChild(
      "WriteToLog", command_line, launch_options);

  // Read logs from the child until it logs its name.
  auto buffer = base::HeapArray<uint8_t>::Uninit(4096);
  while (true) {
    std::optional<size_t> bytes_read =
        read_pipe.ReadAtCurrentPos(buffer.as_span());
    ASSERT_TRUE(bytes_read.has_value());
    const auto log_message = base::as_string_view(buffer.first(*bytes_read));
    if (log_message.find("WriteToLog") != std::string_view::npos) {
      break;
    }
  }

  // Wait for the child to terminate.
  int exit_code = ERROR_SUCCESS;
  ASSERT_TRUE(child_process.WaitForExit(&exit_code));
  EXPECT_EQ(exit_code, ERROR_SUCCESS);
}
