// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of preferences related to Google Drive.

#ifndef COMPONENTS_DRIVE_DRIVE_PREF_NAMES_H_
#define COMPONENTS_DRIVE_DRIVE_PREF_NAMES_H_

namespace drive::prefs {

// A boolean pref to disable Google Drive integration.
// The pref prefix should remain as "gdata" for backward compatibility.
inline constexpr char kDisableDrive[] = "gdata.disabled";

// A boolean pref to disable Drive over cellular or metered connections.
// The pref prefix should remain as "gdata" for backward compatibility.
inline constexpr char kDisableDriveOverCellular[] = "gdata.cellular.disabled";

// A boolean pref to enable or disable verbose logging in DriveFS.
inline constexpr char kDriveFsEnableVerboseLogging[] =
    "drivefs.enable_verbose_logging";

// A string pref containing a random salt used to obfuscate account IDs
// when passed to drivefs.
inline constexpr char kDriveFsProfileSalt[] = "drivefs.profile_salt";

// A boolean pref containing whether pinned files have been migrated to DriveFS.
inline constexpr char kDriveFsPinnedMigrated[] = "drivefs.pinned_migrated";

// A boolean pref containing whether DriveFS was ever successfully launched.
inline constexpr char kDriveFsWasLaunchedAtLeastOnce[] =
    "drivefs.was_launched_at_least_once";

// A boolean pref toggling MirrorSync functionality.
inline constexpr char kDriveFsEnableMirrorSync[] = "drivefs.enable_mirror_sync";

// A string pref containing the machine ID that, when set, ensures existing
// MirrorSync Computers roots are reassociated to the current device.
inline constexpr char kDriveFsMirrorSyncMachineRootId[] =
    "drivefs.mirror_sync_machine_root_id";

// A boolean pref indicating whether the DriveFS bulk-pinning feature is visible
// in Files App and Settings page. If the bulk-pinning feature is visible, then
// it can be enabled by the user.
inline constexpr char kDriveFsBulkPinningVisible[] =
    "drivefs.bulk_pinning.visible";

// A boolean pref indicating whether the DriveFS bulk-pinning feature is enabled
// or disabled by the user.
inline constexpr char kDriveFsBulkPinningEnabled[] =
    "drivefs.bulk_pinning_enabled";

// A time pref indicating the last time the DSS availability metric was emitted.
inline constexpr char kDriveFsDSSAvailabilityLastEmitted[] =
    "drivefs.dss_availability_last_emitted_time";

}  // namespace drive::prefs

#endif  // COMPONENTS_DRIVE_DRIVE_PREF_NAMES_H_
