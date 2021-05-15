// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_MAC_UTIL_H_
#define CHROME_UPDATER_MAC_MAC_UTIL_H_

#include "base/mac/scoped_cftyperef.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace updater {

// For user installations returns: the "~/Library" for the logged in user.
// For system installations returns: "/Library".
absl::optional<base::FilePath> GetLibraryFolderPath(UpdaterScope scope);

// For user installations sets `path` to: the "~/Library/Application Support"
// for the logged in user. For system installations sets `path` to:
// "/Library/Application Support".
absl::optional<base::FilePath> GetApplicationSupportDirectory(
    UpdaterScope scope);

// For user installations:
// ~/Library/Google/GoogleUpdater
// For system installations:
// /Library/Google/GoogleUpdater
absl::optional<base::FilePath> GetUpdaterFolderPath(UpdaterScope scope);

// For user installations:
// ~/Library/Google/GoogleUpdater/88.0.4293.0
// For system installations:
// /Library/Google/GoogleUpdater/88.0.4293.0
absl::optional<base::FilePath> GetVersionedUpdaterFolderPathForVersion(
    UpdaterScope scope,
    const base::Version& version);

// The same as GetVersionedUpdaterFolderPathForVersion, where the version is
// kUpdaterVersion.
absl::optional<base::FilePath> GetVersionedUpdaterFolderPath(
    UpdaterScope scope);

// For user installations:
// ~/Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/MacOS/GoogleUpdater
// For system installations:
// /Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/MacOS/GoogleUpdater
absl::optional<base::FilePath> GetUpdaterExecutablePath(UpdaterScope scope);

// For user installations:
// ~/Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/MacOS
// For system installations:
// /Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/MacOS
absl::optional<base::FilePath> GetExecutableFolderPathForVersion(
    UpdaterScope scope,
    const base::Version& version);

// Removes the Launchd job with the given 'name'.
bool RemoveJobFromLaunchd(UpdaterScope scope,
                          Launchd::Domain domain,
                          Launchd::Type type,
                          base::ScopedCFTypeRef<CFStringRef> name);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_MAC_UTIL_H_
