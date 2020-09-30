// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_SETUP_SETUP_H_
#define CHROME_UPDATER_MAC_SETUP_SETUP_H_

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

// Failed to remove the active(unversioned) update service job from Launchd.
constexpr int kFailedToRemoveActiveUpdateServiceJobFromLaunchd = 20;

// Failed to remove versioned update service job from Launchd.
constexpr int kFailedToRemoveCandidateUpdateServiceJobFromLaunchd = 21;

// Failed to remove versioned control job from Launchd.
constexpr int kFailedToRemoveControlJobFromLaunchd = 22;

// Failed to remove versioned wake job from Launchd.
constexpr int kFailedToRemoveWakeJobFromLaunchd = 23;

// Failed to create the active(unversioned) update service Launchd plist.
constexpr int kFailedToCreateUpdateServiceLaunchdJobPlist = 30;

// Failed to create the versioned update service Launchd plist.
constexpr int kFailedToCreateVersionedUpdateServiceLaunchdJobPlist = 31;

// Failed to create the versioned control Launchd plist.
constexpr int kFailedToCreateControlLaunchdJobPlist = 32;

// Failed to create the versioned wake Launchd plist.
constexpr int kFailedToCreateWakeLaunchdJobPlist = 33;

// Failed to start the active(unversioned) update service job.
constexpr int kFailedToStartLaunchdActiveServiceJob = 40;

// Failed to start the versioned update service job.
constexpr int kFailedToStartLaunchdVersionedServiceJob = 41;

// Failed to start the control job.
constexpr int kFailedToStartLaunchdControlJob = 42;

// Failed to start the wake job.
constexpr int kFailedToStartLaunchdWakeJob = 43;

// Timed out while awaiting launchctl to become aware of the control job.
constexpr int kFailedAwaitingLaunchdControlJob = 44;

}  // namespace setup_exit_codes

// Sets up the candidate updater by copying the bundle, creating launchd plists
// for administration service and XPC service tasks, and start the corresponding
// launchd jobs.
int Setup();

// Uninstalls this version of the updater.
int UninstallCandidate();

// Sets up this version of the Updater as the active version.
int PromoteCandidate();

// Removes the launchd plists for scheduled tasks and xpc service. Deletes the
// updater bundle from its installed location.
int Uninstall(bool is_machine);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_SETUP_SETUP_H_
