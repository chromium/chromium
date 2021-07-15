// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/browser_sync_switches.h"

namespace switches {

// Disables syncing one or more sync data types that are on by default.
// See sync/base/model_type.h for possible types. Types
// should be comma separated, and follow the naming convention for string
// representation of model types, e.g.:
// --disable-synctypes='Typed URLs, Bookmarks, Autofill Profiles'
const char kDisableSyncTypes[] = "disable-sync-types";

// Enabled the local sync backend implemented by the LoopbackServer.
const char kEnableLocalSyncBackend[] = "enable-local-sync-backend";

// Specifies the local sync backend directory. The name is chosen to mimic
// user-data-dir etc. This flag only matters if the enable-local-sync-backend
// flag is present.
const char kLocalSyncBackendDir[] = "local-sync-backend-dir";

#if defined(OS_ANDROID)
const base::Feature kSyncUseSessionsUnregisterDelay{
    "SyncUseSessionsUnregisterDelay", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

// Enables providing the list of FCM registration tokens in the commit request.
const base::Feature kSyncUseFCMRegistrationTokensList{
    "SyncUseFCMRegistrationTokensList", base::FEATURE_ENABLED_BY_DEFAULT};

// Max size of FCM registration tokens list. If the number of active devices
// having FCM registration tokens is higher, then the resulting list will be
// empty meaning unknown FCM registration tokens.
const base::FeatureParam<int> kSyncFCMRegistrationTokensListMaxSize{
    &kSyncUseFCMRegistrationTokensList, "SyncFCMRegistrationTokensListMaxSize",
    5};

// Enables filtering out inactive devices which haven't sent DeviceInfo update
// recently (depending on the device's pulse_interval and an additional margin).
const base::Feature kSyncFilterOutInactiveDevicesForSingleClient{
    "SyncFilterOutInactiveDevicesForSingleClient",
    base::FEATURE_ENABLED_BY_DEFAULT};

// An additional threshold to consider devices as active. It extends device's
// pulse interval to mitigate possible latency after DeviceInfo commit.
const base::FeatureParam<base::TimeDelta> kSyncActiveDeviceMargin{
    &kSyncFilterOutInactiveDevicesForSingleClient, "SyncActiveDeviceMargin",
    base::TimeDelta::FromMinutes(30)};

}  // namespace switches
