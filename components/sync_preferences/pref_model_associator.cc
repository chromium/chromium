// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_model_associator.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync_preferences/dual_layer_user_pref_store.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/preferences_merge_helper.h"
#include "components/sync_preferences/syncable_prefs_database.h"

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

}  // namespace

PrefModelAssociator::PrefModelAssociator(
    const PrefModelAssociatorClient* client,
    scoped_refptr<WriteablePrefStore> user_prefs,
    syncer::ModelType type)
    : type_(type),
      client_(client),
      user_prefs_(user_prefs),
      dual_layer_user_prefs_(nullptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(type_ == syncer::PREFERENCES ||
         type_ == syncer::PRIORITY_PREFERENCES ||
         type_ == syncer::OS_PREFERENCES ||
         type_ == syncer::OS_PRIORITY_PREFERENCES);
#else
  DCHECK(type_ == syncer::PREFERENCES || type_ == syncer::PRIORITY_PREFERENCES);
#endif
  user_prefs_->AddObserver(this);
}

PrefModelAssociator::PrefModelAssociator(
    const PrefModelAssociatorClient* client,
    scoped_refptr<DualLayerUserPrefStore> dual_layer_user_prefs,
    syncer::ModelType type)
    : PrefModelAssociator(client,
                          dual_layer_user_prefs->GetAccountPrefStore(),
                          type) {
  CHECK(base::FeatureList::IsEnabled(syncer::kEnablePreferencesAccountStorage));
  dual_layer_user_prefs_ = std::move(dual_layer_user_prefs);
}

PrefModelAssociator::~PrefModelAssociator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_prefs_->RemoveObserver(this);
}

