// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/pref_names.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/saved_tab_groups/features.h"

namespace tab_groups::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSyncableTabGroups, false);
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(tab_groups::kTabGroupSyncAndroid)) {
    registry->RegisterBooleanPref(prefs::kAutoOpenSyncedTabGroups,
                                  base::GetFieldTrialParamByFeatureAsBool(
                                      tab_groups::kTabGroupSyncAndroid,
                                      "auto_open_synced_tab_groups", true));
  }
  // Always register stop showing prefs. They're conditionally used by a cached
  // feature in Java, which is hard to synchronize.
  registry->RegisterBooleanPref(prefs::kStopShowingTabGroupConfirmationOnClose,
                                false);
  registry->RegisterBooleanPref(
      prefs::kStopShowingTabGroupConfirmationOnUngroup, false);
  registry->RegisterBooleanPref(
      prefs::kStopShowingTabGroupConfirmationOnTabRemove, false);
  registry->RegisterBooleanPref(
      prefs::kStopShowingTabGroupConfirmationOnTabClose, false);
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace tab_groups::prefs
