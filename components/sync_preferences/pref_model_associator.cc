// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_model_associator.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/synced_pref_observer.h"

using syncer::PREFERENCES;
using syncer::PRIORITY_PREFERENCES;

namespace sync_preferences {

namespace {

const sync_pb::PreferenceSpecifics& GetSpecifics(const syncer::SyncData& pref) {
  DCHECK(pref.GetDataType() == syncer::PREFERENCES ||
         pref.GetDataType() == syncer::PRIORITY_PREFERENCES);
  if (pref.GetDataType() == syncer::PRIORITY_PREFERENCES) {
    return pref.GetSpecifics().priority_preference().preference();
  } else {
    return pref.GetSpecifics().preference();
  }
}

sync_pb::PreferenceSpecifics* GetMutableSpecifics(
    const syncer::ModelType type,
    sync_pb::EntitySpecifics* specifics) {
  if (type == syncer::PRIORITY_PREFERENCES) {
    DCHECK(!specifics->has_preference());
    return specifics->mutable_priority_preference()->mutable_preference();
  } else {
    DCHECK(!specifics->has_priority_preference());
    return specifics->mutable_preference();
  }
}

}  // namespace

PrefModelAssociator::PrefModelAssociator(
    const PrefModelAssociatorClient* client,
    syncer::ModelType type,
    UnknownUserPrefAccessor* accessor)
    : pref_accessor_(accessor), type_(type), client_(client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(type_ == PREFERENCES || type_ == PRIORITY_PREFERENCES);
}

PrefModelAssociator::~PrefModelAssociator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_ = nullptr;

  synced_pref_observers_.clear();
}

