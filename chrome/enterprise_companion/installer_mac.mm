// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/installer.h"

#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/time/time.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/enterprise_companion_version.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/enterprise_companion/installer_posix.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace enterprise_companion {

namespace {

constexpr base::TimeDelta kKSAdminTimeout = base::Minutes(5);

// Register the installation with ksadmin.
bool RegisterInstallation(const base::FilePath& install_directory) {
  base::FilePath ksadmin_path = GetKSAdminPath();
  if (!base::PathExists(ksadmin_path)) {
    LOG(ERROR) << "Could not locate ksadmin.";
    return false;
  }

  base::CommandLine command_line(ksadmin_path);
  command_line.AppendArg("-r");
  command_line.AppendArg("-P");
  command_line.AppendArg(ENTERPRISE_COMPANION_APPID);
  command_line.AppendArg("-v");
  command_line.AppendArg(kEnterpriseCompanionVersion);
  command_line.AppendArg("-x");
  command_line.AppendArgPath(install_directory);
  command_line.AppendArg("-S");

  int exit_code = -1;
  base::Process process = base::LaunchProcess(command_line, {});
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch ksadmin";
    return false;
  } else if (!process.WaitForExitWithTimeout(kKSAdminTimeout, &exit_code)) {
    LOG(ERROR) << "Failed to wait for ksadmin to register the installation.";
    return false;
  } else if (exit_code != 0) {
    LOG(ERROR) << "Failed to register the installation with ksadmin. "
               << "Recieved exit code " << exit_code;
    return false;
  }

  return true;
}

}  // namespace

bool Install() {
  std::optional<base::FilePath> install_directory = GetInstallDirectory();
  if (!install_directory) {
    LOG(ERROR) << "Failed to get install directory";
    return false;
  }

  base::FilePath exe_path = install_directory->AppendASCII(kExecutableName);
  base::FilePath backup_exe = exe_path.AddExtensionASCII("old");
  if (base::PathExists(exe_path) && !base::CopyFile(exe_path, backup_exe)) {
    LOG(ERROR) << "Failed to backup existing installation.";
    return false;
  }
  absl::Cleanup delete_backup_exe = [&] { base::DeleteFile(backup_exe); };

  if (!InstallToDir(*install_directory)) {
    return false;
  }

  if (!RegisterInstallation(*install_directory)) {
    if (base::PathExists(backup_exe)) {
      if (!base::Move(backup_exe, exe_path)) {
        LOG(ERROR) << "Failed to restore installation backup.";
      }
    } else {
      if (!base::DeleteFile(exe_path)) {
        LOG(ERROR) << "Failed to clean installation after failure";
      }
    }
    return false;
  }

  return true;
}

}  // namespace enterprise_companion
