// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_util.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/time/time.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"

namespace google_update {

namespace {

const int kGoogleUpdateTimeoutMs = 20 * 1000;

// Launches command |cmd_string|, and waits for |timeout| milliseconds before
// timing out.  To wait indefinitely, one can set
// |timeout| to be base::TimeDelta::Max().
// Returns true if this executes successfully.
// Returns false if command execution fails to execute, or times out.
bool LaunchProcessAndWaitWithTimeout(const std::wstring& cmd_string,
                                     base::TimeDelta timeout) {
  bool success = false;
  int exit_code = 0;
  VLOG(0) << "Launching: " << cmd_string;
  base::Process process =
      base::LaunchProcess(cmd_string, base::LaunchOptions());
  if (!process.IsValid()) {
    PLOG(ERROR) << "Failed to launch (" << cmd_string << ")";
  } else if (!process.WaitForExitWithTimeout(timeout, &exit_code)) {
    // The GetExitCodeProcess failed or timed-out.
    LOG(ERROR) << "Command (" << cmd_string << ") is taking more than "
               << timeout.InMilliseconds() << " milliseconds to complete.";
  } else if (exit_code != 0) {
    LOG(ERROR) << "Command (" << cmd_string << ") exited with code "
               << exit_code;
  } else {
    success = true;
  }
  return success;
}

}  // namespace

bool UninstallGoogleUpdate(bool system_install) {
  bool success = false;
  std::wstring cmd_string(
      GoogleUpdateSettings::GetUninstallCommandLine(system_install));
  if (cmd_string.empty()) {
    success = true;  // Nothing to; vacuous success.
  } else {
    success = LaunchProcessAndWaitWithTimeout(
        cmd_string, base::Milliseconds(kGoogleUpdateTimeoutMs));
  }
  return success;
}

void ElevateIfNeededToReenableUpdates() {
  installer::ProductState product_state;
  const bool system_install = !InstallUtil::IsPerUserInstall();
  if (!product_state.Initialize(system_install))
    return;
  base::FilePath exe_path(product_state.GetSetupPath());
  if (exe_path.empty() || !base::PathExists(exe_path)) {
    LOG(ERROR) << "Could not find setup.exe to reenable updates.";
    return;
  }

  base::CommandLine cmd(exe_path);
  cmd.AppendSwitch(installer::switches::kReenableAutoupdates);
  InstallUtil::AppendModeAndChannelSwitches(&cmd);
  if (system_install)
    cmd.AppendSwitch(installer::switches::kSystemLevel);
  if (product_state.uninstall_command().HasSwitch(
          installer::switches::kVerboseLogging)) {
    cmd.AppendSwitch(installer::switches::kVerboseLogging);
  }

  base::LaunchOptions launch_options;
  launch_options.force_breakaway_from_job_ = true;
  launch_options.elevated = base::win::UserAccountControlIsEnabled();
  base::LaunchProcess(cmd, launch_options);
}

}  // namespace google_update