void PrefModelAssociator::InitPrefAndAssociate(
    const syncer::SyncData& sync_pref,
    const std::string& pref_name,
    syncer::SyncChangeList* sync_changes) {
  UnknownUserPrefAccessor::PreferenceState local_pref_state =
      pref_accessor_->GetPreferenceState(type_, pref_name);
  if (local_pref_state.registration_state ==
          UnknownUserPrefAccessor::RegistrationState::kUnknown ||
      local_pref_state.registration_state ==
          UnknownUserPrefAccessor::RegistrationState::kNotSyncable) {
    // Only process syncable prefs and unknown prefs if whitelisted.
    return;
  }
  VLOG(1) << "Associating preference " << pref_name;

  if (sync_pref.IsValid()) {
    const sync_pb::PreferenceSpecifics& preference = GetSpecifics(sync_pref);
    DCHECK(pref_name == preference.name());
    base::JSONReader reader;
    std::unique_ptr<base::Value> sync_value(
        reader.ReadToValue(preference.value()));
    if (!sync_value.get()) {
      LOG(ERROR) << "Failed to deserialize preference value: "
                 << reader.GetErrorMessage();
      return;
    }

    if (local_pref_state.persisted_value) {
      DVLOG(1) << "Found user pref value for " << pref_name;
      // We have both server and local values. Merge them.
      std::unique_ptr<base::Value> new_value(MergePreference(
          pref_name, *local_pref_state.persisted_value, *sync_value));

      // Update the local preference based on what we got from the
      // sync server. Note: this only updates the user value store, which is
      // ignored if the preference is policy controlled.
      if (new_value->is_none()) {
        LOG(WARNING) << "Sync has null value for pref " << pref_name.c_str();
        pref_accessor_->ClearPref(pref_name, local_pref_state);
      } else if (!local_pref_state.persisted_value->Equals(new_value.get())) {
        pref_accessor_->SetPref(pref_name, local_pref_state, *new_value);
      }

      // If the merge resulted in an updated value, inform the syncer.
      if (!sync_value->Equals(new_value.get())) {
        syncer::SyncData sync_data;
        if (!CreatePrefSyncData(pref_name, *new_value, &sync_data)) {
          LOG(ERROR) << "Failed to update preference.";
          return;
        }

        sync_changes->push_back(syncer::SyncChange(
            FROM_HERE, syncer::SyncChange::ACTION_UPDATE, sync_data));
      }
    } else if (!sync_value->is_none()) {
      // Only a server value exists. Just set the local user value.
      pref_accessor_->SetPref(pref_name, local_pref_state, *sync_value);
    } else {
      LOG(WARNING) << "Sync has null value for pref " << pref_name.c_str();
    }
    synced_preferences_.insert(preference.name());
  } else if (local_pref_state.persisted_value) {
    DCHECK_EQ(local_pref_state.registration_state,
              UnknownUserPrefAccessor::RegistrationState::kSyncable);
    // The server does not know about this preference and should be added
    // to the syncer's database.
    syncer::SyncData sync_data;
    if (!CreatePrefSyncData(pref_name, *local_pref_state.persisted_value,
                            &sync_data)) {
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

void PrefModelAssociator::RegisterMergeDataFinishedCallback(
    const base::Closure& callback) {
  if (!models_associated_)
    callback_list_.push_back(callback);
  else
    callback.Run();
}

syncer::SyncMergeResult PrefModelAssociator::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK_EQ(type_, type);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_factory.get());
  syncer::SyncMergeResult merge_result(type);
  sync_processor_ = std::move(sync_processor);
  sync_error_factory_ = std::move(sync_error_factory);

  syncer::SyncChangeList new_changes;
  std::set<std::string> remaining_preferences = registered_preferences_;

  // Go through and check for all preferences we care about that sync already
  // knows about.
  for (auto sync_iter = initial_sync_data.begin();
       sync_iter != initial_sync_data.end(); ++sync_iter) {
    DCHECK_EQ(type_, sync_iter->GetDataType());

    const sync_pb::PreferenceSpecifics& preference = GetSpecifics(*sync_iter);
    std::string sync_pref_name = preference.name();
    remaining_preferences.erase(sync_pref_name);
    InitPrefAndAssociate(*sync_iter, sync_pref_name, &new_changes);
  }

  // Go through and build sync data for any remaining preferences.
  for (auto pref_name_iter = remaining_preferences.begin();
       pref_name_iter != remaining_preferences.end(); ++pref_name_iter) {
    InitPrefAndAssociate(syncer::SyncData(), *pref_name_iter, &new_changes);
  }

  UMA_HISTOGRAM_COUNTS_1000("Sync.Preferences.SyncingUnknownPrefs",
                            pref_accessor_->GetNumberOfSyncingUnknownPrefs());

  // Push updates to sync.
  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));
  if (merge_result.error().IsSet())
    return merge_result;

  for (const auto& callback : callback_list_)
    callback.Run();
  callback_list_.clear();

  models_associated_ = true;
  pref_service_->OnIsSyncingChanged();
  return merge_result;
}

void PrefModelAssociator::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type_, type);
  models_associated_ = false;
  sync_processor_.reset();
  sync_error_factory_.reset();
  pref_service_->OnIsSyncingChanged();
}

