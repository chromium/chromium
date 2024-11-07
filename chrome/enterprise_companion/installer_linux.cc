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

namespace enterprise_companion {

namespace {

bool InstallToDir(const base::FilePath& install_directory) {
  base::FilePath source_exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &source_exe_path)) {
    LOG(ERROR) << "Failed to retrieve the current executable's path.";
    return false;
  }

  base::FilePath dest_exe_path = install_directory.AppendASCII(kExecutableName);
  if (!base::CopyFile(source_exe_path, dest_exe_path)) {
    LOG(ERROR) << "Failed to copy the new executable to the install directory.";
    return false;
  }

  if (!base::SetPosixFilePermissions(install_directory,
                                     kInstallDirPermissionsMask)) {
    LOG(ERROR) << "Failed to set permissions to drwxr-xr-x at"
               << install_directory;
    return false;
  }

  if (!base::SetPosixFilePermissions(dest_exe_path,
                                     kInstallDirPermissionsMask)) {
    LOG(ERROR) << "Failed to set permissions to rwxr-xr-x at" << dest_exe_path;
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

  return InstallToDir(*install_directory);
}

}  // namespace enterprise_companion
