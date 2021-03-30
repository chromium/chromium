// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_SETUP_SETUP_H_
#define CHROME_UPDATER_MAC_SETUP_SETUP_H_

#include "chrome/updater/updater_scope.h"

namespace updater {

namespace setup_exit_codes {

// Success.
constexpr int kSuccess = 0;

// Failed to copy the updater's bundle.
constexpr int kFailedToCopyBundle = 10;

// Failed to delete the updater's install folder.
constexpr int kFailedToDeleteFolder = 11;

// Failed to delete the updater's data folder.
constexpr int kFailedToDeleteDataFolder = 12;

// Failed to get versioned updater folder path.
constexpr int kFailedToGetVersionedUpdaterFolderPath = 13;

// Failed to remove the active(unversioned) update service job from Launchd.
constexpr int kFailedToRemoveActiveUpdateServiceJobFromLaunchd = 20;

// Failed to remove versioned update service job from Launchd.
constexpr int kFailedToRemoveCandidateUpdateServiceJobFromLaunchd = 21;

// Failed to remove versioned update service internal job from Launchd.
constexpr int kFailedToRemoveUpdateServiceInternalJobFromLaunchd = 22;

// Failed to remove versioned wake job from Launchd.
constexpr int kFailedToRemoveWakeJobFromLaunchd = 23;

// Failed to create the active(unversioned) update service Launchd plist.
constexpr int kFailedToCreateUpdateServiceLaunchdJobPlist = 30;

// Failed to create the versioned update service Launchd plist.
constexpr int kFailedToCreateVersionedUpdateServiceLaunchdJobPlist = 31;

// Failed to create the versioned update service internal Launchd plist.
constexpr int kFailedToCreateUpdateServiceInternalLaunchdJobPlist = 32;

// Failed to create the versioned wake Launchd plist.
constexpr int kFailedToCreateWakeLaunchdJobPlist = 33;

// Failed to start the active(unversioned) update service job.
constexpr int kFailedToStartLaunchdActiveServiceJob = 40;

// Failed to start the versioned update service job.
constexpr int kFailedToStartLaunchdVersionedServiceJob = 41;

// Failed to start the update service internal job.
constexpr int kFailedToStartLaunchdUpdateServiceInternalJob = 42;

// Failed to start the wake job.
constexpr int kFailedToStartLaunchdWakeJob = 43;

// Timed out while awaiting launchctl to become aware of the update service
// internal job.
constexpr int kFailedAwaitingLaunchdUpdateServiceInternalJob = 44;

}  // namespace setup_exit_codes

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

#endif  // CHROME_UPDATER_MAC_SETUP_SETUP_H_
