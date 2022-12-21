// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_MAC_UTIL_H_
#define CHROME_UPDATER_UTIL_MAC_UTIL_H_

#include "base/mac/scoped_cftyperef.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace updater {

// For user installations returns: the "~/Library" for the logged in user.
// For system installations returns: "/Library".
absl::optional<base::FilePath> GetLibraryFolderPath(UpdaterScope scope);

// For user installations returns "~/Library/Application Support" for the
// logged in user. For system installations returns
// "/Library/Application Support".
absl::optional<base::FilePath> GetApplicationSupportDirectory(
    UpdaterScope scope);

// Returns the path to ksadmin, if it is present on the system. Ksadmin may be
// the shim installed by this updater or a Keystone ksadmin.
absl::optional<base::FilePath> GetKSAdminPath(UpdaterScope scope);

base::ScopedCFTypeRef<CFStringRef> CopyWakeLaunchdName(UpdaterScope scope);

// Removes the Launchd job with the given 'name'.
bool RemoveJobFromLaunchd(UpdaterScope scope,
                          Launchd::Domain domain,
                          Launchd::Type type,
                          base::ScopedCFTypeRef<CFStringRef> name);

// Recursively remove quarantine attributes on the path.
bool RemoveQuarantineAttributes(const base::FilePath& path);

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_MAC_UTIL_H_
