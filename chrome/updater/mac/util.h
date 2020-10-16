// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_UTIL_H_
#define CHROME_UPDATER_MAC_UTIL_H_

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace updater {

bool IsSystemInstall();

// For user installations returns: the "~/Library" for the logged in user.
// For system installations returns: "/Library".
base::FilePath GetLibraryFolderPath();

// For user installations:
// ~/Library/Google/GoogleUpdater
// For system installations:
// /Library/Google/GoogleUpdater
base::FilePath GetUpdaterFolderPath();

// For user installations:
// ~/Library/Google/GoogleUpdater/88.0.4293.0
// For system installations:
// /Library/Google/GoogleUpdater/88.0.4293.0
base::FilePath GetVersionedUpdaterFolderPathForVersion(
    const base::Version& version);

// The same as GetVersionedUpdaterFolderPathForVersion, where the version is
// UPDATER_VERSION_STRING.
base::FilePath GetVersionedUpdaterFolderPath();

// For user installations:
// ~/Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/MacOS/GoogleUpdater
// For system installations:
// /Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/MacOS/GoogleUpdater
base::FilePath GetUpdaterExecutablePath();

// For user installations:
// ~/Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/MacOS
// For system installations:
// /Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/MacOS
base::FilePath GetExecutableFolderPathForVersion(const base::Version& version);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_UTIL_H_