void PrefModelAssociator::SetPrefService(
    PrefServiceForAssociator* pref_service) {
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
  VLOG(1) << "Associating preference " << pref_name;

  const base::Value* user_pref_value = nullptr;
  user_prefs_->GetValue(pref_name, &user_pref_value);

  if (sync_pref.IsValid()) {
    const sync_pb::PreferenceSpecifics& preference = GetSpecifics(sync_pref);
    CHECK_EQ(pref_name, preference.name());
    ASSIGN_OR_RETURN(
        base::Value sync_value,
        base::JSONReader::ReadAndReturnValueWithError(preference.value()),
        [&](base::JSONReader::Error error) {
          LOG(ERROR) << "Failed to deserialize value of preference '"
                     << pref_name << "': " << std::move(error).message;
        });

    if (user_pref_value) {
      DVLOG(1) << "Found user pref value for " << pref_name;
      // We have both server and local values. Merge them if account storage
      // is not supported.
      // TODO(crbug.com/1434943): Consider the case where a value is set before
      // initial merge. This would overwrite the value the user just set.
      base::Value new_value(helper::MergePreference(
          client_, pref_name, *user_pref_value, sync_value));
      // Update the local preference based on what we got from the sync
      // server.
      if (new_value.is_none()) {
        LOG(WARNING) << "Sync has null value for pref " << pref_name.c_str();
        user_prefs_->RemoveValue(pref_name,
                                 pref_service_->GetWriteFlags(pref_name));
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
    // The server does not know about this preference and it should be added
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
  // Else: This pref has neither a sync value nor a user-controlled value, so
  // ignore it for now. If it gets a new user-controlled value in the future,
  // that value will then be sent to the server.
}

void PrefModelAssociator::WaitUntilReadyToSync(base::OnceClosure done) {
  // Prefs are loaded very early during profile initialization, so nothing to
  // wait for here.
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

  if (base::FeatureList::IsEnabled(syncer::kEnablePreferencesAccountStorage)) {
    // Inform the pref store to enable account storage for `type_`.
    dual_layer_user_prefs_->EnableType(type_);
  }

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
  Stop(/*is_browser_shutdown=*/false);
}

void PrefModelAssociator::OnBrowserShutdown(syncer::ModelType type) {
  DCHECK_EQ(type_, type);
  Stop(/*is_browser_shutdown=*/true);
}

void PrefModelAssociator::Stop(bool is_browser_shutdown) {
  models_associated_ = false;
  sync_processor_.reset();
  if (!is_browser_shutdown &&
      base::FeatureList::IsEnabled(syncer::kEnablePreferencesAccountStorage)) {
    // Avoid clearing account store in case of browser shutdown, since it
    // tries to notify the observers which may or may not exist by this time
    // during browser shutdown (crbug.com/1434902).
    dual_layer_user_prefs_->DisableTypeAndClearAccountStore(type_);
  }
  synced_preferences_.clear();
  pref_service_->OnIsSyncingChanged();
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
      user_prefs_->RemoveValue(pref_name,
                               pref_service_->GetWriteFlags(pref_name));
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
  DCHECK(
      !client_ ||
      (client_->GetSyncablePrefsDatabase().IsPreferenceSyncable(name) &&
       client_->GetSyncablePrefsDatabase()
               .GetSyncablePrefMetadata(name)
               ->model_type() == type_))
      << "Preference " << name
      << " has not been added to syncable prefs allowlist, or has incorrect "
         "data.";
  registered_preferences_.insert(name);
}

bool PrefModelAssociator::IsPrefRegistered(const std::string& name) const {
  return registered_preferences_.count(name) > 0;
}

void PrefModelAssociator::OnPrefValueChanged(const std::string& name) {
  if (processing_syncer_changes_) {
    return;  // These are changes originating from us, ignore.
  }

  // We only process changes if we've already associated models.
  // This also filters out local changes during the initial merge.
  if (!models_associated_) {
    return;
  }

  if (!IsPrefRegistered(name)) {
    // We are not syncing this preference -- this also filters out synced
    // preferences of the wrong type (e.g. priority preference are handled by a
    // separate associator).
    return;
  }

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  NotifySyncedPrefObservers(name, /*from_sync=*/false);

  syncer::SyncChangeList changes;

  if (synced_preferences_.count(name) == 0) {
    // Not in synced_preferences_ means no synced data. InitPrefAndAssociate(..)
    // will determine if we care about its data (e.g. if it has a default value
    // and hasn't been changed yet we don't) and take care syncing any new data.
    InitPrefAndAssociate(syncer::SyncData(), name, &changes);
  } else {
    // We are already syncing this preference, just update or delete its sync
    // node.
    const base::Value* user_pref_value = nullptr;
    user_prefs_->GetValue(name, &user_pref_value);
    if (user_pref_value) {
      // If the pref was updated, update it.
      syncer::SyncData sync_data;
      if (!CreatePrefSyncData(name, *user_pref_value, &sync_data)) {
        LOG(ERROR) << "Failed to update preference.";
        return;
      }
      changes.emplace_back(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                           sync_data);
    } else {
      // Otherwise, the pref must have been cleared and hence delete it.
      changes.emplace_back(FROM_HERE, syncer::SyncChange::ACTION_DELETE,
                           syncer::SyncData::CreateLocalDelete(name, type_));
    }
  }

  if (client_ &&
      // Only log if there's actually something to sync.
      !changes.empty()) {
    base::UmaHistogramSparse("Sync.SyncablePrefValueChanged",
                             client_->GetSyncablePrefsDatabase()
                                 .GetSyncablePrefMetadata(name)
                                 ->syncable_pref_id());
  }

  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void PrefModelAssociator::OnInitializationCompleted(bool succeeded) {}

void PrefModelAssociator::NotifySyncedPrefObservers(const std::string& path,
                                                    bool from_sync) const {
  auto observer_iter = synced_pref_observers_.find(path);
  if (observer_iter == synced_pref_observers_.end()) {
    return;
  }
  for (auto& observer : *observer_iter->second) {
    observer.OnSyncedPrefChanged(path, from_sync);
  }
}

bool PrefModelAssociator::SetPrefWithTypeCheck(const std::string& pref_name,
                                               const base::Value& new_value) {
  base::Value::Type registered_type =
      pref_service_->GetRegisteredPrefType(pref_name);
  if (registered_type != new_value.type()) {
    DLOG(WARNING) << "Unexpected type mis-match for pref. "
                  << "Synced value for " << pref_name << " is of type "
                  << new_value.type() << " which doesn't match the registered "
                  << "pref type: " << registered_type;
    return false;
  }
  if (base::FeatureList::IsEnabled(syncer::kEnablePreferencesAccountStorage)) {
    CHECK(dual_layer_user_prefs_);
    // `dual_layer_user_prefs_->SetValueInAccountStoreOnly()` is almost
    // equivalent to `user_prefs_->SetValue()` except that if the effective
    // value of the pref for the `dual_layer_user_prefs_` is unchanged, no
    // notifications are sent out to its observers.
    dual_layer_user_prefs_->SetValueInAccountStoreOnly(
        pref_name, new_value.Clone(), pref_service_->GetWriteFlags(pref_name));
    return true;
  }
  // Write directly to the user controlled value store, which is ignored if the
  // preference is controlled by a higher-priority layer (e.g. policy).
  user_prefs_->SetValue(pref_name, new_value.Clone(),
                        pref_service_->GetWriteFlags(pref_name));
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

bool PrefModelAssociator::IsUsingDualLayerUserPrefStoreForTesting() const {
  return dual_layer_user_prefs_.get();
}

}  // namespace sync_preferences
