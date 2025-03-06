// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/installer.h"

#include <optional>

#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/enterprise_companion_version.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/enterprise_companion/installer_posix.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if defined(ADDRESS_SANITIZER)
#include "base/base_paths.h"
#include "base/path_service.h"
#endif

namespace enterprise_companion {

namespace {

#if defined(ADDRESS_SANITIZER)
constexpr char kAsanDylibFilename[] = "libclang_rt.asan_osx_dynamic.dylib";
#endif

constexpr base::TimeDelta kKSAdminTimeout = base::Minutes(5);

// Register the installation with ksadmin.
bool RegisterInstallation(const base::FilePath& install_directory) {
  base::FilePath ksadmin_path = GetKSAdminPath();
  if (!base::PathExists(ksadmin_path)) {
    VLOG(1) << "Could not locate ksadmin.";
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
    VLOG(1) << "Failed to launch ksadmin";
    return false;
  } else if (!process.WaitForExitWithTimeout(kKSAdminTimeout, &exit_code)) {
    VLOG(1) << "Failed to wait for ksadmin to register the installation.";
    return false;
  } else if (exit_code != 0) {
    VLOG(1) << "Failed to register the installation with ksadmin. "
            << "Recieved exit code " << exit_code;
    return false;
  }

  return true;
}

bool InstallToDir(const base::FilePath& install_directory) {
  base::FilePath source_exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &source_exe_path)) {
    VLOG(1) << "Failed to retrieve the current executable's path.";
    return false;
  }

  const base::FilePath source_app_bundle_path =
      base::apple::GetInnermostAppBundlePath(source_exe_path);
  if (source_app_bundle_path.empty()) {
    VLOG(1) << "Failed to determine the path to the app bundle containing "
            << source_exe_path
            << ". The installer must be run from within an application bundle.";
    return false;
  }

  const base::FilePath dest_app_bundle_path =
      install_directory.Append(base::StrCat({PRODUCT_FULLNAME_STRING, ".app"}));
  if (!base::DeletePathRecursively(dest_app_bundle_path)) {
    VLOG(1) << "Failed to delete " << dest_app_bundle_path;
    return false;
  }

  if (!base::CopyDirectory(source_app_bundle_path, dest_app_bundle_path,
                           /*recursive=*/true)) {
    VLOG(1)
        << "Failed to copy the application bundle to the install directory.";
    return false;
  }

  if (!base::SetPosixFilePermissions(install_directory,
                                     kInstallDirPermissionsMask)) {
    VLOG(1) << "Failed to set permissions to drwxr-xr-x at"
            << install_directory;
    return false;
  }

  return true;
}

}  // namespace

bool Install() {
  std::optional<base::FilePath> install_directory = GetInstallDirectory();
  if (!install_directory) {
    VLOG(1) << "Failed to get install directory";
    return false;
  }

  base::FilePath app_path = install_directory->Append(
      base::StrCat({PRODUCT_FULLNAME_STRING, ".app"}));
  base::FilePath backup_path = app_path.AddExtension("old");
  if (base::PathExists(app_path) &&
      !base::CopyDirectory(app_path, backup_path, /*recursive=*/true)) {
    VLOG(1) << "Failed to backup existing installation.";
    return false;
  }
  absl::Cleanup delete_backup = [&] {
    base::DeletePathRecursively(backup_path);
  };

  if (!InstallToDir(*install_directory)) {
    return false;
  }

#if defined(ADDRESS_SANITIZER)
  base::FilePath dir_exe;
  if (!base::PathService::Get(base::DIR_EXE, &dir_exe)) {
    VLOG(1) << "Failed to get the current executable's directory.";
    return false;
  }

  base::FilePath asan_dylib_path = dir_exe.Append(kAsanDylibFilename);
  if (base::PathExists(asan_dylib_path) &&
      !base::CopyFile(asan_dylib_path,
                      install_directory->Append(asan_dylib_path.BaseName()))) {
    VLOG(1) << "Failed to copy " << asan_dylib_path << " to "
            << *install_directory;
    return false;
  }
#endif

  if (!RegisterInstallation(*install_directory)) {
    if (base::PathExists(backup_path)) {
      if (!base::DeletePathRecursively(app_path)) {
        VLOG(1) << "Failed to delete " << app_path
                << " while trying to restore backup.";
      } else if (!base::Move(backup_path, app_path)) {
        VLOG(1) << "Failed to restore installation backup.";
      }
    } else {
      if (!base::DeletePathRecursively(app_path)) {
        VLOG(1) << "Failed to clean installation after failure";
      }
    }
    return false;
  }

  return true;
}

}  // namespace enterprise_companion
