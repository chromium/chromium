// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/installer.h"

#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "installer_posix.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace enterprise_companion {

namespace {

bool InstallToDir(const base::FilePath& install_directory) {
  base::FilePath source_exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &source_exe_path)) {
    VLOG(1) << "Failed to retrieve the current executable's path.";
    return false;
  }

  base::FilePath dest_exe_path = install_directory.Append(kExecutableName);
  base::FilePath backup_exe_path = dest_exe_path.AddExtension("old");
  if (base::PathExists(dest_exe_path) &&
      !base::CopyFile(dest_exe_path, backup_exe_path)) {
    VPLOG(1) << "Failed to backup existing installation";
    return false;
  }

  absl::Cleanup restore_backup = [&] {
    if (base::PathExists(backup_exe_path) &&
        !base::CopyFile(backup_exe_path, dest_exe_path)) {
      VPLOG(1) << "Failed to restore backup installation";
    }
  };
  absl::Cleanup delete_backup = [&] { base::DeleteFile(backup_exe_path); };

  if (!base::CopyFile(source_exe_path, dest_exe_path)) {
    VPLOG(1) << "Failed to copy the new executable to the install directory.";
    return false;
  }

  if (!base::SetPosixFilePermissions(install_directory,
                                     kInstallDirPermissionsMask)) {
    VPLOG(1) << "Failed to set permissions to drwxr-xr-x at"
             << install_directory;
    return false;
  }

  if (!base::SetPosixFilePermissions(dest_exe_path,
                                     kInstallDirPermissionsMask)) {
    VPLOG(1) << "Failed to set permissions to rwxr-xr-x at" << dest_exe_path;
    return false;
  }

  std::move(restore_backup).Cancel();
  return true;
}

}  // namespace

bool Install() {
  std::optional<base::FilePath> install_directory = GetInstallDirectory();
  if (!install_directory) {
    VLOG(1) << "Failed to get install directory";
    return false;
  }

  return InstallToDir(*install_directory);
}

}  // namespace enterprise_companion
