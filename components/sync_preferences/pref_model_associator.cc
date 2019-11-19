// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_model_associator.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/feature_list.h"
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
#include "components/sync/base/model_type.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/pref_service_syncable.h"

using syncer::PREFERENCES;
using syncer::PRIORITY_PREFERENCES;

namespace sync_preferences {

namespace {

const sync_pb::PreferenceSpecifics& GetSpecifics(const syncer::SyncData& pref) {
  switch (pref.GetDataType()) {
    case syncer::PREFERENCES:
      return pref.GetSpecifics().preference();
    case syncer::PRIORITY_PREFERENCES:
      return pref.GetSpecifics().priority_preference().preference();
#if defined(OS_CHROMEOS)
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

}  // namespace

PrefModelAssociator::PrefModelAssociator(
    const PrefModelAssociatorClient* client,
    syncer::ModelType type,
    PersistentPrefStore* user_pref_store)
    : type_(type), client_(client), user_pref_store_(user_pref_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(OS_CHROMEOS)
  DCHECK(type_ == syncer::PREFERENCES ||
         type_ == syncer::PRIORITY_PREFERENCES ||
         type_ == syncer::OS_PREFERENCES ||
         type_ == syncer::OS_PRIORITY_PREFERENCES);
#else
  DCHECK(type_ == syncer::PREFERENCES || type_ == syncer::PRIORITY_PREFERENCES);
#endif
  DCHECK(user_pref_store_);
}

PrefModelAssociator::~PrefModelAssociator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_ = nullptr;

  synced_pref_observers_.clear();
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
#if defined(OS_CHROMEOS)
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
    base::JSONReader reader;
    std::unique_ptr<base::Value> sync_value(
        reader.ReadToValueDeprecated(preference.value()));
    if (!sync_value.get()) {
      LOG(ERROR) << "Failed to deserialize value of preference '" << pref_name
                 << "': " << reader.GetErrorMessage();
      return;
    }

    if (user_pref_value) {
      DVLOG(1) << "Found user pref value for " << pref_name;
      // We have both server and local values. Merge them.
      std::unique_ptr<base::Value> new_value(
          MergePreference(pref_name, *user_pref_value, *sync_value));

      // Update the local preference based on what we got from the
      // sync server. Note: this only updates the user value store, which is
      // ignored if the preference is policy controlled.
      if (new_value->is_none()) {
        LOG(WARNING) << "Sync has null value for pref " << pref_name.c_str();
        pref_service_->ClearPref(pref_name);
      } else if (!user_pref_value->Equals(new_value.get())) {
        SetPrefWithTypeCheck(pref_name, *new_value);
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
      SetPrefWithTypeCheck(pref_name, *sync_value);
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

    if (remaining_preferences.count(sync_pref_name) == 0) {
      // We're not syncing this preference locally, ignore the sync data.
      // TODO(zea): Eventually we want to be able to have the syncable service
      // reconstruct all sync data for its datatype (therefore having
      // GetAllSyncData be a complete representation). We should store this
      // data somewhere, even if we don't use it.
      continue;
    }

    remaining_preferences.erase(sync_pref_name);
    InitPrefAndAssociate(*sync_iter, sync_pref_name, &new_changes);
  }

  // Go through and build sync data for any remaining preferences.
  for (auto pref_name_iter = remaining_preferences.begin();
       pref_name_iter != remaining_preferences.end(); ++pref_name_iter) {
    InitPrefAndAssociate(syncer::SyncData(), *pref_name_iter, &new_changes);
  }

  for (const std::string& legacy_pref_name : legacy_model_type_preferences_) {
    // Track preferences for which we have a local user-controlled value. That
    // could be a value from last run, or a value just set by the initial sync.
    // We don't call InitPrefAndAssociate because we don't want the initial sync
    // to trigger outgoing changes -- these prefs are only tracked to send
    // updates back to older clients.
    if (pref_service_->GetUserPrefValue(legacy_pref_name))
      synced_preferences_.insert(legacy_pref_name);
  }

  // Push updates to sync.
  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));
  if (merge_result.error().IsSet())
    return merge_result;

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
    std::unique_ptr<base::Value> merged_value =
        client_->MaybeMergePreferenceValues(name, local_value, server_value);
    if (merged_value)
      return merged_value;
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
  for (const auto& value : from_value.GetList()) {
    if (!base::Contains(result.GetList(), value))
      result.Append(value.Clone());
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

// Note: This will build a model of all preferences registered as syncable
// with user controlled data. We do not track any information for preferences
// not registered locally as syncable and do not inform the syncer of
// non-user controlled preferences.
syncer::SyncDataList PrefModelAssociator::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(type_, type);
  syncer::SyncDataList current_data;
  for (auto iter = synced_preferences_.begin();
       iter != synced_preferences_.end(); ++iter) {
    std::string name = *iter;
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

    // It is possible that we may receive a change to a preference we do not
    // want to sync. For example if the user is syncing a Mac client and a
    // Windows client, the Windows client does not support
    // kConfirmToQuitEnabled. Ignore updates from these preferences.
    std::string pref_name = pref_specifics.name();
    if (!IsPrefRegistered(pref_name))
      continue;

    if (iter->change_type() == syncer::SyncChange::ACTION_DELETE) {
      pref_service_->ClearPref(pref_name);
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

    if (!TypeMatchesUserPrefStore(pref_name, *new_value)) {
      // Ignore updates where the server type doesn't match the local type.
      // Don't use SetPrefWithTypeCheck() because we want to skip notifying
      // observers and inserting into |synced_preferences_|.
      continue;
    }

    // This will only modify the user controlled value store, which takes
    // priority over the default value but is ignored if the preference is
    // policy controlled.
    pref_service_->Set(pref_name, *new_value);

    NotifySyncedPrefObservers(pref_specifics.name(), true /*from_sync*/);

    // Keep track of any newly synced preferences.
    if (iter->change_type() == syncer::SyncChange::ACTION_ADD) {
      synced_preferences_.insert(pref_specifics.name());
    }
  }
  return syncer::SyncError();
}

// static
base::Value* PrefModelAssociator::ReadPreferenceSpecifics(
    const sync_pb::PreferenceSpecifics& preference) {
  base::JSONReader reader;
  std::unique_ptr<base::Value> value(
      reader.ReadToValueDeprecated(preference.value()));
  if (!value.get()) {
    std::string err =
        "Failed to deserialize preference value: " + reader.GetErrorMessage();
    LOG(ERROR) << err;
    return nullptr;
  }
  return value.release();
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

bool PrefModelAssociator::IsPrefSyncedForTesting(
    const std::string& name) const {
  return synced_preferences_.find(name) != synced_preferences_.end();
}

void PrefModelAssociator::RegisterPref(const std::string& name) {
  DCHECK(registered_preferences_.count(name) == 0);
  registered_preferences_.insert(name);

  // Make sure data in the local store matches the registered type (where "type"
  // means base::Value data type like string, not ModelType like PREFERENCES).
  // If this results in a modification of the local pref store, we don't want
  // to tell ChromeSync about these -- it's a local anomaly,
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);
  EnforceRegisteredTypeInStore(name);
}

void PrefModelAssociator::RegisterPrefWithLegacyModelType(
    const std::string& name) {
  DCHECK(legacy_model_type_preferences_.count(name) == 0);
  DCHECK(registered_preferences_.count(name) == 0);
  legacy_model_type_preferences_.insert(name);
}

bool PrefModelAssociator::IsPrefRegistered(const std::string& name) const {
  return registered_preferences_.count(name) > 0;
}

bool PrefModelAssociator::IsLegacyModelTypePref(const std::string& name) const {
  return legacy_model_type_preferences_.count(name) > 0;
}

void PrefModelAssociator::ProcessPrefChange(const std::string& name) {
  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.

  // We only process changes if we've already associated models.
  // This also filters out local changes during the initial merge.
  if (!models_associated_)
    return;

  const PrefService::Preference* preference =
      pref_service_->FindPreference(name);
  // TODO(tschumann): When can this ever happen? Should this be a DCHECK?
  if (!preference)
    return;

  if (!IsPrefRegistered(name) && !IsLegacyModelTypePref(name)) {
    // We are not syncing this preference -- this also filters out synced
    // preferences of the wrong type (e.g. priority preference are handled by a
    // separate associator). Legacy model type preferences are allowed to
    // continue because we want to push updates to old clients using the
    // old ModelType.
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
      changes.push_back(syncer::SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_UPDATE, sync_data));
    } else {
      // Otherwise, the pref must have been cleared and hence delete it.
      changes.push_back(syncer::SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_DELETE, sync_data));
    }
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
  // Don't notify for prefs we are only observing to support old clients.
  // The PrefModelAssociator for the new ModelType will notify.
  if (IsLegacyModelTypePref(path)) {
    DCHECK(!from_sync);
    return;
  }
  for (auto& observer : *observer_iter->second)
    observer.OnSyncedPrefChanged(path, from_sync);
}

void PrefModelAssociator::SetPrefWithTypeCheck(const std::string& pref_name,
                                               const base::Value& new_value) {
  if (TypeMatchesUserPrefStore(pref_name, new_value))
    pref_service_->Set(pref_name, new_value);
}

bool PrefModelAssociator::TypeMatchesUserPrefStore(
    const std::string& pref_name,
    const base::Value& new_value) const {
  const base::Value* local_value = nullptr;
  user_pref_store_->GetValue(pref_name, &local_value);
  if (!local_value || local_value->type() == new_value.type())
    return true;

  UMA_HISTOGRAM_BOOLEAN("Sync.Preferences.RemotePrefTypeMismatch", true);
  DLOG(WARNING) << "Unexpected type mis-match for pref. "
                << "Synced value for " << pref_name << " is of type "
                << new_value.type() << " which doesn't match the locally "
                << "present pref type: " << local_value->type();
  return false;
}

void PrefModelAssociator::EnforceRegisteredTypeInStore(
    const std::string& pref_name) {
  const base::Value* persisted_value = nullptr;
  if (user_pref_store_->GetValue(pref_name, &persisted_value)) {
    // Get the registered type (typically from the default value).
    const PrefService::Preference* pref =
        pref_service_->FindPreference(pref_name);
    DCHECK(pref);
    if (pref->GetType() != persisted_value->type()) {
      // We see conflicting type information and there's a chance the local
      // type-conflicting data came in via sync. Remove it.
      // TODO(tschumann): The value should get removed silently. Add a method
      // RemoveValueSilently() to WriteablePrefStore. Note, that as of today
      // that removal will only notify other pref stores but not sync -- that's
      // done on a higher level.
      user_pref_store_->RemoveValue(
          pref_name, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
      UMA_HISTOGRAM_BOOLEAN("Sync.Preferences.ClearedLocalPrefOnTypeMismatch",
                            true);
    }
  }
}

}  // namespace sync_preferences
