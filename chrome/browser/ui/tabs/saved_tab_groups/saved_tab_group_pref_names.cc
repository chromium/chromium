// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_pref_names.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace tab_groups::saved_tab_groups::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kTabGroupSavesUIUpdateMigrated, false);
  registry->RegisterBooleanPref(kTabGroupsDeletionSkipDialogOnDelete, false);
  registry->RegisterBooleanPref(kTabGroupsDeletionSkipDialogOnUngroup, false);
  registry->RegisterBooleanPref(kTabGroupsDeletionSkipDialogOnRemoveTab, false);
  registry->RegisterBooleanPref(kTabGroupsDeletionSkipDialogOnCloseTab, false);
}

bool IsTabGroupSavesUIUpdateMigrated(PrefService* pref_service) {
  return pref_service->GetBoolean(kTabGroupSavesUIUpdateMigrated);
}

void SetTabGroupSavesUIUpdateMigrated(PrefService* pref_service) {
  pref_service->SetBoolean(kTabGroupSavesUIUpdateMigrated, true);
}

}  // namespace tab_groups::saved_tab_groups::prefs
