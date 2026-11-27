// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_PATH_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_PATH_UTIL_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace user_manager {
class User;
}  // namespace user_manager

namespace file_manager::util {

// Folder names in the user's home directory.
inline constexpr char kFolderNameDownloads[] = "Downloads";
inline constexpr char kFolderNameMyFiles[] = "MyFiles";
inline constexpr char kFolderNameShareCache[] = "ShareCache";

// Returns the default downloads directory used on linux-chromeos, i.e. when
// not running on a ChromeOS device. There, this is used as the MyFiles and
// Downloads folder of the primary user, unless
// --use-myfiles-in-user-data-dir-for-testing is specified.
// This is computed from the XDG user directory and base::DIR_HOME, and should
// be the same as DownloadPrefs::GetDefaultDownloadDirectory().
COMPONENT_EXPORT(FILE_MANAGER_PATH_UTIL)
const base::FilePath& GetDefaultDownloadDirectoryForLinuxChromeOS();

// Gets the absolute path for the 'Downloads' folder for the `user`.
// The `user`'s home directory must be mounted, i.e., `user.username_hash()`
// must not be empty.
COMPONENT_EXPORT(FILE_MANAGER_PATH_UTIL)
base::FilePath GetDownloadsFolder(const user_manager::User& user);

// Gets the absolute path for the 'MyFiles' folder for the `user`.
// The `user`'s home directory must be mounted, i.e., `user.username_hash()`
// must not be empty.
COMPONENT_EXPORT(FILE_MANAGER_PATH_UTIL)
base::FilePath GetMyFilesFolder(const user_manager::User& user);

// The canonical mount point name for "Downloads" folder of the `user`.
COMPONENT_EXPORT(FILE_MANAGER_PATH_UTIL)
std::string GetDownloadsMountPointName(const user_manager::User& user);

// The canonical mount point name for "ShareCache" folder of the `user`.
COMPONENT_EXPORT(FILE_MANAGER_PATH_UTIL)
std::string GetShareCacheMountPointName(const user_manager::User& user);

// The functions below are only for the deprecated Profile* variants in
// chrome/browser/ash/file_manager/path_util.h, which still need to handle
// Profiles whose user's home directory is not mounted, in tests. Do not use
// them in new code.
// TODO(crbug.com/579585875): Remove them once the callers of the Profile*
// variants are migrated to the User variants.

// Returns the overridden path for the "MyFiles" folder of the `user`, if any.
// Otherwise returns std::nullopt.
COMPONENT_EXPORT(FILE_MANAGER_PATH_UTIL)
std::optional<base::FilePath> GetOverriddenMyFilesFolder(
    const user_manager::User& user);

// Returns the overridden path for the "Downloads" folder of the `user`, if
// any. Otherwise returns std::nullopt.
COMPONENT_EXPORT(FILE_MANAGER_PATH_UTIL)
std::optional<base::FilePath> GetOverriddenDownloadsFolder(
    const user_manager::User& user);

}  // namespace file_manager::util

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_PATH_UTIL_H_
