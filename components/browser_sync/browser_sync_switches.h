// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace switches {

extern const char kDisableSyncTypes[];
extern const char kEnableLocalSyncBackend[];
extern const char kLocalSyncBackendDir[];

#if defined(OS_ANDROID)
extern const base::Feature kSyncUseSessionsUnregisterDelay;
#endif

extern const base::Feature kSyncFilterOutInactiveDevicesForSingleClient;
extern const base::FeatureParam<base::TimeDelta> kSyncActiveDeviceMargin;
extern const base::Feature kSyncUseFCMRegistrationTokensList;
extern const base::FeatureParam<int> kSyncFCMRegistrationTokensListMaxSize;

}  // namespace switches

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_
