// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNC_BASE_SWITCHES_H_
#define COMPONENTS_SYNC_BASE_SYNC_BASE_SWITCHES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace switches {

extern const char kSyncServiceURL[];
extern const char kTrustedVaultServiceURL[];

extern const base::Feature kSyncNigoriRemoveMetadataOnCacheGuidMismatch;
extern const base::Feature kSyncForceDisableScryptForCustomPassphrase;
extern const base::Feature kSyncE2ELatencyMeasurement;
extern const base::Feature kSyncCustomSharingMessageNudgeDelay;
extern const base::FeatureParam<int> kSyncSharingMessageNudgeDelayMilliseconds;

extern const base::Feature kSyncResetEntitiesStateOnCommitFailure;

}  // namespace switches

#endif  // COMPONENTS_SYNC_BASE_SYNC_BASE_SWITCHES_H_
