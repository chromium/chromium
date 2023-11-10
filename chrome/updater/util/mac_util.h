// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_MAC_UTIL_H_
#define CHROME_UPDATER_UTIL_MAC_UTIL_H_

#include <optional>
#include <string>

#include "chrome/updater/updater_scope.h"

namespace base {
class FilePath;
}  // namespace base

namespace updater {

// For user installations returns: the "~/Library" for the logged in user.
// For system installations returns: "/Library".
std::optional<base::FilePath> GetLibraryFolderPath(UpdaterScope scope);

// For user installations returns "~/Library/Application Support" for the
// logged in user. For system installations returns
// "/Library/Application Support".
std::optional<base::FilePath> GetApplicationSupportDirectory(
    UpdaterScope scope);

// Returns the path to Keystone's root directory.
std::optional<base::FilePath> GetKeystoneFolderPath(UpdaterScope scope);

// Returns the path to ksadmin, if it is present on the system. Ksadmin may be
// the shim installed by this updater or a Keystone ksadmin.
std::optional<base::FilePath> GetKSAdminPath(UpdaterScope scope);

// Returns the path to the wake task plist.
std::optional<base::FilePath> GetWakeTaskPlistPath(UpdaterScope scope);

std::string GetWakeLaunchdName(UpdaterScope scope);

// Removes the wake launch job.
bool RemoveWakeJobFromLaunchd(UpdaterScope scope);

// Recursively remove quarantine attributes on the path.
bool RemoveQuarantineAttributes(const base::FilePath& path);

std::string GetDomain(UpdaterScope scope);

// Reads the value associated with `key` from the plist at `path`. Returns
// nullopt if `path` or `key` are empty, if the plist does not contain `key`, or
// if there are any errors.
std::optional<std::string> ReadValueFromPlist(const base::FilePath& path,
                                              const std::string& key);

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_MAC_UTIL_H_
