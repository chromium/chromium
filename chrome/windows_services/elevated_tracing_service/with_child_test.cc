// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/elevated_tracing_service/with_child_test.h"

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/win_util.h"
#include "testing/multiprocess_func_list.h"

namespace elevated_tracing_service {

namespace {

constexpr std::string_view kStartEventHandle = "start-event-handle";
constexpr std::string_view kExitEventHandle = "exit-event-handle";

void AppendSwitchHandle(base::CommandLine& command_line,
                        std::string_view switch_name,
                        HANDLE handle) {
  command_line.AppendSwitchNative(
      switch_name, base::NumberToWString(base::win::HandleToUint32(handle)));
}

// Returns a handle from the value of a switch on the process's command line,
// or an invalid handle in case of error.
base::win::ScopedHandle HandleFromSwitch(const base::CommandLine& command_line,
                                         std::string_view switch_name) {
  if (auto switch_value = command_line.GetSwitchValueNative(switch_name);
      !switch_value.empty()) {
    uint32_t handle_value;
    if (base::StringToUint(switch_value, &handle_value)) {
      return base::win::ScopedHandle(base::win::Uint32ToHandle(handle_value));
    }
  }
  return base::win::ScopedHandle();
}

// A child process's main function. It signals to the parent that it has
// started, and then waits for the parent to signal that it's time to terminate.
MULTIPROCESS_TEST_MAIN(ExitWhenSignaled) {
  WithChildTest::SignalChildStart();
  return WithChildTest::WaitForChildTermination() ? 0 : 1;
}

}  // namespace

// static
void WithChildTest::SignalChildStart() {
  // Notify the test process that the child has started.
  if (auto handle = HandleFromSwitch(*base::CommandLine::ForCurrentProcess(),
                                     kStartEventHandle);
      handle.is_valid()) {
    base::WaitableEvent(std::move(handle)).Signal();
  }
}

// static
bool WithChildTest::WaitForChildTermination() {
  // Notify the test process that the child has started.
  if (auto handle = HandleFromSwitch(*base::CommandLine::ForCurrentProcess(),
                                     kExitEventHandle);
      handle.is_valid()) {
    base::WaitableEvent(std::move(handle)).Wait();
    return true;
  }
  return false;
}

base::Process WithChildTest::SpawnChildWithEventHandles(
    std::string_view procname) {
  base::LaunchOptions launch_options;
  launch_options.start_hidden = true;
  launch_options.feedback_cursor_off = true;
  launch_options.handles_to_inherit.push_back(child_started_event_.handle());
  launch_options.handles_to_inherit.push_back(exit_child_event_.handle());
  return SpawnChildWithOptions(std::string(procname), launch_options);
}

base::CommandLine WithChildTest::MakeCmdLine(const std::string& procname) {
  auto result = base::MultiProcessTest::MakeCmdLine(procname);
  AppendSwitchHandle(result, kStartEventHandle, child_started_event_.handle());
  AppendSwitchHandle(result, kExitEventHandle, exit_child_event_.handle());
  return result;
}

}  // namespace elevated_tracing_service
