// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <shlobj.h>

#include <string>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/win/test/test_initializer.h"
#include "chrome/updater/win/test/test_strings.h"

namespace {

base::WaitableEvent EventForSwitch(const base::CommandLine& command_line,
                                   const char switch_value[]) {
  CHECK(command_line.HasSwitch(switch_value));

  const std::wstring event_name =
      command_line.GetSwitchValueNative(switch_value);
  VLOG(1) << __func__ << " event name '" << event_name << "'";
  base::win::ScopedHandle handle(
      ::OpenEvent(EVENT_ALL_ACCESS, TRUE, event_name.c_str()));
  PLOG_IF(ERROR, !handle.IsValid())
      << __func__ << " cannot open event '" << event_name << "'";
  return base::WaitableEvent(std::move(handle));
}

int DoMain(const base::CommandLine* command_line) {
  VLOG(1) << "Test process starting. Command line: " << ::GetCommandLine()
          << ": Pid: " << GetCurrentProcessId();

  if (command_line->HasSwitch(updater::kTestName)) {
    VLOG(1) << "Running for test: "
            << command_line->GetSwitchValueASCII(updater::kTestName);
  }

  if (command_line->HasSwitch(updater::kTestSleepSecondsSwitch)) {
    std::string value =
        command_line->GetSwitchValueASCII(updater::kTestSleepSecondsSwitch);
    int sleep_seconds = 0;
    if (!base::StringToInt(value, &sleep_seconds) || sleep_seconds <= 0) {
      LOG(ERROR) << "Invalid sleep delay value " << value;
      NOTREACHED_IN_MIGRATION();
    }

    VLOG(1) << "Process is sleeping for " << sleep_seconds << " seconds";
    ::Sleep(base::Seconds(sleep_seconds).InMilliseconds());
    return 0;
  }

  if (command_line->HasSwitch(updater::kTestEventToSignal)) {
    EventForSwitch(*command_line, updater::kTestEventToSignal).Signal();
  }

  if (command_line->HasSwitch(updater::kTestEventToSignalIfMediumIntegrity)) {
    if (!::IsUserAnAdmin()) {
      EventForSwitch(*command_line,
                     updater::kTestEventToSignalIfMediumIntegrity)
          .Signal();
    } else {
      LOG(ERROR) << "Process running at High Integrity instead of Medium";
    }
  }

  if (command_line->HasSwitch(updater::kTestEventToWaitOn)) {
    EventForSwitch(*command_line, updater::kTestEventToWaitOn).Wait();
  }

  if (command_line->HasSwitch(updater::kTestExitCode)) {
    int exit_code = 0;
    CHECK(base::StringToInt(
        command_line->GetSwitchValueASCII(updater::kTestExitCode), &exit_code));
    VLOG(1) << "Process ending with exit code: " << exit_code;
    return exit_code;
  }

  return 0;
}

}  // namespace

int main(int, char**) {
  base::AtExitManager exit_manager;

  bool success = base::CommandLine::Init(0, nullptr);
  CHECK(success);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(updater::kEnableLoggingSwitch)) {
    InitLogging(command_line->HasSwitch(updater::kSystemSwitch)
                    ? updater::UpdaterScope::kSystem
                    : updater::UpdaterScope::kUser);
  }

  updater::NotifyInitializationDoneForTesting();

  int exit_code = DoMain(command_line);
  VLOG(1) << "Test process ended. Exit code: " << exit_code
          << ": Pid: " << GetCurrentProcessId();
  return exit_code;
}
