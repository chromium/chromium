// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/win/test/test_inheritable_event.h"
#include "chrome/updater/win/test/test_strings.h"

namespace updater {

// If you add another test executable here, also add it to the data_deps in
// the "test_executables" target of updater/win/test/BUILD.gn.
const base::char16 kTestProcessExecutableName[] = L"updater_test_process.exe";

base::Process LongRunningProcess(base::CommandLine* cmd) {
  base::FilePath exe_dir;
  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
    LOG(ERROR) << "Failed to get the executable path, unable to create always "
                  "running process";
    return base::Process();
  }

  base::FilePath exe_path = exe_dir.Append(updater::kTestProcessExecutableName);
  base::CommandLine command_line(exe_path);

  // This will ensure this new process will run for one minute before dying.
  command_line.AppendSwitchASCII(updater::kTestSleepMinutesSwitch, "1");

  auto init_done_event = updater::CreateInheritableEvent(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  command_line.AppendSwitchNative(
      updater::kInitDoneNotifierSwitch,
      base::NumberToString16(
          base::win::HandleToUint32(init_done_event->handle())));

  if (cmd)
    *cmd = command_line;

  base::LaunchOptions launch_options;
  launch_options.handles_to_inherit.push_back(init_done_event->handle());
  base::Process result = base::LaunchProcess(command_line, launch_options);

  if (!init_done_event->TimedWait(base::TimeDelta::FromSeconds(10))) {
    LOG(ERROR) << "Process did not signal";
    result.Terminate(/*exit_code=*/1, /*wait=*/false);
    return base::Process();
  }

  return result;
}

}  // namespace updater
