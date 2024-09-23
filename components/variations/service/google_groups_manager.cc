// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/google_groups_manager.h"

#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/service/sync_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/google_groups_manager_prefs.h"
#include "components/variations/variations_seed_processor.h"

GoogleGroupsManager::GoogleGroupsManager(
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
      base::BindRepeating(&GoogleGroupsManager::UpdateGoogleGroups,
                          base::Unretained(this)));

  // Also process the initial value.
  UpdateGoogleGroups();
}

GoogleGroupsManager::~GoogleGroupsManager() = default;

// static
void GoogleGroupsManager::RegisterProfilePrefs(
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

bool GoogleGroupsManager::IsFeatureEnabledForProfile(
    const base::Feature& feature) const {
  if (!base::FeatureList::IsEnabled(feature)) {
    return false;
  }

  const std::string google_groups_param =
      base::FeatureParam<std::string>(
          &feature, variations::internal::kGoogleGroupFeatureParamName, "")
          .Get();
  const std::vector<std::string_view> group_strings = base::SplitStringPiece(
      google_groups_param,
      variations::internal::kGoogleGroupFeatureParamSeparator,
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (group_strings.empty()) {
    // No required Google Group for this experiment.
    return true;
  }
  return base::ranges::any_of(group_strings, [&user_groups = google_group_ids_](
                                                 std::string_view group) {
    return user_groups.contains(group);
  });
}

void GoogleGroupsManager::Shutdown() {
  sync_service_observation_.Reset();
}

void GoogleGroupsManager::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_service_observation_.Observe(sync_service);

  // Honor the initial state, in case OnStateChanged() never gets called.
  OnStateChanged(sync_service);
}

void GoogleGroupsManager::OnStateChanged(syncer::SyncService* sync) {
  switch (sync->GetTransportState()) {
    case syncer::SyncService::TransportState::DISABLED:
      ClearSigninScopedState();
      break;
    case syncer::SyncService::TransportState::PAUSED:
    case syncer::SyncService::TransportState::START_DEFERRED:
    case syncer::SyncService::TransportState::INITIALIZING:
    case syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case syncer::SyncService::TransportState::CONFIGURING:
    case syncer::SyncService::TransportState::ACTIVE:
      break;
  }
}

void GoogleGroupsManager::OnSyncShutdown(syncer::SyncService* sync) {
  sync_service_observation_.Reset();
}

void GoogleGroupsManager::ClearSigninScopedState() {
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

void GoogleGroupsManager::UpdateGoogleGroups() {
  // Get the current value of the local state dict.
  ScopedDictPrefUpdate target_prefs_update(
      &target_prefs_.get(), variations::prefs::kVariationsGoogleGroups);
  base::Value::Dict& target_prefs_dict = target_prefs_update.Get();

  const base::Value::List& source_list = source_prefs_->GetList(
#if BUILDFLAG(IS_CHROMEOS_ASH)
      variations::kOsDogfoodGroupsSyncPrefName
#else
      variations::kDogfoodGroupsSyncPrefName
#endif
  );

  base::Value::List groups;
  std::vector<std::string> group_ids;
  group_ids.reserve(source_list.size());
  for (const auto& group_value : source_list) {
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
    group_ids.push_back(*group_str);
  }
  target_prefs_dict.Set(key_, std::move(groups));
  google_group_ids_ = {std::make_move_iterator(group_ids.begin()),
                       std::make_move_iterator(group_ids.end())};
}