std::unique_ptr<base::Value> PrefModelAssociator::MergePreference(
    const std::string& name,
    const base::Value& local_value,
    const base::Value& server_value) {
  // This function special cases preferences individually, so don't attempt
  // to merge for all migrated values.
  if (client_) {
    std::string new_pref_name;
    if (client_->IsMergeableListPreference(name))
      return MergeListValues(local_value, server_value);
    if (client_->IsMergeableDictionaryPreference(name)) {
      return std::make_unique<base::Value>(
          MergeDictionaryValues(local_value, server_value));
    }
  }

  // If this is not a specially handled preference, server wins.
  return base::WrapUnique(server_value.DeepCopy());
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

std::unique_ptr<base::Value> PrefModelAssociator::MergeListValues(
    const base::Value& from_value,
    const base::Value& to_value) {
  if (from_value.is_none())
    return base::Value::ToUniquePtrValue(to_value.Clone());
  if (to_value.is_none())
    return base::Value::ToUniquePtrValue(from_value.Clone());

  DCHECK(from_value.type() == base::Value::Type::LIST);
  DCHECK(to_value.type() == base::Value::Type::LIST);

  base::Value result = to_value.Clone();
  base::Value::ListStorage& list = result.GetList();
  for (const auto& value : from_value.GetList()) {
    if (!base::ContainsValue(list, value))
      list.emplace_back(value.Clone());
  }

  return base::Value::ToUniquePtrValue(std::move(result));
}

base::Value PrefModelAssociator::MergeDictionaryValues(
    const base::Value& from_value,
    const base::Value& to_value) {
  if (from_value.is_none())
    return to_value.Clone();
  if (to_value.is_none())
    return from_value.Clone();

  DCHECK(from_value.is_dict());
  DCHECK(to_value.is_dict());
  base::Value result = to_value.Clone();

  for (const auto& it : from_value.DictItems()) {
    const base::Value* from_key_value = &it.second;
    base::Value* to_key_value = result.FindKey(it.first);
    if (to_key_value) {
      if (from_key_value->is_dict() && to_key_value->is_dict()) {
        *to_key_value = MergeDictionaryValues(*from_key_value, *to_key_value);
      }
      // Note that for all other types we want to preserve the "to"
      // values so we do nothing here.
    } else {
      result.SetKey(it.first, from_key_value->Clone());
    }
  }
  return result;
}

syncer::SyncDataList PrefModelAssociator::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(type_, type);
  syncer::SyncDataList current_data;
  for (auto iter = synced_preferences_.begin();
       iter != synced_preferences_.end(); ++iter) {
    std::string name = *iter;
    if (pref_accessor_->GetPreferenceState(type_, name).registration_state !=
        UnknownUserPrefAccessor::RegistrationState::kSyncable) {
      continue;
    }
    const PrefService::Preference* pref = pref_service_->FindPreference(name);
    DCHECK(pref);
    if (!pref->IsUserControlled() || pref->IsDefaultValue())
      continue;  // This is not data we care about.
    // TODO(zea): plumb a way to read the user controlled value.
    syncer::SyncData sync_data;
    if (!CreatePrefSyncData(name, *pref->GetValue(), &sync_data))
      continue;
    current_data.push_back(sync_data);
  }
  return current_data;
}

syncer::SyncError PrefModelAssociator::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!models_associated_) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Models not yet associated.", PREFERENCES);
    return error;
  }
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);
  syncer::SyncChangeList::const_iterator iter;
  for (iter = change_list.begin(); iter != change_list.end(); ++iter) {
    DCHECK_EQ(type_, iter->sync_data().GetDataType());

    const sync_pb::PreferenceSpecifics& pref_specifics =
        GetSpecifics(iter->sync_data());

    UnknownUserPrefAccessor::PreferenceState local_pref_state =
        pref_accessor_->GetPreferenceState(type_, pref_specifics.name());
    if (local_pref_state.registration_state ==
        UnknownUserPrefAccessor::RegistrationState::kUnknown) {
      // It is possible that we may receive a change to a preference we do not
      // want to sync. For example if the user is syncing a Mac client and a
      // Windows client, the Windows client does not support
      // kConfirmToQuitEnabled. Ignore updates from these preferences.
      // We only sync such prefs if they are whitelisted.
      continue;
    }
    if (local_pref_state.registration_state ==
        UnknownUserPrefAccessor::RegistrationState::kNotSyncable) {
      // Don't process remote changes for prefs this client doesn't want synced.
      continue;
    }
    if (iter->change_type() == syncer::SyncChange::ACTION_DELETE) {
      pref_accessor_->ClearPref(pref_specifics.name(), local_pref_state);
      continue;
    }

    std::unique_ptr<base::Value> new_value(
        ReadPreferenceSpecifics(pref_specifics));
    if (!new_value.get()) {
      // Skip values we can't deserialize.
      // TODO(zea): consider taking some further action such as erasing the
      // bad data.
      continue;
    }

    // This will only modify the user controlled value store, which takes
    // priority over the default value but is ignored if the preference is
    // policy controlled.
    pref_accessor_->SetPref(pref_specifics.name(), local_pref_state,
                            *new_value);

    NotifySyncedPrefObservers(pref_specifics.name(), true /*from_sync*/);

    // Keep track of any newly synced preferences. This can happen if a
    // preference was late registered or remotely added (ACTION_ADD).
    synced_preferences_.insert(pref_specifics.name());
  }
  return syncer::SyncError();
}

