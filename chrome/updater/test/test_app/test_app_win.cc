// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/test_app/test_app.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

int InstallUpdater() {
  base::FilePath test_executable;
  CHECK(base::PathService::Get(base::FILE_EXE, &test_executable));

  base::CommandLine command_line(
      test_executable.DirName().AppendASCII("UpdaterSetup.exe"));
  command_line.AppendSwitch(kInstallSwitch);
  if (GetProcessScope() == UpdaterScope::kSystem)
    command_line.AppendSwitch(kSystemSwitch);
  command_line.AppendSwitch("enable-logging");
  command_line.AppendSwitchASCII("--vmodule", "*/updater/*=2");

  VLOG(0) << " Run command: " << command_line.GetCommandLineString();

  // TODO(crbug.com/1096654): get the timeout from TestTimeouts.
  base::Process process = base::LaunchProcess(command_line, {});
  int exit_code = -1;
  process.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(45), &exit_code);

  LOG_IF(ERROR, exit_code != 0)
      << "Couldn't install the updater. Exit code: " << exit_code;

  return exit_code;
}

}  // namespace updater
