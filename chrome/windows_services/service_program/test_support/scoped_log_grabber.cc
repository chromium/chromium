// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/test_support/scoped_log_grabber.h"

#include <windows.h>

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/windows_handle_util.h"
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
  command_line.AppendSwitchASCII(switches::kLogFileHandle,
                                 base::NumberToString(base::win::HandleToUint32(
                                     write_pipe_.GetPlatformFile())));
  command_line.AppendSwitchASCII(
      switches::kLogFileSource,
      base::NumberToString(base::Process::Current().Pid()));
}

void ScopedLogGrabber::SetLogMessageCallback(LogMessageCallback callback) {
  base::AutoLock callback_lock(lock_);
  callback_ = std::move(callback);
}

namespace {

// Processes `current_message` (any unprocessed data from the service); sending
// each newline-delimited log message to `callback`. Messages that are not
// processed by the callback are emitted to `std_err`. Messages are removed from
// `current_message` following processing so that any remaining data is included
// in subsequent invocations.
void ProcessMessages(ScopedLogGrabber::LogMessageCallback& callback,
                     base::File& std_err,
                     std::string& current_message) {
  // Basic assumption: `current_message` is either empty, or begins with a log
  // message. Lines of data that do not begin with a message header (i.e.,
  // "[PID:") are assumed to be continuations of a previous multi-line message.
  // Lines that precede the first valid message are given to the callback with a
  // null PID.

  // A view of the unprocessed portion of `current_message`.
  std::string_view remaining(current_message);

  // The position in `remaining` of the start of a line.
  std::string_view::size_type line_start = 0;

  // Scan forward to the next distinct message; i.e., a newline at the end of
  // the string or a newline followed by a new message header.
  while (true) {
    const std::string_view::size_type newline =
        remaining.find('\n', line_start);
    if (newline == std::string_view::npos) {
      break;  // Incomplete line. Return to read more data.
    }
    // If this newline is at the end of the data or it is followed by a new
    // message, send all data up to and including this newline to the callback
    // and emit it if the callback returns false.
    std::string_view next_text = remaining.substr(newline + 1);
    if (next_text.empty() ||
        ScopedLogGrabber::ParseProcessId(next_text) != base::kNullProcessId) {
      std::string_view text = remaining.substr(0, newline + 1);
      if (!callback.Run(ScopedLogGrabber::ParseProcessId(text), text)) {
        PCHECK(std_err.WriteAtCurrentPosAndCheck(base::as_byte_span(text)));
      }
      // Update `remaining` and look for another message in it.
      remaining = next_text;
      line_start = 0;
      continue;
    }
    // Otherwise, this line is part of a multi-line message. Continue scanning
    // forward for more lines until the next message or the end of the data is
    // found.
    line_start += newline + 1;
  }

  // Erase all data before `remaining`, as it has all been processed.
  current_message.erase(0, remaining.data() - current_message.data());
}

}  // namespace

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

  // A buffer to hold text to be handled by `ProcessMessages()` if a callback
  // has been set.
  std::string current_message;

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
    base::span<const uint8_t> bytes = buffer.first(*bytes_read);
    base::AutoLock callback_lock(lock_);
    if (!callback_) {
      // No callback, so simply emit all data read
      PCHECK(std_err.WriteAtCurrentPosAndCheck(bytes));
    } else {
      // Accumulate data into `current_message` and pass all individual messages
      // to the callback, emitting only if the callback returns false.
      current_message += base::as_string_view(bytes);
      ProcessMessages(callback_, std_err, current_message);
    }
  }
}

// static
base::ProcessId ScopedLogGrabber::ParseProcessId(std::string_view message) {
  constexpr size_t kMinMessageHeaderSize = 3;  // '[', N, ':'
  if (message.size() >= kMinMessageHeaderSize && message.front() == '[') {
    std::string_view rest = message.substr(1);
    if (std::string_view::size_type colon_pos = rest.find(':');
        colon_pos != std::string_view::npos) {
      unsigned pid = base::kNullProcessId;
      if (base::StringToUint(rest.substr(0, colon_pos), &pid)) {
        return static_cast<base::ProcessId>(pid);
      }
    }
  }
  return base::kNullProcessId;
}
