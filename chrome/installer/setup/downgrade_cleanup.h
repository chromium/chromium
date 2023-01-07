// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_DOWNGRADE_CLEANUP_H_
#define CHROME_INSTALLER_SETUP_DOWNGRADE_CLEANUP_H_

#include <string>

#include "chrome/installer/util/util_constants.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

class WorkItemList;

namespace installer {

class InstallerState;

// Cleans stale data after a downgrade to `version` if `revert` is false. If
// `revert` is true, revert a previous attempt to cleanup after a downgrade to
// `version`.
InstallStatus ProcessCleanupForDowngrade(const base::Version& version,
                                         bool revert);

// Returns the command line to cleanup after a downgrade. This command line has
// two placeholders, the first one for the version to which we are downgrading
// and the second one to decide if we are to do the cleanup or revert a previous
// cleanup. The second placeholder accepts "cleanup" or "revert" as value. The
// command will fail to run if either the version or the operation type is empty
// or invalid.
std::wstring GetDowngradeCleanupCommandWithPlaceholders(
    const base::FilePath& installer_path,
    const InstallerState& installer_state);

// Adds the work items to cleanup stale data in case the installation of
// `new_version` results in a downgrade that crosses a breaking installer
// version. If no cleanup is necessary, this does nothing. This returns `true`
// if items are actually added to `list` and `false` otherwise.
bool AddDowngradeCleanupItems(const base::Version& new_version,
                              WorkItemList* list);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_DOWNGRADE_CLEANUP_H_
