// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/data_sharing/public/features.h"

namespace tab_groups {

// The default time interval to clean up a hidden tab group.
const int kDefaultGroupCleanUpTimeInternalInSeconds = 60 * 60;

// Finch parameter key value for the group clean up time interval in seconds.
constexpr char kGroupCleanUpTimeIntervalInSecondsFinchKey[] =
    "group_clean_up_time_internal_seconds";

// Feature flag for Java controller layer migration to use TabGroupSyncDelegate.
// Noop when disabled.
BASE_FEATURE(kTabGroupSyncDelegateAndroid, base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag to restrict download on synced tabs if the navigation is
// triggered without attention..
BASE_FEATURE(kRestrictDownloadOnSyncedTabs, base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag to determine whether an alternate illustration should be used on
// the history sync consent screen. This feature should be used independent of
// any other features in this file.
BASE_FEATURE(kUseAlternateHistorySyncIllustration,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Force remove all closed tab groups from the sync local DB on startup if this
// feature flag is enabled.
BASE_FEATURE(kForceRemoveClosedTabGroupsOnStartup,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables checking for URLs before syncing them to remote devices.
BASE_FEATURE(kEnableUrlRestriction, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables clean up of hidden groups.
BASE_FEATURE(kEnableOriginatingSavedGroupCleanUp,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Swaps the click actions for the tab group header.
BASE_FEATURE(kLeftClickOpensTabGroupBubble, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupSyncDelegateAndroidEnabled() {
  return base::FeatureList::IsEnabled(kTabGroupSyncDelegateAndroid);
}

bool IsTabGroupSyncCoordinatorEnabled() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif
}

bool RestrictDownloadOnSyncedTabs() {
  return base::FeatureList::IsEnabled(kRestrictDownloadOnSyncedTabs);
}

bool DeferMediaLoadInBackgroundTab() {
  return data_sharing::features::IsDataSharingFunctionalityEnabled();
}

bool ShouldForceRemoveClosedTabGroupsOnStartup() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return base::FeatureList::IsEnabled(kForceRemoveClosedTabGroupsOnStartup);
#else
  return false;
#endif
}

bool IsTabTitleSanitizationEnabled() {
  return data_sharing::features::IsDataSharingFunctionalityEnabled();
}

bool IsUrlRestrictionEnabled() {
  return data_sharing::features::IsDataSharingFunctionalityEnabled() &&
         base::FeatureList::IsEnabled(kEnableUrlRestriction);
}

bool IsOriginatingSavedGroupCleanUpEnabled() {
  return base::FeatureList::IsEnabled(kEnableOriginatingSavedGroupCleanUp);
}

base::TimeDelta GetOriginatingSavedGroupCleanUpTimeInterval() {
  int time_in_seconds = base::GetFieldTrialParamByFeatureAsInt(
      kEnableOriginatingSavedGroupCleanUp,
      kGroupCleanUpTimeIntervalInSecondsFinchKey,
      kDefaultGroupCleanUpTimeInternalInSeconds);
  return base::Seconds(time_in_seconds);
}

}  // namespace tab_groups
