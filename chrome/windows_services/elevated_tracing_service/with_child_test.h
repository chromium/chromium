// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_WITH_CHILD_TEST_H_
#define CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_WITH_CHILD_TEST_H_

#include <string>
#include <string_view>

#include "base/process/process.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_handle.h"

namespace base {
class CommandLine;
}

namespace elevated_tracing_service {

// A test fixture that has facilities for coordinating with a child process.
class WithChildTest : public base::MultiProcessTest {
 public:
  // Helper functions for use in children.

  // Notify the test process that the child has started.
  static void SignalChildStart();

  // Wait for the test process to tell the child to terminate.
  static bool WaitForChildTermination();

 protected:
  static constexpr std::string_view kExitWhenSignaled = "ExitWhenSignaled";

  WithChildTest() = default;

  // Runs the child process, passing it handles to the two events; one for it to
  // signal when it enters its main function, and one for it to wait on before
  // terminating. `procname` may be "ExitWhenSignaled" for a child that will
  // signal that it has started and then wait for the test to signal that it
  // should terminate. Alternatively, the test may supply a custom function.
  base::Process SpawnChildWithEventHandles(std::string_view procname);

  // Waits for the child process to enter its main function.
  void WaitForChildStart() { child_started_event_.Wait(); }

  // Signals the child process to terminate.
  void SignalChildTermination() { exit_child_event_.Signal(); }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  // base::MultiProcessTest:
  base::CommandLine MakeCmdLine(const std::string& procname) override;

 private:
  base::test::TaskEnvironment task_environment_;

  // An event signaled by the child once it has started.
  base::WaitableEvent child_started_event_;

  // An event signaled by the test to tell the child to terminate.
  base::WaitableEvent exit_child_event_;
};

}  // namespace elevated_tracing_service

#endif  // CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_WITH_CHILD_TEST_H_
