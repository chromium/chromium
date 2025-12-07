// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_FEATURES_H_
#define COMPONENTS_SYNC_PREFERENCES_FEATURES_H_

#include "base/feature_list.h"

namespace sync_preferences::features {

// Enables the CrossDevicePrefTracker, a KeyedService for tracking select
// non-syncing Prefs across a user's devices.
BASE_DECLARE_FEATURE(kEnableCrossDevicePrefTracker);

}  // namespace sync_preferences::features

#endif  // COMPONENTS_SYNC_PREFERENCES_FEATURES_H_
