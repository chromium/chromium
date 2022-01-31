// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_SWITCHES_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_SWITCHES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace switches {

constexpr inline base::Feature kSyncResetPollIntervalOnStart{
    "SyncResetPollIntervalOnStart", base::FEATURE_DISABLED_BY_DEFAULT};

// Causes Sync to ignore updates encrypted with keys that have been missing for
// too long from this client; Sync will proceed normally as if those updates
// didn't exist.
constexpr inline base::Feature kIgnoreSyncEncryptionKeysLongMissing{
    "IgnoreSyncEncryptionKeysLongMissing", base::FEATURE_DISABLED_BY_DEFAULT};

// The threshold for kIgnoreSyncEncryptionKeysLongMissing to start ignoring keys
// (measured in number of GetUpdatesResponses messages).
constexpr inline base::FeatureParam<int> kMinGuResponsesToIgnoreKey{
    &kIgnoreSyncEncryptionKeysLongMissing, "MinGuResponsesToIgnoreKey", 50};

// Causes the sync engine to count a quota for commits of data types that can
// be committed by extension JS API. If the quota is depleted, an extra long
// nudge delay is applied to that data type. As a result, more changes are
// likely to get combined into one commit message.
constexpr inline base::Feature kSyncExtensionTypesThrottling{
    "SyncExtensionTypesThrottling", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_SWITCHES_H_
