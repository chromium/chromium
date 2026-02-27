// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_prefs.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_search_feature.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ui/actions/actions.h"

namespace {

std::optional<bool> g_tab_search_trailing_tabstrip_at_startup = std::nullopt;
}

namespace tabs {

bool GetDefaultTabSearchRightAligned() {
  // These platforms are all left aligned, the others should be right.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  return false;
#else
  return true;
#endif
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kTabSearchRightAligned,
                                GetDefaultTabSearchRightAligned());
  registry->RegisterBooleanPref(prefs::kTabSearchPinnedToTabstrip, true);
  registry->RegisterBooleanPref(
      prefs::kTabSearchPinnedToTabstripMigrationComplete, false);
  registry->RegisterBooleanPref(prefs::kProjectsPanelPinnedToTabstrip, true);
  registry->RegisterBooleanPref(prefs::kEverythingMenuPinnedToTabstrip, true);
  registry->RegisterBooleanPref(prefs::kVerticalTabsEnabled, false);
  registry->RegisterBooleanPref(prefs::kVerticalTabsEnabledFirstTime, false);
}

void MigrateTabSearchPref(PrefService* profile_prefs) {
  if (profile_prefs->GetBoolean(
          prefs::kTabSearchPinnedToTabstripMigrationComplete)) {
    return;
  }

  const std::optional<std::string>& tab_search_action_id =
      actions::ActionIdMap::ActionIdToString(kActionTabSearch);
  if (tab_search_action_id.has_value()) {
    const base::ListValue& pinned_actions =
        profile_prefs->GetList(prefs::kPinnedActions);
    bool is_pinned = false;
    for (const auto& action : pinned_actions) {
      if (action.is_string() && action.GetString() == *tab_search_action_id) {
        is_pinned = true;
        break;
      }
    }
    profile_prefs->SetBoolean(prefs::kTabSearchPinnedToTabstrip, is_pinned);
  }
  profile_prefs->SetBoolean(prefs::kTabSearchPinnedToTabstripMigrationComplete,
                            true);
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

  if (base::FeatureList::IsEnabled(tabs::kHorizontalTabStripComboButton)) {
    return TabSearchPosition::kLeadingHorizontalTabstrip;
  }

  const bool has_tab_search_toolbar_button =
      features::HasTabSearchToolbarButton();
  if (has_tab_search_toolbar_button) {
    return TabSearchPosition::kToolbarButton;
  }

  // If this pref has already been read, we need to return the same value.
  if (!g_tab_search_trailing_tabstrip_at_startup.has_value()) {
    g_tab_search_trailing_tabstrip_at_startup =
        GetDefaultTabSearchRightAligned();
  }

  return g_tab_search_trailing_tabstrip_at_startup.value()
             ? TabSearchPosition::kTrailingHorizontalTabstrip
             : TabSearchPosition::kLeadingHorizontalTabstrip;
}

void SetTabSearchRightAlignedForTesting(bool is_right_aligned) {
  g_tab_search_trailing_tabstrip_at_startup = is_right_aligned;
}

}  // namespace tabs
