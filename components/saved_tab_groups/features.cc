// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/features.h"

namespace tab_groups {

// Core feature flag for tab group sync on Android.
BASE_FEATURE(kTabGroupSyncAndroid,
             "TabGroupSyncAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Builds off of the original TabGroupsSave feature by making some UI tweaks and
// adjustments. This flag controls the v2 update of sync, restore, dialog
// triggering, extension support etc. b/325123353
BASE_FEATURE(kTabGroupsSaveV2,
             "TabGroupsSaveV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This flag controls the UI update made to saved tab groups as well as model
// and sync support for pinning saved tab groups.
BASE_FEATURE(kTabGroupsSaveUIUpdate,
             "TabGroupsSaveUIUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupsSaveV2Enabled() {
  return base::FeatureList::IsEnabled(kTabGroupsSaveV2);
}

bool IsTabGroupsSaveUIUpdateEnabled() {
  return base::FeatureList::IsEnabled(kTabGroupsSaveUIUpdate);
}

}  // namespace tab_groups
