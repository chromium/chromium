// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_model_associator.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace sync_preferences {

namespace {

const sync_pb::PreferenceSpecifics& GetSpecifics(const syncer::SyncData& pref) {
  switch (pref.GetDataType()) {
    case syncer::PREFERENCES:
      return pref.GetSpecifics().preference();
    case syncer::PRIORITY_PREFERENCES:
      return pref.GetSpecifics().priority_preference().preference();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case syncer::OS_PREFERENCES:
      return pref.GetSpecifics().os_preference().preference();
    case syncer::OS_PRIORITY_PREFERENCES:
      return pref.GetSpecifics().os_priority_preference().preference();
#endif
    default:
      NOTREACHED();
      return pref.GetSpecifics().preference();
  }
}

absl::optional<base::Value> ReadPreferenceSpecifics(
    const sync_pb::PreferenceSpecifics& preference) {
  base::JSONReader::Result parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(preference.value());
  if (!parsed_json.has_value()) {
    LOG(ERROR) << "Failed to deserialize preference value: "
               << parsed_json.error().message;
    return absl::nullopt;
  }
  return std::move(*parsed_json);
}

base::Value::List MergeListValues(const base::Value::List& from_value,
                                  const base::Value::List& to_value) {
  base::Value::List result = to_value.Clone();
  for (const auto& value : from_value) {
    if (!base::Contains(result, value)) {
      result.Append(value.Clone());
    }
  }

  return result;
}

base::Value::Dict MergeDictionaryValues(const base::Value::Dict& from_value,
                                        const base::Value::Dict& to_value) {
  base::Value::Dict result = to_value.Clone();

  for (auto it : from_value) {
    // It's not clear whether using a C++17 structured binding here would cause
    // a copy of the value or not, so in doubt unpack the old way.
    const base::Value* from_key_value = &it.second;
    base::Value* to_key_value = result.Find(it.first);
    if (to_key_value) {
      if (from_key_value->is_dict() && to_key_value->is_dict()) {
        *to_key_value = base::Value(MergeDictionaryValues(
            from_key_value->GetDict(), to_key_value->GetDict()));
      }
      // Note that for all other types we want to preserve the "to"
      // values so we do nothing here.
    } else {
      result.Set(it.first, from_key_value->Clone());
    }
  }
  return result;
}

}  // namespace

PrefModelAssociator::PrefModelAssociator(
    const PrefModelAssociatorClient* client,
    syncer::ModelType type)
    : type_(type), client_(client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(type_ == syncer::PREFERENCES ||
         type_ == syncer::PRIORITY_PREFERENCES ||
         type_ == syncer::OS_PREFERENCES ||
         type_ == syncer::OS_PRIORITY_PREFERENCES);
#else
  DCHECK(type_ == syncer::PREFERENCES || type_ == syncer::PRIORITY_PREFERENCES);
#endif
}

PrefModelAssociator::~PrefModelAssociator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PrefModelAssociator::SetPrefService(PrefServiceSyncable* pref_service) {
  DCHECK(pref_service_ == nullptr);
  pref_service_ = pref_service;
}

// static
sync_pb::PreferenceSpecifics* PrefModelAssociator::GetMutableSpecifics(
    syncer::ModelType type,
    sync_pb::EntitySpecifics* specifics) {
  switch (type) {
    case syncer::PREFERENCES:
      return specifics->mutable_preference();
    case syncer::PRIORITY_PREFERENCES:
      return specifics->mutable_priority_preference()->mutable_preference();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case syncer::OS_PREFERENCES:
      return specifics->mutable_os_preference()->mutable_preference();
    case syncer::OS_PRIORITY_PREFERENCES:
      return specifics->mutable_os_priority_preference()->mutable_preference();
#endif
    default:
      NOTREACHED();
      return nullptr;
  }
}

