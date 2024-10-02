// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace tab_groups {
// Core feature flag for tab group sync on Android.
// Controls registration with the sync service and tab model hookup UI layer.
// TabGroupSyncService is eanbled when either this flag or kTabGroupPaneAndroid
// is enabled.
BASE_FEATURE(kTabGroupSyncAndroid,
             "TabGroupSyncAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag used to enable tab group revisit surface.
BASE_FEATURE(kTabGroupPaneAndroid,
             "TabGroupPaneAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag used to determine whether the network layer is disabled for
// tab group sync.
BASE_FEATURE(kTabGroupSyncDisableNetworkLayer,
             "TabGroupSyncDisableNetworkLayer",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupSyncForceOff,
             "TabGroupSyncForceOff",
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
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Feature flag specific to UNO. Controls how we handle tab groups on sign-out
// and sync toggle. Can be defined independently for each platform.
BASE_FEATURE(kTabGroupSyncUno,
             "TabGroupSyncUno",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag specific to Desktop platforms. When enabled, desktop platforms
// will use the TabGroupSyncService. When disabled, desktop platforms will
// continue to use SavedTabGroupKeyedService.
BASE_FEATURE(kTabGroupSyncServiceDesktopMigration,
             "TabGroupSyncServiceDesktopMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag to remove any merge logic from saved tab group model.
BASE_FEATURE(kAlwaysAcceptServerDataInModel,
             "AlwaysAcceptServerDataInModel",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag to disable auto-open of saved tab groups. Note that the
// settings page for auto open will still be visible, and when user is allowed
// to change. However the written pref from the user selection will not be
// honored. This feature flag should be used only in case of an emergency.
BASE_FEATURE(kTabGroupSyncAutoOpenKillSwitch,
             "TabGroupSyncAutoOpenKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag to restrict download on synced tabs if the navigation is
// triggered without attention..
BASE_FEATURE(kRestrictDownloadOnSyncedTabs,
             "RestrictDownloadOnSyncedTabs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag to defer media load on background tab.
BASE_FEATURE(kDeferMediaLoadInBackgroundTab,
             "DeferMediaLoadInBackgroundTab",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch to stop notifying observers when user interaction time is
// updated and storing it to the storage.
BASE_FEATURE(kSavedTabGroupNotifyOnInteractionTimeChanged,
             "SavedTabGroupNotifyOnInteractionTimeChanged",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag to determine whether an alternate illustration should be used on
// the history sync consent screen. This feature should be used independent of
// any other features in this file.
BASE_FEATURE(kUseAlternateHistorySyncIllustration,
             "UseAlternateHistorySyncIllustration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Force remove all closed tab groups from the sync local DB on startup if this
// feature flag is enabled.
BASE_FEATURE(kForceRemoveClosedTabGroupsOnStartup,
             "ForceRemoveClosedTabGroupsOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupsSaveV2Enabled() {
  return base::FeatureList::IsEnabled(kTabGroupsSaveV2);
}

bool IsTabGroupsSaveUIUpdateEnabled() {
  return base::FeatureList::IsEnabled(kTabGroupsSaveUIUpdate);
}

bool IsTabGroupSyncServiceDesktopMigrationEnabled() {
  return base::FeatureList::IsEnabled(kTabGroupSyncServiceDesktopMigration);
}

bool IsTabGroupSyncCoordinatorEnabled() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif
}

bool AlwaysAcceptServerDataInModel() {
  return base::FeatureList::IsEnabled(kAlwaysAcceptServerDataInModel);
}

bool RestrictDownloadOnSyncedTabs() {
  return base::FeatureList::IsEnabled(kRestrictDownloadOnSyncedTabs);
}

bool DeferMediaLoadInBackgroundTab() {
  return base::FeatureList::IsEnabled(kDeferMediaLoadInBackgroundTab);
}

bool ShouldForceRemoveClosedTabGroupsOnStartup() {
  return base::FeatureList::IsEnabled(kForceRemoveClosedTabGroupsOnStartup);
}

}  // namespace tab_groups
