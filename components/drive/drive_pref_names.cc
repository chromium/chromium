// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/drive_pref_names.h"

namespace drive::prefs {

// A boolean pref to disable Google Drive integration.
// The pref prefix should remain as "gdata" for backward compatibility.
const char kDisableDrive[] = "gdata.disabled";

// A boolean pref to disable Drive over cellular connections.
// The pref prefix should remain as "gdata" for backward compatibility.
const char kDisableDriveOverCellular[] = "gdata.cellular.disabled";

// A boolean pref to enable or disable verbose logging in DriveFS.
const char kDriveFsEnableVerboseLogging[] = "drivefs.enable_verbose_logging";

// A string pref containing a random salt used to obfuscate account IDs
// when passed to drivefs.
const char kDriveFsProfileSalt[] = "drivefs.profile_salt";

// A boolean pref containing whether pinned files have been migrated to DriveFS.
const char kDriveFsPinnedMigrated[] = "drivefs.pinned_migrated";

// A boolean pref containing whether DriveFS was ever successfully launched.
const char kDriveFsWasLaunchedAtLeastOnce[] =
    "drivefs.was_launched_at_least_once";

// A boolean pref toggling MirrorSync functionality.
const char kDriveFsEnableMirrorSync[] = "drivefs.enable_mirror_sync";

// A string pref containing the machine ID that, when set, ensures existing
// MirrorSync Computers roots are reassociated to the current device.
const char kDriveFsMirrorSyncMachineRootId[] =
    "drivefs.mirror_sync_machine_root_id";

// A boolean pref that maintains whether the feature is enabled or disabled by
// the user.
const char kDriveFsBulkPinningEnabled[] = "drivefs.bulk_pinning_enabled";

// The maximum number of items that the bulk pinning feature will try to keep
// track of.
const char kDriveFsBulkPinningMaxQueueSize[] =
    "drivefs.bulk_pinning.max_queue_size";

}  // namespace drive::prefs
