// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_POSIX_UTIL_H_
#define CHROME_UPDATER_UTIL_POSIX_UTIL_H_

#include <optional>

namespace base {
class FilePath;
}

namespace updater {
enum class UpdaterScope;

// Recursively delete a folder and its contents, returning `true` on success.
bool DeleteFolder(const std::optional<base::FilePath>& installed_path);

// Delete this updater's versioned install folder.
bool DeleteCandidateInstallFolder(UpdaterScope scope);

std::optional<base::FilePath> GetUpdateServiceLauncherPath(UpdaterScope scope);

// Copy a directory, including symlinks.
bool CopyDir(const base::FilePath& from_path,
             const base::FilePath& to_path,
             bool world_readable);

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_POSIX_UTIL_H_
