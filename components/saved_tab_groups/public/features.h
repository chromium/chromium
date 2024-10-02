// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_FEATURES_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace tab_groups {

BASE_DECLARE_FEATURE(kTabGroupSyncAndroid);

BASE_DECLARE_FEATURE(kTabGroupPaneAndroid);

BASE_DECLARE_FEATURE(kTabGroupSyncDisableNetworkLayer);

BASE_DECLARE_FEATURE(kTabGroupSyncForceOff);

BASE_DECLARE_FEATURE(kTabGroupsSaveV2);

BASE_DECLARE_FEATURE(kTabGroupsSaveUIUpdate);

BASE_DECLARE_FEATURE(kTabGroupSyncUno);

BASE_DECLARE_FEATURE(kTabGroupSyncServiceDesktopMigration);

BASE_DECLARE_FEATURE(kAlwaysAcceptServerDataInModel);

BASE_DECLARE_FEATURE(kTabGroupSyncAutoOpenKillSwitch);

BASE_DECLARE_FEATURE(kRestrictDownloadOnSyncedTabs);

BASE_DECLARE_FEATURE(kDeferMediaLoadInBackgroundTab);

BASE_DECLARE_FEATURE(kSavedTabGroupNotifyOnInteractionTimeChanged);

BASE_DECLARE_FEATURE(kUseAlternateHistorySyncIllustration);

BASE_DECLARE_FEATURE(kForceRemoveClosedTabGroupsOnStartup);

extern bool IsTabGroupsSaveV2Enabled();

extern bool IsTabGroupsSaveUIUpdateEnabled();

extern bool IsTabGroupSyncServiceDesktopMigrationEnabled();

extern bool IsTabGroupSyncCoordinatorEnabled();

extern bool AlwaysAcceptServerDataInModel();

extern bool RestrictDownloadOnSyncedTabs();

extern bool DeferMediaLoadInBackgroundTab();

extern bool ShouldForceRemoveClosedTabGroupsOnStartup();

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_FEATURES_H_
