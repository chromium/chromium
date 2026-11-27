// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/path_util.h"

#include <string_view>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "storage/browser/file_system/external_mount_points.h"

namespace file_manager::util {
namespace {

using base::FilePath;

// By default, in ChromeOS it uses the $profile_dir/MyFiles however,
// for manual tests/development in linux-chromeos it uses $HOME/Downloads for
// the primary user.
// The flag `--use-myfiles-in-user-data-dir-for-testing` forces the ChromeOS
// pattern, even for non-ChromeOS environments. It is used in browser tests.
bool ShouldMountPrimaryUserDownloads(const user_manager::User& user) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kUseMyFilesInUserDataDirForTesting)) {
    return false;
  }

  if (base::SysInfo::IsRunningOnChromeOS()) {
    return false;
  }

  const session_manager::SessionManager* const session_manager =
      session_manager::SessionManager::Get();
  if (!session_manager) {
    return false;
  }
  const session_manager::Session* const primary_session =
      session_manager->GetPrimarySession();
  return primary_session &&
         user.GetAccountId() == primary_session->account_id();
}

// Returns the path to the `user`'s data directory.
// The user's home directory must be mounted.
FilePath GetUserDataDirectory(const user_manager::User& user) {
  std::optional<FilePath> path = user.GetPath();
  CHECK(path) << "User's home directory is not yet mounted";
  return *std::move(path);
}

// Returns the mount point name for `folder_name` of the `user`.
std::string GetMountPointName(const user_manager::User& user,
                              std::string_view folder_name) {
  // To distinguish profiles in multi-profile session, we append user name hash
  // to folder_name.
  return base::EscapeQueryParamValue(
      base::StrCat({folder_name, "-", user.username_hash()}), false);
}

}  // namespace

const FilePath& GetDefaultDownloadDirectoryForLinuxChromeOS() {
  CHECK(!base::SysInfo::IsRunningOnChromeOS());
  static const base::NoDestructor<FilePath> path([] {
    // The same as chrome::DIR_DEFAULT_DOWNLOADS on Linux.
    FilePath downloads =
        base::nix::GetXDGUserDirectory("DOWNLOAD", kFolderNameDownloads);

    // Avoid $HOME and the desktop directory, similar to
    // DownloadPrefs::GetDefaultDownloadDirectory().
    const FilePath home = base::PathService::CheckedGet(base::DIR_HOME);
    const FilePath desktop =
        base::PathService::CheckedGet(base::DIR_USER_DESKTOP);
    if (downloads == home || downloads == desktop) {
      // The same as chrome::DIR_DEFAULT_DOWNLOADS_SAFE on Linux.
      downloads = home.AppendASCII(kFolderNameDownloads);
    }
    return downloads;
  }());
  return *path;
}

FilePath GetDownloadsFolder(const user_manager::User& user) {
  if (std::optional<FilePath> path = GetOverriddenDownloadsFolder(user)) {
    return *std::move(path);
  }

  // Return <cryptohome>/MyFiles/Downloads.
  return GetUserDataDirectory(user)
      .AppendASCII(kFolderNameMyFiles)
      .AppendASCII(kFolderNameDownloads);
}

FilePath GetMyFilesFolder(const user_manager::User& user) {
  if (std::optional<FilePath> path = GetOverriddenMyFilesFolder(user)) {
    return *std::move(path);
  }

  // Return <cryptohome>/MyFiles.
  return GetUserDataDirectory(user).AppendASCII(kFolderNameMyFiles);
}

std::string GetDownloadsMountPointName(const user_manager::User& user) {
  return GetMountPointName(user, kFolderNameDownloads);
}

std::string GetShareCacheMountPointName(const user_manager::User& user) {
  return GetMountPointName(user, kFolderNameShareCache);
}

std::optional<FilePath> GetOverriddenMyFilesFolder(
    const user_manager::User& user) {
  // Check if FilesApp has a registered path already. This happens for tests.
  FilePath path;
  if (storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          GetDownloadsMountPointName(user), &path)) {
    return path;
  }

  // Return $HOME/Downloads as MyFiles folder.
  if (ShouldMountPrimaryUserDownloads(user)) {
    return GetDefaultDownloadDirectoryForLinuxChromeOS();
  }

  return std::nullopt;
}

std::optional<FilePath> GetOverriddenDownloadsFolder(
    const user_manager::User& user) {
  // Check if FilesApp has a registered path already.  This happens for tests.
  FilePath path;
  if (storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          GetDownloadsMountPointName(user), &path)) {
    return path.AppendASCII(kFolderNameDownloads);
  }

  // Return $HOME/Downloads as Download folder.
  if (ShouldMountPrimaryUserDownloads(user)) {
    return GetDefaultDownloadDirectoryForLinuxChromeOS();
  }

  return std::nullopt;
}

}  // namespace file_manager::util