void PrefModelAssociator::InitPrefAndAssociate(
    const syncer::SyncData& sync_pref,
    const std::string& pref_name,
    syncer::SyncChangeList* sync_changes) {
  const base::Value* user_pref_value =
      pref_service_->GetUserPrefValue(pref_name);
  VLOG(1) << "Associating preference " << pref_name;

  if (sync_pref.IsValid()) {
    const sync_pb::PreferenceSpecifics& preference = GetSpecifics(sync_pref);
    DCHECK(pref_name == preference.name());
    base::JSONReader::Result parsed_json =
        base::JSONReader::ReadAndReturnValueWithError(preference.value());
    if (!parsed_json.has_value()) {
      LOG(ERROR) << "Failed to deserialize value of preference '" << pref_name
                 << "': " << parsed_json.error().message;
      return;
    }
    base::Value sync_value = std::move(*parsed_json);

    if (user_pref_value) {
      DVLOG(1) << "Found user pref value for " << pref_name;
      // We have both server and local values. Merge them.
      base::Value new_value(
          MergePreference(pref_name, *user_pref_value, sync_value));

      // Update the local preference based on what we got from the
      // sync server. Note: this only updates the user value store, which is
      // ignored if the preference is policy controlled.
      if (new_value.is_none()) {
        LOG(WARNING) << "Sync has null value for pref " << pref_name.c_str();
        pref_service_->ClearPref(pref_name);
      } else if (*user_pref_value != new_value) {
        SetPrefWithTypeCheck(pref_name, new_value);
      }

      // If the merge resulted in an updated value, inform the syncer.
      if (sync_value != new_value) {
        syncer::SyncData sync_data;
        if (!CreatePrefSyncData(pref_name, new_value, &sync_data)) {
          LOG(ERROR) << "Failed to update preference.";
          return;
        }

        sync_changes->push_back(syncer::SyncChange(
            FROM_HERE, syncer::SyncChange::ACTION_UPDATE, sync_data));
      }
    } else if (!sync_value.is_none()) {
      // Only a server value exists. Just set the local user value.
      SetPrefWithTypeCheck(pref_name, sync_value);
    } else {
      LOG(WARNING) << "Sync has null value for pref " << pref_name.c_str();
    }
    synced_preferences_.insert(preference.name());
  } else if (user_pref_value) {
    // The server does not know about this preference and should be added
    // to the syncer's database.
    syncer::SyncData sync_data;
    if (!CreatePrefSyncData(pref_name, *user_pref_value, &sync_data)) {
      LOG(ERROR) << "Failed to update preference.";
      return;
    }
    sync_changes->push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD, sync_data));
    synced_preferences_.insert(pref_name);
  }

  // Else this pref does not have a sync value but also does not have a user
  // controlled value (either it's a default value or it's policy controlled,
  // either way it's not interesting). We can ignore it. Once it gets changed,
  // we'll send the new user controlled value to the syncer.
}

void PrefModelAssociator::WaitUntilReadyToSync(base::OnceClosure done) {
  // Prefs are loaded very early during profile initialization.
  DCHECK_NE(pref_service_->GetInitializationStatus(),
            PrefService::INITIALIZATION_STATUS_WAITING);
  std::move(done).Run();
}

absl::optional<syncer::ModelError>
PrefModelAssociator::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) {
  DCHECK_EQ(type_, type);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  sync_processor_ = std::move(sync_processor);

  syncer::SyncChangeList new_changes;
  std::set<std::string> remaining_preferences = registered_preferences_;

  // Go through and check for all preferences we care about that sync already
  // knows about.
  for (const syncer::SyncData& sync_data : initial_sync_data) {
    DCHECK_EQ(type_, sync_data.GetDataType());

    const sync_pb::PreferenceSpecifics& preference = GetSpecifics(sync_data);
    std::string sync_pref_name = preference.name();

    if (remaining_preferences.count(sync_pref_name) == 0) {
      // We're not syncing this preference locally, ignore the sync data.
      continue;
    }

    remaining_preferences.erase(sync_pref_name);
    InitPrefAndAssociate(sync_data, sync_pref_name, &new_changes);
    NotifyStartedSyncing(sync_pref_name);
  }

  // Go through and build sync data for any remaining preferences.
  for (const std::string& remaining_preference : remaining_preferences) {
    InitPrefAndAssociate(syncer::SyncData(), remaining_preference,
                         &new_changes);
  }

  for (const std::string& legacy_pref_name : legacy_model_type_preferences_) {
    // Track preferences for which we have a local user-controlled value. That
    // could be a value from last run, or a value just set by the initial sync.
    // We don't call InitPrefAndAssociate because we don't want the initial sync
    // to trigger outgoing changes -- these prefs are only tracked to send
    // updates back to older clients.
    if (pref_service_->GetUserPrefValue(legacy_pref_name)) {
      synced_preferences_.insert(legacy_pref_name);
    }
  }

  // Push updates to sync.
  absl::optional<syncer::ModelError> error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);
  if (!error.has_value()) {
    models_associated_ = true;
    pref_service_->OnIsSyncingChanged();
  }
  return error;
}

void PrefModelAssociator::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type_, type);
  models_associated_ = false;
  sync_processor_.reset();
  pref_service_->OnIsSyncingChanged();
}