// static
base::Value* PrefModelAssociator::ReadPreferenceSpecifics(
    const sync_pb::PreferenceSpecifics& preference) {
  base::JSONReader reader;
  std::unique_ptr<base::Value> value(reader.ReadToValue(preference.value()));
  if (!value.get()) {
    std::string err =
        "Failed to deserialize preference value: " + reader.GetErrorMessage();
    LOG(ERROR) << err;
    return nullptr;
  }
  return value.release();
}

bool PrefModelAssociator::IsPrefSynced(const std::string& name) const {
  return synced_preferences_.find(name) != synced_preferences_.end();
}

void PrefModelAssociator::AddSyncedPrefObserver(const std::string& name,
                                                SyncedPrefObserver* observer) {
  auto& observers = synced_pref_observers_[name];
  if (!observers)
    observers = std::make_unique<SyncedPrefObserverList>();

  observers->AddObserver(observer);
}

void PrefModelAssociator::RemoveSyncedPrefObserver(
    const std::string& name,
    SyncedPrefObserver* observer) {
  auto observer_iter = synced_pref_observers_.find(name);
  if (observer_iter == synced_pref_observers_.end())
    return;
  observer_iter->second->RemoveObserver(observer);
}

void PrefModelAssociator::RegisterPref(const std::string& name) {
  DCHECK(!registered_preferences_.count(name));
  registered_preferences_.insert(name);

  // This pref might be registered after sync started. Make sure data in the
  // local store matches the registered type.
  // If this results in a modification of the local pref store, we don't want
  // to tell ChromeSync about these -- it's a local anomaly,
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);
  pref_accessor_->EnforceRegisteredTypeInStore(name);
}

bool PrefModelAssociator::IsPrefRegistered(const std::string& name) const {
  return registered_preferences_.count(name) > 0;
}

void PrefModelAssociator::ProcessPrefChange(const std::string& name) {
  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.

  // We only process changes if we've already associated models.
  // This also filters out local changes during the initial merge.
  if (!models_associated_)
    return;

  // From now on, this method does not have to deal with lazily registered
  // prefs, as local changes can only happen after they were registered.

  const PrefService::Preference* preference =
      pref_service_->FindPreference(name);
  // TODO(tschumann): When can this ever happen? Should this be a DCHECK?
  if (!preference)
    return;

  if (!IsPrefRegistered(name)) {
    // We are not syncing this preference -- this also filters out synced
    // preferences of the wrong type (priority preference are handled by a
    // separate associator).
    return;
  }

  syncer::SyncChangeList changes;

  if (!preference->IsUserModifiable()) {
    // If the preference is no longer user modifiable, it must now be
    // controlled by policy, whose values we do not sync. Just return. If the
    // preference stops being controlled by policy, it will revert back to the
    // user value (which we continue to update with sync changes).
    return;
  }

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  NotifySyncedPrefObservers(name, false /*from_sync*/);

  if (synced_preferences_.count(name) == 0) {
    // Not in synced_preferences_ means no synced data.
    // InitPrefAndAssociate(..) will determine if we care about its data (e.g.
    // if it has a default value and hasn't been changed yet we don't) and
    // take care syncing any new data.
    InitPrefAndAssociate(syncer::SyncData(), name, &changes);
  } else {
    // We are already syncing this preference, just update it's sync node.
    syncer::SyncData sync_data;
    if (!CreatePrefSyncData(name, *preference->GetValue(), &sync_data)) {
      LOG(ERROR) << "Failed to update preference.";
      return;
    }
    changes.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE, sync_data));
  }

  syncer::SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void PrefModelAssociator::SetPrefService(PrefServiceSyncable* pref_service) {
  DCHECK(pref_service_ == nullptr);
  pref_service_ = pref_service;
}

void PrefModelAssociator::NotifySyncedPrefObservers(const std::string& path,
                                                    bool from_sync) const {
  auto observer_iter = synced_pref_observers_.find(path);
  if (observer_iter == synced_pref_observers_.end())
    return;
  for (auto& observer : *observer_iter->second)
    observer.OnSyncedPrefChanged(path, from_sync);
}

}  // namespace sync_preferences
