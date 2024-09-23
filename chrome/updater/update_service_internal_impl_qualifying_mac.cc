// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_internal_impl_qualifying.h"

#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"

namespace updater {

namespace {

// CheckLauncherCanLaunchServer runs the updater UpdateService launcher in a
// test mode. The launcher will confirm that it can launch the UpdateService
// and exit with exit code 0 if so. This function returns true if the launcher
// exits with 0.
bool CheckLauncherCanLaunchServer(UpdaterScope scope) {
  std::optional<base::FilePath> app_bundle_path =
      GetUpdaterAppBundlePath(scope);
  if (!app_bundle_path) {
    VLOG(1) << "No app bundle path.";
    return false;
  }
  base::CommandLine command_line(
      app_bundle_path->Append("Contents").Append("Helpers").Append("launcher"));
  command_line.AppendSwitch("--test");
  int exit = -1;
  const base::Process process = base::LaunchProcess(command_line, {});
  if (!process.IsValid() ||
      !process.WaitForExitWithTimeout(base::Seconds(10), &exit) || exit != 0) {
    VLOG(1) << "Launcher test run exited " << exit;
    return false;
  }
  return true;
}

}  // namespace

bool DoPlatformSpecificHealthChecks(UpdaterScope scope) {
  return CheckLauncherCanLaunchServer(scope);
}

}  // namespace updater
