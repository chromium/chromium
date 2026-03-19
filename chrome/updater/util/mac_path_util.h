// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_MAC_PATH_UTIL_H_
#define CHROME_UPDATER_UTIL_MAC_PATH_UTIL_H_

#include <optional>

namespace base {
class FilePath;
}  // namespace base

namespace updater {

enum class UpdaterScope;

// For user installations returns: the "~/Library" for the logged in user.
// For system installations returns: "/Library".
std::optional<base::FilePath> GetLibraryFolderPath(UpdaterScope scope);

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_MAC_PATH_UTIL_H_
