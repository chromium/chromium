// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_LOG_GRABBER_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_LOG_GRABBER_H_

#include "base/files/file.h"
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

  // base::PlatformThread::Delegate:
  void ThreadMain() override;

 private:
  // One end of an anonymous pipe on which the service will write its stderr
  // output (the destination of `LOG()` statements).
  base::File write_pipe_;

  // One end of an anonymous pipe on which the test will read the service's
  // stderr output (the destination of `LOG()` statements).
  base::File read_pipe_;

  // A thread that reads from `read_pipe_` and emits to stderr.
  base::PlatformThreadHandle output_thread_;
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_LOG_GRABBER_H_