base::Value PrefModelAssociator::MergePreference(
    const std::string& name,
    const base::Value& local_value,
    const base::Value& server_value) const {
  // This function special cases preferences individually, so don't attempt
  // to merge for all migrated values.
  if (client_) {
    if (client_->IsMergeableListPreference(name)) {
      if (local_value.is_none())
        return server_value.Clone();
      if (server_value.is_none())
        return local_value.Clone();
      return base::Value(
          MergeListValues(local_value.GetList(), server_value.GetList()));
    }
    if (client_->IsMergeableDictionaryPreference(name)) {
      if (local_value.is_none())
        return server_value.Clone();
      if (server_value.is_none())
        return local_value.Clone();
      return base::Value(
          MergeDictionaryValues(local_value.GetDict(), server_value.GetDict()));
    }
    base::Value merged_value =
        client_->MaybeMergePreferenceValues(name, local_value, server_value);
    if (!merged_value.is_none()) {
      return merged_value;
    }
  }

  // If this is not a specially handled preference, server wins.
  return server_value.Clone();
}

bool PrefModelAssociator::CreatePrefSyncData(
    const std::string& name,
    const base::Value& value,
    syncer::SyncData* sync_data) const {
  if (value.is_none()) {
    LOG(ERROR) << "Attempting to sync a null pref value for " << name;
    return false;
  }

  std::string serialized;
  // TODO(zea): consider JSONWriter::Write since you don't have to check
  // failures to deserialize.
  JSONStringValueSerializer json(&serialized);
  if (!json.Serialize(value)) {
    LOG(ERROR) << "Failed to serialize preference value.";
    return false;
  }

  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics =
      GetMutableSpecifics(type_, &specifics);

  pref_specifics->set_name(name);
  pref_specifics->set_value(serialized);
  *sync_data = syncer::SyncData::CreateLocalData(name, name, specifics);
  return true;
}

absl::optional<syncer::ModelError> PrefModelAssociator::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!models_associated_) {
    return syncer::ModelError(FROM_HERE, "Models not yet associated.");
  }
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);
  for (const syncer::SyncChange& sync_change : change_list) {
    DCHECK_EQ(type_, sync_change.sync_data().GetDataType());

    const sync_pb::PreferenceSpecifics& pref_specifics =
        GetSpecifics(sync_change.sync_data());

    // It is possible that we may receive a change to a preference we do not
    // want to sync. For example if the user is syncing a Mac client and a
    // Windows client, the Windows client does not support
    // kConfirmToQuitEnabled. Ignore updates from these preferences.
    std::string pref_name = pref_specifics.name();
    if (!IsPrefRegistered(pref_name)) {
      continue;
    }

    if (sync_change.change_type() == syncer::SyncChange::ACTION_DELETE) {
      pref_service_->ClearPref(pref_name);
      continue;
    }

    absl::optional<base::Value> new_value(
        ReadPreferenceSpecifics(pref_specifics));
    if (!new_value) {
      // Skip values we can't deserialize.
      // TODO(zea): consider taking some further action such as erasing the
      // bad data.
      continue;
    }

    if (!SetPrefWithTypeCheck(pref_name, *new_value)) {
      // Ignore updates where the server type doesn't match the local type. In
      // that case, don't notify observers or insert into `synced_preferences_`.
      continue;
    }

    NotifySyncedPrefObservers(pref_specifics.name(), true /*from_sync*/);

    // Keep track of any newly synced preferences.
    if (sync_change.change_type() == syncer::SyncChange::ACTION_ADD) {
      synced_preferences_.insert(pref_specifics.name());
    }
  }
  return absl::nullopt;
}

void PrefModelAssociator::AddSyncedPrefObserver(const std::string& name,
                                                SyncedPrefObserver* observer) {
  auto& observers = synced_pref_observers_[name];
  if (!observers) {
    observers = std::make_unique<SyncedPrefObserverList>();
  }

  observers->AddObserver(observer);
}

void PrefModelAssociator::RemoveSyncedPrefObserver(
    const std::string& name,
    SyncedPrefObserver* observer) {
  auto observer_iter = synced_pref_observers_.find(name);
  if (observer_iter == synced_pref_observers_.end()) {
    return;
  }
  observer_iter->second->RemoveObserver(observer);
}

bool PrefModelAssociator::IsPrefSyncedForTesting(
    const std::string& name) const {
  return synced_preferences_.find(name) != synced_preferences_.end();
}

