// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/posix/setup.h"

#include <stdio.h>
#include <unistd.h>

#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/linux/ipc_constants.h"
#include "chrome/updater/linux/systemd_util.h"
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"

namespace updater {

int Setup(UpdaterScope scope) {
  VLOG(1) << base::CommandLine::ForCurrentProcess()->GetCommandLineString()
          << " : " << __func__;
  std::optional<base::FilePath> dest_path = GetVersionedInstallDirectory(scope);

  if (!dest_path) {
    return kErrorFailedToGetVersionedInstallDirectory;
  }

  if (base::PathExists(*dest_path)) {
    if (!DeleteExcept(dest_path->Append("Crashpad"))) {
      LOG(ERROR) << "Could not remove existing copy of this updater.";
      return kErrorFailedToDeleteFolder;
    }
  }

  dest_path = dest_path->Append(GetExecutableRelativePath());

  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path)) {
    return kErrorPathServiceFailed;
  }

  if (!base::CopyFile(exe_path, dest_path.value())) {
    return kErrorFailedToCopyBinary;
  }

  // rwx------ for user installs, rwxr-xr-x for system installs.
  int permissions_mask = base::FILE_PERMISSION_USER_MASK;
  if (IsSystemInstall(scope)) {
    permissions_mask |= base::FILE_PERMISSION_READ_BY_GROUP |
                        base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                        base::FILE_PERMISSION_READ_BY_OTHERS |
                        base::FILE_PERMISSION_EXECUTE_BY_OTHERS;
  }
  if (!base::SetPosixFilePermissions(dest_path.value(), permissions_mask)) {
    return kErrorFailedToCopyBinary;
  }

#ifdef INCLUDE_ENTERPRISE_COMPANION_IN_INSTALLER
  // Copy the adjacent enterprise companion app at best effort.
  base::FilePath enterprise_companion_src_path = exe_path.DirName().AppendASCII(
      base::StrCat({kCompanionAppExecutableName, kExecutableSuffix}));
  std::optional<base::FilePath> enterprise_companion_dest_path =
      GetBundledEnterpriseCompanionExecutablePath(scope);
  if (!enterprise_companion_dest_path) {
    VLOG(1) << "Failed to get the bundled enterprise companion path.";
  } else if (!base::CopyFile(enterprise_companion_src_path,
                             *enterprise_companion_dest_path) ||
             !base::SetPosixFilePermissions(*enterprise_companion_dest_path,
                                            permissions_mask)) {
    VLOG(1) << "Failed to copy " << enterprise_companion_src_path << " to "
            << enterprise_companion_dest_path;
  }
#endif  // INCLUDE_ENTERPRISE_COMPANION_IN_INSTALLER

  return kErrorOk;
}

int UninstallCandidate(UpdaterScope scope) {
  VLOG(1) << base::CommandLine::ForCurrentProcess()->GetCommandLineString()
          << " : " << __func__;
  int error = kErrorOk;
  if (!DeleteCandidateInstallFolder(scope)) {
    VLOG(1) << "Failed to delete versioned folder.";
    error = kErrorFailedToDeleteFolder;
  }

  std::optional<base::FilePath> versioned_socket =
      GetActiveDutyInternalSocketPath(scope);
  if (!versioned_socket || !base::DeleteFile(versioned_socket.value())) {
    VLOG(1) << "Failed to delete versioned socket file.";
    error = kErrorFailedToDeleteSocket;
  }

  return error;
}

int PromoteCandidate(UpdaterScope scope) {
  // Create a hard link in the base install directory to this updater.
  std::optional<base::FilePath> launcher_path =
      GetUpdateServiceLauncherPath(scope);
  base::FilePath updater_executable;

  if (!launcher_path ||
      !base::PathService::Get(base::FILE_EXE, &updater_executable)) {
    return kErrorFailedToGetVersionedInstallDirectory;
  }

  base::FilePath tmp_launcher_name =
      launcher_path->DirName().AppendASCII("launcher_new");
  if (link(updater_executable.value().c_str(),
           tmp_launcher_name.value().c_str())) {
    return kErrorFailedToLinkCurrent;
  }
  if (rename(tmp_launcher_name.value().c_str(),
             launcher_path->value().c_str())) {
    return kErrorFailedToRenameCurrent;
  }

  if (!InstallSystemdUnits(scope)) {
    return kErrorFailedToInstallSystemdUnit;
  }

  return kErrorOk;
}

int Uninstall(UpdaterScope scope) {
  VLOG(1) << base::CommandLine::ForCurrentProcess()->GetCommandLineString()
          << " : " << __func__;
  int error = kErrorOk;

  if (!UninstallSystemdUnits(scope)) {
    error = kErrorFailedToRemoveSystemdUnit;
  }

  if (!DeleteFolder(GetInstallDirectory(scope)) ||
      !DeleteFolder(GetInstallDirectory(scope))) {
    error = kErrorFailedToDeleteFolder;
  }

  return error;
}
}  // namespace updater
