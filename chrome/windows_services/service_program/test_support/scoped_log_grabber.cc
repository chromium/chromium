// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/test_support/scoped_log_grabber.h"

#include <windows.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/win_util.h"
#include "chrome/windows_services/service_program/switches.h"

ScopedLogGrabber::ScopedLogGrabber() {
  // Create a pipe to give to the child as stderr.
  HANDLE read_handle;
  HANDLE write_handle;
  PCHECK(::CreatePipe(&read_handle, &write_handle,
                      /*lpPipeAttributes=*/nullptr, /*nSize=*/0));
  read_pipe_ = base::File(std::exchange(read_handle, nullptr));
  write_pipe_ = base::File(std::exchange(write_handle, nullptr));

  // Spin off a thread that will stream from `read_pipe_` to this process's
  // stderr (its log destination). `base::ThreadPool` cannot be used because a
  // test using this class may not yet have created a TaskEnvironment.
  CHECK(base::PlatformThread::Create(/*stack_size=*/0, /*delegate=*/this,
                                     &output_thread_));
}

ScopedLogGrabber::~ScopedLogGrabber() {
  // Close the write pipe, which will trigger the streaming thread to terminate.
  write_pipe_.Close();

  // Collect the streaming thread.
  base::PlatformThread::Join(output_thread_);
}

void ScopedLogGrabber::AddLoggingSwitches(
    base::CommandLine& command_line) const {
  command_line.AppendSwitchASCII(switches::kLogFile,
                                 base::NumberToString(base::win::HandleToUint32(
                                     write_pipe_.GetPlatformFile())));
  command_line.AppendSwitchASCII(
      switches::kLogFileSource,
      base::NumberToString(base::Process::Current().Pid()));
}

void ScopedLogGrabber::ThreadMain() {
  base::PlatformThread::SetName("ScopedLogGrabber");

  // Duplicate this process's stderr handle for use below.
  HANDLE stderr_handle = ::GetStdHandle(STD_ERROR_HANDLE);
  PCHECK(stderr_handle != INVALID_HANDLE_VALUE);
  HANDLE duplicate;
  PCHECK(::DuplicateHandle(
      ::GetCurrentProcess(), std::exchange(stderr_handle, nullptr),
      ::GetCurrentProcess(), &duplicate,
      /*dwDesiredAccess=*/0, /*bInheritHandle=*/FALSE, DUPLICATE_SAME_ACCESS));
  base::File std_err(std::exchange(duplicate, nullptr));

  // Read logs from the service until the write-side of the pipe is closed by
  // both any running service and the test's ScopedLogGrabber instance.
  auto buffer = base::HeapArray<uint8_t>::Uninit(4096);
  while (true) {
    std::optional<size_t> bytes_read =
        read_pipe_.ReadAtCurrentPos(buffer.as_span());
    if (!bytes_read.has_value()) {
      const auto error = ::GetLastError();
      CHECK_EQ(error, static_cast<DWORD>(ERROR_BROKEN_PIPE)) << error;
      break;
    }
    PCHECK(std_err.WriteAtCurrentPosAndCheck(buffer.first(*bytes_read)));
  }
}
