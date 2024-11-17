// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/test_support/scoped_log_grabber.h"

#include <windows.h>

#include <ostream>

#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "chrome/windows_services/service_program/logging_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SaveArg;

namespace {

MULTIPROCESS_TEST_MAIN(WriteToLog) {
  InitializeLogging(*base::CommandLine::ForCurrentProcess());

  // Emitting a log message should go to the parent.
  LOG(ERROR) << __func__;
  return ERROR_SUCCESS;
}

// Emits a multi-line log message.
MULTIPROCESS_TEST_MAIN(WriteMultiLineToLog) {
  InitializeLogging(*base::CommandLine::ForCurrentProcess());

  LOG(ERROR) << __func__ << std::endl << __func__;
  return ERROR_SUCCESS;
}

}  // namespace

// A test fixture that redirects the test process's stderr to a pipe so that
// tests can validate messages written to stderr by code under test. There is
// also a facility for launching a child proc, whereby the child proc's log
// output is sent to the test's process's stderr.
class ScopedLogGrabberTest : public testing::Test {
 protected:
  ScopedLogGrabberTest() : original_stderr_(::GetStdHandle(STD_ERROR_HANDLE)) {}
  ~ScopedLogGrabberTest() override {
    // Restore stderr for the test process.
    ::SetStdHandle(STD_ERROR_HANDLE, original_stderr_);
  }

  void SetUp() override {
    ASSERT_NE(original_stderr_, INVALID_HANDLE_VALUE);

    // Create a pipe to intercept this process's stderr.
    HANDLE read_handle = nullptr;
    HANDLE write_handle = nullptr;
    ASSERT_TRUE(::CreatePipe(&read_handle, &write_handle,
                             /*lpPipeAttributes=*/nullptr, /*nSize=*/0));
    read_pipe_ = base::File(std::exchange(read_handle, nullptr));
    write_pipe_ = base::File(std::exchange(write_handle, nullptr));
    ASSERT_TRUE(
        ::SetStdHandle(STD_ERROR_HANDLE, write_pipe_.GetPlatformFile()));
  }

  // Returns a handle to a child process that will run `function_name` (which
  // must call `InitializeLogging()`) and grab its log output via `log_grabber`.
  base::Process SpawnChild(std::string_view function_name,
                           const ScopedLogGrabber& log_grabber) {
    base::CommandLine command_line =
        base::GetMultiProcessTestChildBaseCommandLine();
    log_grabber.AddLoggingSwitches(command_line);
    base::LaunchOptions launch_options;
    launch_options.start_hidden = true;
    return base::SpawnMultiProcessTestChild(std::string(function_name),
                                            command_line, launch_options);
  }

  // Returns the read end of the pipe connected to the test process's stderr.
  base::File& read_pipe() { return read_pipe_; }

 private:
  // The process's original stderr handle for restoration at destruction.
  const HANDLE original_stderr_;

  // Handles to an anonymous pipe used to intercept this test process's stderr.
  base::File read_pipe_;
  base::File write_pipe_;
};

// Tests that log message from a child process are emitting to this process's
// stderr.
TEST_F(ScopedLogGrabberTest, TestLogging) {
  // Prepare to capture logs from the child process.
  ScopedLogGrabber log_grabber;

  // Launch the child process with the logging options.
  constexpr std::string_view kChildFunction = "WriteToLog";
  base::Process child_process = SpawnChild(kChildFunction, log_grabber);

  // Read logs from the child until it logs its name.
  auto buffer = base::HeapArray<uint8_t>::Uninit(4096);
  while (true) {
    std::optional<size_t> bytes_read =
        read_pipe().ReadAtCurrentPos(buffer.as_span());
    ASSERT_TRUE(bytes_read.has_value());
    const auto log_message = base::as_string_view(buffer.first(*bytes_read));
    if (log_message.find(kChildFunction) != std::string_view::npos) {
      break;
    }
  }

  // Wait for the child to terminate.
  int exit_code = ERROR_SUCCESS;
  ASSERT_TRUE(child_process.WaitForExit(&exit_code));
  EXPECT_EQ(exit_code, ERROR_SUCCESS);
}

// Tests that log message from a child process are passed to a callback.
TEST_F(ScopedLogGrabberTest, TestWithDelegate) {
  // Prepare to capture logs from the child process with a callback. Ignore
  base::MockCallback<ScopedLogGrabber::LogMessageCallback> mock_callback;

  ScopedLogGrabber log_grabber;
  log_grabber.SetLogMessageCallback(mock_callback.Get());

  // Ignore all messages except one expected multi-line message.
  base::ProcessId pid = base::kNullProcessId;
  EXPECT_CALL(mock_callback, Run(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_callback,
              Run(_, HasSubstr("WriteMultiLineToLog\nWriteMultiLineToLog\n")))
      .WillOnce(DoAll(SaveArg<0>(&pid), Return(true)));

  // Launch the child process with the logging options.
  constexpr std::string_view kChildFunction = "WriteMultiLineToLog";
  base::Process child_process = SpawnChild(kChildFunction, log_grabber);

  // Wait for the child to terminate.
  int exit_code = ERROR_SUCCESS;
  ASSERT_TRUE(child_process.WaitForExit(&exit_code));
  EXPECT_EQ(exit_code, ERROR_SUCCESS);

  // The callback should have been run with the child process's pid.
  EXPECT_EQ(pid, child_process.Pid());
}

TEST(ScopedLogGrabberParseTest, ProcessId) {
  // Inputs that don't match "^\[[0-9]+:.*"
  EXPECT_EQ(ScopedLogGrabber::ParseProcessId(""), base::kNullProcessId);
  EXPECT_EQ(ScopedLogGrabber::ParseProcessId("["), base::kNullProcessId);
  EXPECT_EQ(ScopedLogGrabber::ParseProcessId("[1"), base::kNullProcessId);
  EXPECT_EQ(ScopedLogGrabber::ParseProcessId("1:"), base::kNullProcessId);
  EXPECT_EQ(ScopedLogGrabber::ParseProcessId("[a"), base::kNullProcessId);
  EXPECT_EQ(ScopedLogGrabber::ParseProcessId("[:"), base::kNullProcessId);
  EXPECT_EQ(ScopedLogGrabber::ParseProcessId("hi, mom"), base::kNullProcessId);

  // Inputs that do.
  EXPECT_EQ(ScopedLogGrabber::ParseProcessId("[1:"), 1U);
  EXPECT_EQ(ScopedLogGrabber::ParseProcessId("[535:"), 535U);
  EXPECT_EQ(ScopedLogGrabber::ParseProcessId(
                "[11916:15856:ERROR:service_unittest.cc(91)] blah"),
            11916U);
}
