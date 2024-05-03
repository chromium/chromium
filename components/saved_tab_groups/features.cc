// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace tab_groups {
// Core feature flag for tab group sync on Android.
BASE_FEATURE(kTabGroupSyncAndroid,
             "TabGroupSyncAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupSyncForceOff,
             "TabGroupSyncForceOff",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabGroupStableIds,
             "AndroidTabGroupStableIds",
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

// Feature flag specific to UNO. Controls how we handle tab groups on sign-out
// and sync toggle. Can be defined independently for each platform.
BASE_FEATURE(kTabGroupSyncUno,
             "TabGroupSyncUno",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupsSaveV2Enabled() {
  return base::FeatureList::IsEnabled(kTabGroupsSaveV2);
}

bool IsTabGroupsSaveUIUpdateEnabled() {
  return base::FeatureList::IsEnabled(kTabGroupsSaveUIUpdate);
}

bool ShouldCloseAllTabGroupsOnSignOut() {
  return GetFieldTrialParamByFeatureAsBool(
      kTabGroupSyncUno, "close_all_tab_groups_on_sign_out", false);
}

}  // namespace tab_groups
