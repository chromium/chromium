// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_FEATURES_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"

namespace tab_groups {

BASE_DECLARE_FEATURE(kTabGroupSyncDisableNetworkLayer);

BASE_DECLARE_FEATURE(kTabGroupsSaveV2);

BASE_DECLARE_FEATURE(kTabGroupSyncDelegateAndroid);

BASE_DECLARE_FEATURE(kRestrictDownloadOnSyncedTabs);

BASE_DECLARE_FEATURE(kUseAlternateHistorySyncIllustration);

BASE_DECLARE_FEATURE(kForceRemoveClosedTabGroupsOnStartup);

BASE_DECLARE_FEATURE(kEnableUrlRestriction);

BASE_DECLARE_FEATURE(kEnableOriginatingSavedGroupCleanUp);

BASE_DECLARE_FEATURE(kLeftClickOpensTabGroupBubble);

extern bool IsTabGroupSyncDelegateAndroidEnabled();

extern bool IsTabGroupSyncCoordinatorEnabled();

extern bool AlwaysAcceptServerDataInModel();

extern bool RestrictDownloadOnSyncedTabs();

extern bool DeferMediaLoadInBackgroundTab();

extern bool ShouldForceRemoveClosedTabGroupsOnStartup();

extern bool IsTabTitleSanitizationEnabled();

extern bool IsUrlRestrictionEnabled();

extern bool IsOriginatingSavedGroupCleanUpEnabled();

extern base::TimeDelta GetOriginatingSavedGroupCleanUpTimeInterval();
}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_FEATURES_H_
