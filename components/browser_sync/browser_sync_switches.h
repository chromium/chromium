// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace switches {

// Enabled the local sync backend implemented by the LoopbackServer.
inline constexpr char kEnableLocalSyncBackend[] = "enable-local-sync-backend";

// Specifies the local sync backend directory. The name is chosen to mimic
// user-data-dir etc. This flag only matters if the enable-local-sync-backend
// flag is present.
inline constexpr char kLocalSyncBackendDir[] = "local-sync-backend-dir";

// Sync invalidation switches.
//
// Enables providing the list of FCM registration tokens in the commit request.
BASE_DECLARE_FEATURE(kSyncUseFCMRegistrationTokensList);
// Max size of FCM registration tokens list. If the number of active devices
// having FCM registration tokens is higher, then the resulting list will be
// empty meaning unknown FCM registration tokens.
inline constexpr base::FeatureParam<int> kSyncFCMRegistrationTokensListMaxSize{
    &kSyncUseFCMRegistrationTokensList, "SyncFCMRegistrationTokensListMaxSize",
    5};
// Enables filtering out inactive devices which haven't sent DeviceInfo update
// recently (depending on the device's pulse_interval and an additional margin).
BASE_DECLARE_FEATURE(kSyncFilterOutInactiveDevicesForSingleClient);
// An additional threshold to consider devices as active. It extends device's
// pulse interval to mitigate possible latency after DeviceInfo commit.
inline constexpr base::FeatureParam<base::TimeDelta> kSyncActiveDeviceMargin{
    &kSyncFilterOutInactiveDevicesForSingleClient, "SyncActiveDeviceMargin",
    base::Days(7)};

// If enabled, eligible users (i.e. those for which Sync-the-feature is active)
// are migrated, at browser startup, to the signed-in non-syncing state.
BASE_DECLARE_FEATURE(kMigrateSyncingUserToSignedIn);

// Feature parameter for kMigrateSyncingUserToSignedIn.
// Say the user has sync-the-feature enabled but is in TransportState::PAUSED
// due to a persistent auth error.
// - If kMigrateSyncingUserToSignedIn is on & kForceMigrateSyncingUserToSignedIn
//   is off, MaybeMigrateSyncingUserToSignedIn() will only proceed with the
//   migration if kMinDelayToMigrateSyncPaused has passed since the first
//   call, or if the error got resolved in that meantime.
// - If both flags are on, the migration runs on the first call to
//   MaybeMigrateSyncingUserToSignedIn() and this value is irrelevant.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kMinDelayToMigrateSyncPaused);

// If enabled, users who were migrated from syncing to signed-in via the above
// flag are migrated back into the syncing state.
BASE_DECLARE_FEATURE(kUndoMigrationOfSyncingUserToSignedIn);

// If enabled in addition to `kMigrateSyncingUserToSignedIn`, then all users
// with Sync-the-feature enabled are migrated, at browser startup, to the
// signed-in non-syncing state. I.e. this bypasses the "eligibility"
// requirements.
BASE_DECLARE_FEATURE(kForceMigrateSyncingUserToSignedIn);

}  // namespace switches

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_
