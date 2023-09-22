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

// This feature controls whether variations code copies the dogfood group
// information from per-profile data to local-state.
BASE_FEATURE(kVariationsGoogleGroupFiltering,
             "VariationsGoogleGroupFiltering",
             base::FEATURE_ENABLED_BY_DEFAULT);

GoogleGroupsUpdaterService::GoogleGroupsUpdaterService(
    PrefService& target_prefs,
    const std::string& key,
    PrefService& source_prefs)
    : target_prefs_(target_prefs), key_(key), source_prefs_(source_prefs) {
  // Register for preference changes.
  pref_change_registrar_.Init(&source_prefs_.get());
  pref_change_registrar_.Add(
#if BUILDFLAG(IS_CHROMEOS_ASH)
      variations::kOsDogfoodGroupsSyncPrefName,
#else
      variations::kDogfoodGroupsSyncPrefName,
#endif
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
      variations::kOsDogfoodGroupsSyncPrefName,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF
#else
      variations::kDogfoodGroupsSyncPrefName,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF
#endif
  );
}

void GoogleGroupsUpdaterService::ClearSigninScopedState() {
  source_prefs_->ClearPref(
#if BUILDFLAG(IS_CHROMEOS_ASH)
      variations::kOsDogfoodGroupsSyncPrefName
#else
      variations::kDogfoodGroupsSyncPrefName
#endif
  );

  // UpdateGoogleGroups() will be called via the PrefChangeRegistrar, and will
  // propagate this change to local state.
}

void GoogleGroupsUpdaterService::UpdateGoogleGroups() {
  // Get the current value of the local state dict.
  ScopedDictPrefUpdate target_prefs_update(
      &target_prefs_.get(), variations::prefs::kVariationsGoogleGroups);
  base::Value::Dict& target_prefs_dict = target_prefs_update.Get();

  base::Value::List groups;
  for (const auto& group_value : source_prefs_->GetList(
#if BUILDFLAG(IS_CHROMEOS_ASH)
           variations::kOsDogfoodGroupsSyncPrefName
#else
           variations::kDogfoodGroupsSyncPrefName
#endif
           )) {
    const base::Value::Dict* group_dict = group_value.GetIfDict();
    if (group_dict == nullptr) {
      continue;
    }
    const std::string* group_str =
        group_dict->FindString(variations::kDogfoodGroupsSyncPrefGaiaIdKey);
    if ((group_str == nullptr) || group_str->empty()) {
      continue;
    }
    groups.Append(*group_str);
  }
  target_prefs_dict.Set(key_, std::move(groups));
}