void PrefModelAssociator::RegisterPref(const std::string& name) {
  DCHECK(!base::Contains(registered_preferences_, name));
  registered_preferences_.insert(name);
}

void PrefModelAssociator::RegisterPrefWithLegacyModelType(
    const std::string& name) {
  DCHECK(!base::Contains(legacy_model_type_preferences_, name));
  DCHECK(!base::Contains(registered_preferences_, name));
  legacy_model_type_preferences_.insert(name);
}

bool PrefModelAssociator::IsPrefRegistered(const std::string& name) const {
  return registered_preferences_.count(name) > 0;
}

bool PrefModelAssociator::IsLegacyModelTypePref(const std::string& name) const {
  return legacy_model_type_preferences_.count(name) > 0;
}

void PrefModelAssociator::ProcessPrefChange(const std::string& name) {
  if (processing_syncer_changes_) {
    return;  // These are changes originating from us, ignore.
  }

  // We only process changes if we've already associated models.
  // This also filters out local changes during the initial merge.
  if (!models_associated_) {
    return;
  }

  if (!IsPrefRegistered(name) && !IsLegacyModelTypePref(name)) {
    // We are not syncing this preference -- this also filters out synced
    // preferences of the wrong type (e.g. priority preference are handled by a
    // separate associator). Legacy model type preferences are allowed to
    // continue because we want to push updates to old clients using the
    // old ModelType.
    return;
  }

  const PrefService::Preference* preference =
      pref_service_->FindPreference(name);
  DCHECK(preference);

  if (!preference->IsUserModifiable()) {
    // If the preference is no longer user modifiable, it must now be
    // controlled by policy, whose values we do not sync. Just return. If the
    // preference stops being controlled by policy, it will revert back to the
    // user value (which we continue to update with sync changes).
    return;
  }

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  NotifySyncedPrefObservers(name, false /*from_sync*/);

  syncer::SyncChangeList changes;

  if (synced_preferences_.count(name) == 0) {
    // Not in synced_preferences_ means no synced data. InitPrefAndAssociate(..)
    // will determine if we care about its data (e.g. if it has a default value
    // and hasn't been changed yet we don't) and take care syncing any new data.
    InitPrefAndAssociate(syncer::SyncData(), name, &changes);
  } else {
    // We are already syncing this preference, just update or delete its sync
    // node.
    syncer::SyncData sync_data;
    if (!CreatePrefSyncData(name, *preference->GetValue(), &sync_data)) {
      LOG(ERROR) << "Failed to update preference.";
      return;
    }
    if (pref_service_->GetUserPrefValue(name)) {
      // If the pref was updated, update it.
      changes.emplace_back(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                           sync_data);
    } else {
      // Otherwise, the pref must have been cleared and hence delete it.
      changes.emplace_back(FROM_HERE, syncer::SyncChange::ACTION_DELETE,
                           sync_data);
    }
  }

  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void PrefModelAssociator::NotifySyncedPrefObservers(const std::string& path,
                                                    bool from_sync) const {
  auto observer_iter = synced_pref_observers_.find(path);
  if (observer_iter == synced_pref_observers_.end()) {
    return;
  }
  // Don't notify for prefs we are only observing to support old clients.
  // The PrefModelAssociator for the new ModelType will notify.
  if (IsLegacyModelTypePref(path)) {
    DCHECK(!from_sync);
    return;
  }
  for (auto& observer : *observer_iter->second) {
    observer.OnSyncedPrefChanged(path, from_sync);
  }
}

bool PrefModelAssociator::SetPrefWithTypeCheck(const std::string& pref_name,
                                               const base::Value& new_value) {
  const PrefService::Preference* pref =
      pref_service_->FindPreference(pref_name);
  if (pref->GetType() != new_value.type()) {
    DLOG(WARNING) << "Unexpected type mis-match for pref. "
                  << "Synced value for " << pref_name << " is of type "
                  << new_value.type() << " which doesn't match the registered "
                  << "pref type: " << pref->GetType();
    return false;
  }
  // This will only modify the user controlled value store, which takes priority
  // over the default value but is ignored if the preference is policy
  // controlled.
  pref_service_->Set(pref_name, new_value);
  return true;
}

void PrefModelAssociator::NotifyStartedSyncing(const std::string& path) const {
  auto observer_iter = synced_pref_observers_.find(path);
  if (observer_iter == synced_pref_observers_.end()) {
    return;
  }

  for (auto& observer : *observer_iter->second) {
    observer.OnStartedSyncing(path);
  }
}

}  // namespace sync_preferences
