// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POSIX_SETUP_H_
#define CHROME_UPDATER_POSIX_SETUP_H_

#include "chrome/updater/updater_scope.h"

namespace updater {

// Sets up the candidate updater by copying the bundle, creating launchd plists
// for administration service and XPC service tasks, and start the corresponding
// launchd jobs.
int Setup(UpdaterScope scope);

// Uninstalls this version of the updater.
int UninstallCandidate(UpdaterScope scope);

// Sets up this version of the Updater as the active version.
int PromoteCandidate(UpdaterScope scope);

// Removes the launchd plists for scheduled tasks and xpc service. Deletes the
// updater bundle from its installed location.
int Uninstall(UpdaterScope scope);

}  // namespace updater

#endif  // CHROME_UPDATER_POSIX_SETUP_H_
