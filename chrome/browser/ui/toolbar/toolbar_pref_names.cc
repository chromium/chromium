// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/actions/actions.h"

namespace toolbar {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  base::Value::List default_pinned_actions;
  const std::optional<std::string>& chrome_labs_action =
      actions::ActionIdMap::ActionIdToString(kActionShowChromeLabs);
  // ActionIdToStringMappings are not initialized in unit tests, therefore will
  // not have a value. In the normal case, the action should always have a
  // value.
  if (chrome_labs_action.has_value()) {
    default_pinned_actions.Append(chrome_labs_action.value());
  }

  if (features::HasTabSearchToolbarButton()) {
    const std::optional<std::string>& tab_search_action =
        actions::ActionIdMap::ActionIdToString(kActionTabSearch);
    if (tab_search_action.has_value()) {
      default_pinned_actions.Append(tab_search_action.value());
    }
  }

  registry->RegisterListPref(prefs::kPinnedActions,
                             std::move(default_pinned_actions),
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPinnedSearchCompanionMigrationComplete, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPinnedChromeLabsMigrationComplete, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPinnedCastMigrationComplete, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kTabSearchMigrationComplete, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

}  // namespace toolbar
