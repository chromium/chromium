// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/test/test_executables.h"

#include <memory>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/win_util.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/win/test/test_inheritable_event.h"
#include "chrome/updater/win/test/test_strings.h"

namespace updater {

// If you add another test executable here, also add it to the data_deps in
// the "test_executables" target of updater/win/test/BUILD.gn.
const wchar_t kTestProcessExecutableName[] = L"updater_test_process.exe";

base::Process LongRunningProcess(UpdaterScope scope,
                                 const std::string& test_name,
                                 base::CommandLine* cmd) {
  base::CommandLine command_line = GetTestProcessCommandLine(scope, test_name);

  // This will ensure this new process will run for one minute before dying.
  command_line.AppendSwitchASCII(updater::kTestSleepSecondsSwitch, "60");

  auto init_done_event = updater::CreateInheritableEvent(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  command_line.AppendSwitchNative(
      updater::kInitDoneNotifierSwitch,
      base::NumberToWString(
          base::win::HandleToUint32(init_done_event->handle())));

  if (cmd) {
    *cmd = command_line;
  }

  base::LaunchOptions launch_options;
  launch_options.handles_to_inherit.push_back(init_done_event->handle());
  base::Process result = base::LaunchProcess(command_line, launch_options);

  if (!init_done_event->TimedWait(base::Seconds(10))) {
    LOG(ERROR) << "Process did not signal";
    result.Terminate(/*exit_code=*/1, /*wait=*/false);
    return base::Process();
  }

  return result;
}

base::CommandLine GetTestProcessCommandLine(UpdaterScope scope,
                                            const std::string& test_name) {
  base::FilePath executable_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &executable_path));

  base::CommandLine command_line(
      executable_path.Append(kTestProcessExecutableName));
  if (IsSystemInstall(scope)) {
    command_line.AppendSwitch(kSystemSwitch);
  }

  command_line.AppendSwitchASCII(kTestName, test_name);
  return command_line;
}

}  // namespace updater
