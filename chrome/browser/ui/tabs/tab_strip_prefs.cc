// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_prefs.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ui/actions/actions.h"

namespace tabs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kTabSearchPinnedToTabstrip, true);
  registry->RegisterBooleanPref(prefs::kProjectsPanelPinnedToTabstrip, true);
  registry->RegisterBooleanPref(
      prefs::kEverythingMenuPinnedToTabstrip,
      !base::FeatureList::IsEnabled(
          tabs::kMigrateEverythingMenuPinnedToTabstrip));
  registry->RegisterBooleanPref(
      prefs::kEverythingMenuPinnedToTabstripMigrationComplete, false);
  registry->RegisterBooleanPref(prefs::kVerticalTabsEnabled, false);
  registry->RegisterBooleanPref(
      prefs::kVerticalTabsExpandOnHoverEnabled,
      tabs::kVerticalTabsExpandOnHoverDefaultEnabled.Get());
  registry->RegisterBooleanPref(prefs::kVerticalTabsEnabledFirstTime, false);
  registry->RegisterBooleanPref(prefs::kVerticalTabsCollapsedState, false);
  registry->RegisterIntegerPref(prefs::kVerticalTabsUncollapsedWidth,
                                kVerticalTabStripDefaultUncollapsedWidth);
}

void MigrateHoverCardMemoryPref(PrefService* local_prefs) {
  if (!features::IsTabStripDeclutterEnabled() ||
      local_prefs->GetBoolean(
          prefs::kHoverCardMemoryUsageDisableMigrationComplete)) {
    return;
  }

  local_prefs->SetBoolean(prefs::kHoverCardMemoryUsageEnabled, false);
  local_prefs->SetBoolean(prefs::kHoverCardMemoryUsageDisableMigrationComplete,
                          true);
}

void MigrateEverythingMenuPinnedToTabstripPref(PrefService* profile_prefs) {
  // If the migration hasn't started yet or is complete, return early.
  if (!base::FeatureList::IsEnabled(
          tabs::kMigrateEverythingMenuPinnedToTabstrip) ||
      profile_prefs->GetBoolean(
          prefs::kEverythingMenuPinnedToTabstripMigrationComplete)) {
    return;
  }

  // If a user has previously enabled vertical tabs and hasn't changed the value
  // of `prefs::kEverythingMenuPinnedToTabstrip` then set the value to be true.
  // This is needed because the default value of the pref is changing to false
  // for users who haven't seen the pinned button yet.
  if (!profile_prefs->HasPrefPath(prefs::kEverythingMenuPinnedToTabstrip) &&
      profile_prefs->GetBoolean(prefs::kVerticalTabsEnabledFirstTime)) {
    profile_prefs->SetBoolean(prefs::kEverythingMenuPinnedToTabstrip, true);
  }

  profile_prefs->SetBoolean(
      prefs::kEverythingMenuPinnedToTabstripMigrationComplete, true);
}

TabSearchPosition GetTabSearchPosition(
    const BrowserWindowInterface* browser_window) {
  if (browser_window) {
    auto* const controller =
        tabs::VerticalTabStripStateController::From(browser_window);
    if (controller && controller->ShouldDisplayVerticalTabs()) {
      return TabSearchPosition::kVerticalTabstrip;
    }
  }
  return TabSearchPosition::kLeadingHorizontalTabstrip;
}

}  // namespace tabs
