// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/browser_sync_switches.h"

#include "base/feature_list.h"

namespace switches {

BASE_FEATURE(kSyncUseFCMRegistrationTokensList,
             "SyncUseFCMRegistrationTokensList",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncFilterOutInactiveDevicesForSingleClient,
             "SyncFilterOutInactiveDevicesForSingleClient",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMigrateSyncingUserToSignedIn,
             "MigrateSyncingUserToSignedIn",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUndoMigrationOfSyncingUserToSignedIn,
             "UndoMigrationOfSyncingUserToSignedIn",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace switches
