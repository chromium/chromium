// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/installer.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/win/util.h"

namespace updater {

// Clear the installer progress, run the application installer, loop and query
// the installer progress, then collect the process exit code, or return with
// a time out if the process did not terminate in time.
// The installer progress is written by the application installer as a value
// under the application's client state in the Windows registry and read by
// an utility function invoked below.
int Installer::RunApplicationInstaller(const base::FilePath& app_installer,
                                       const std::string& arguments,
                                       ProgressCallback progress_callback) {
  DeleteInstallerProgress(app_id());

  base::LaunchOptions options;
  options.start_hidden = true;
  const auto cmdline =
      base::StrCat({base::CommandLine(app_installer).GetCommandLineString(),
                    L" ", base::UTF8ToWide(arguments)});
  DVLOG(1) << "Running application installer: " << cmdline;
  auto process = base::LaunchProcess(cmdline, options);
  int exit_code = -1;
  const auto time_begin = base::Time::NowFromSystemTime();
  do {
    bool wait_result = process.WaitForExitWithTimeout(
        base::TimeDelta::FromSeconds(kWaitForInstallerProgressSec), &exit_code);
    auto progress = GetInstallerProgress(app_id());
    DVLOG(3) << "installer progress: " << progress;
    progress_callback.Run(progress);
    if (wait_result)
      break;
  } while (base::Time::NowFromSystemTime() - time_begin <=
           base::TimeDelta::FromSeconds(kWaitForAppInstallerSec));

  return exit_code;
}

}  // namespace updater
