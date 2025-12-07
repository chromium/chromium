// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_LOG_GRABBER_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_LOG_GRABBER_H_

#include <string_view>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"

namespace base {
class CommandLine;
}

// A test helper for grabbing the log output of a test installation of a COM
// service and emitting it to a test process's stderr.
class ScopedLogGrabber : public base::PlatformThread::Delegate {
 public:
  ScopedLogGrabber();
  ScopedLogGrabber(const ScopedLogGrabber&) = delete;
  ScopedLogGrabber& operator=(const ScopedLogGrabber&) = delete;
  ~ScopedLogGrabber() override;

  // Adds switches to `command_line` so that a COM service started with it can
  // direct its stderr to this test process's stderr.
  void AddLoggingSwitches(base::CommandLine& command_line) const;

  // A callback that will be run on a background thread upon receipt of a log
  // message.
  using LogMessageCallback =
      base::RepeatingCallback<bool(base::ProcessId process_id,
                                   std::string_view message)>;

  // Sets a callback to be run for each message received from a service process.
  // The callback is run with the process ID of the service and the log message.
  // The message is not emitted to the test process's stderr if the callback
  // returns true.
  void SetLogMessageCallback(LogMessageCallback callback);

  // base::PlatformThread::Delegate:
  void ThreadMain() override;

  // Returns the pid parsed from a message string of the form "[PID:.*", or
  // kNullProcessId in case of unexpected data.
  static base::ProcessId ParseProcessId(std::string_view message);

 private:
  // One end of an anonymous pipe on which the service will write its stderr
  // output (the destination of `LOG()` statements).
  base::File write_pipe_;

  // One end of an anonymous pipe on which the test will read the service's
  // stderr output (the destination of `LOG()` statements).
  base::File read_pipe_;

  // A thread that reads from `read_pipe_` and emits to stderr.
  base::PlatformThreadHandle output_thread_;

  // A lock to protect `callback_`, which may be accessed both by callers on
  // the main thread and by the log processing thread.
  base::Lock lock_;

  // An optional callback to which log messages may be sent.
  LogMessageCallback callback_ GUARDED_BY(lock_);
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_LOG_GRABBER_H_
