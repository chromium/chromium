// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_FEATURES_H_
#define COMPONENTS_SYNC_PREFERENCES_FEATURES_H_

#include "base/feature_list.h"

namespace sync_preferences::features {

// If enabled, supports account-scoping of preferences. That is, the values of
// the specially tagged preferences will be cleared upon sign out.
BASE_DECLARE_FEATURE(kAccountScopedPrefs);

// Enables the CrossDevicePrefTracker, a KeyedService for tracking select
// non-syncing Prefs across a user's devices.
BASE_DECLARE_FEATURE(kEnableCrossDevicePrefTracker);

// Enables additional logging for the CrossDevicePrefTracker and related code.
//
// TODO(crbug.com/485956752): Remove this flag and all associated log statements
// once `CrossDevicePrefTracker` debugging is complete and the feature is
// stable.
BASE_DECLARE_FEATURE(kCrossDevicePrefTrackerExtraLogs);

}  // namespace sync_preferences::features

#endif  // COMPONENTS_SYNC_PREFERENCES_FEATURES_H_
