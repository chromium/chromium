// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/google_groups_updater_service.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/pref_names.h"

// Per-profile preference for the sync data containing the list of dogfood group
// gaia IDs for a given syncing user.
// The variables below are the pref name, and the key for the gaia ID within
// the dictionary value.
const char kDogfoodGroupsSyncPrefName[] = "sync.dogfood_groups";
const char kDogfoodGroupsSyncPrefGaiaIdKey[] = "gaia_id";

// This feature controls whether variations code copies the dogfood group
// information from per-profile data to local-state.
BASE_FEATURE(kVariationsGoogleGroupFiltering,
             "VariationsGoogleGroupFiltering",
             base::FEATURE_DISABLED_BY_DEFAULT);

GoogleGroupsUpdaterService::GoogleGroupsUpdaterService(
    PrefService& target_prefs,
    const std::string& key,
    PrefService& source_prefs)
    : target_prefs_(target_prefs), key_(key), source_prefs_(source_prefs) {
  // Register for preference changes.
  pref_change_registrar_.Init(&source_prefs_.get());
  pref_change_registrar_.Add(
      kDogfoodGroupsSyncPrefName,
      base::BindRepeating(&GoogleGroupsUpdaterService::UpdateGoogleGroups,
                          base::Unretained(this)));

  // Also process the initial value.
  UpdateGoogleGroups();
}

GoogleGroupsUpdaterService::~GoogleGroupsUpdaterService() = default;

// static
void GoogleGroupsUpdaterService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(
      kDogfoodGroupsSyncPrefName,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
}

void GoogleGroupsUpdaterService::UpdateGoogleGroups() {
  // Get the current value of the local state dict.
  ScopedDictPrefUpdate target_prefs_update(
      &target_prefs_.get(), variations::prefs::kVariationsGoogleGroups);
  base::Value::Dict& target_prefs_dict = target_prefs_update.Get();

  base::Value::List groups;
  for (const auto& group_value :
       source_prefs_->GetList(kDogfoodGroupsSyncPrefName)) {
    const base::Value::Dict* group_dict = group_value.GetIfDict();
    if (group_dict == nullptr) {
      continue;
    }
    const std::string* group_str =
        group_dict->FindString(kDogfoodGroupsSyncPrefGaiaIdKey);
    if ((group_str == nullptr) || group_str->empty()) {
      continue;
    }
    groups.Append(*group_str);
  }
  target_prefs_dict.Set(key_, std::move(groups));
}
