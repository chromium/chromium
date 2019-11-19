// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_H_
#define COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync_preferences/synced_pref_observer.h"

class PersistentPrefStore;

namespace base {
class Value;
}

namespace sync_pb {
class EntitySpecifics;
class PreferenceSpecifics;
}

namespace sync_preferences {

class PrefModelAssociatorClient;
class PrefServiceSyncable;

// Contains all preference sync related logic.
// TODO(sync): Merge this into PrefService once we separate the profile
// PrefService from the local state PrefService.
class PrefModelAssociator : public syncer::SyncableService {
 public:
  // Constructs a PrefModelAssociator initializing the |client_| and |type_|
  // instance variable. The |client| and |user_pref_store| are not owned by this
  // object and they must outlive the PrefModelAssociator.
  PrefModelAssociator(const PrefModelAssociatorClient* client,
                      syncer::ModelType type,
                      PersistentPrefStore* user_pref_store);
  ~PrefModelAssociator() override;

  // See description above field for details.
  bool models_associated() const { return models_associated_; }

  // Returns the mutable preference from |specifics| for a given model |type|.
  // Exposed for testing.
  static sync_pb::PreferenceSpecifics* GetMutableSpecifics(
      syncer::ModelType type,
      sync_pb::EntitySpecifics* specifics);

  // syncer::SyncableService implementation.
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncError ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  // Note for GetAllSyncData: This will build a model of all preferences
  // registered as syncable with user controlled data. We do not track any
  // information for preferences not registered locally as syncable and do not
  // inform the syncer of non-user controlled preferences.
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;

  // Register a preference with the specified name for syncing. We do not care
  // about the type at registration time, but when changes arrive from the
  // syncer, we check if they can be applied and if not drop them.
  // Note: This should only be called at profile startup time (before sync
  // begins).
  void RegisterPref(const std::string& name);

  // See |legacy_model_type_preferences_|.
  void RegisterPrefWithLegacyModelType(const std::string& name);

  // Process a local preference change. This can trigger new SyncChanges being
  // sent to the syncer.
  void ProcessPrefChange(const std::string& name);

  void SetPrefService(PrefServiceSyncable* pref_service);

  // Merges the local_value into the supplied server_value and returns
  // the result (caller takes ownership). If there is a conflict, the server
  // value always takes precedence. Note that only certain preferences will
  // actually be merged, all others will return a copy of the server value. See
  // the method's implementation for details.
  std::unique_ptr<base::Value> MergePreference(const std::string& name,
                                               const base::Value& local_value,
                                               const base::Value& server_value);

  // Fills |sync_data| with a sync representation of the preference data
  // provided.
  bool CreatePrefSyncData(const std::string& name,
                          const base::Value& value,
                          syncer::SyncData* sync_data) const;

  // Returns true if the specified preference is registered for syncing.
  bool IsPrefRegistered(const std::string& name) const;

  // See |legacy_model_type_preferences_|.
  // Exposed for testing.
  bool IsLegacyModelTypePref(const std::string& name) const;

  // Adds a SyncedPrefObserver to watch for changes to a specific pref.
  void AddSyncedPrefObserver(const std::string& name,
                             SyncedPrefObserver* observer);

  // Removes a SyncedPrefObserver from a pref's list of observers.
  void RemoveSyncedPrefObserver(const std::string& name,
                                SyncedPrefObserver* observer);

  // Returns the PrefModelAssociatorClient for this object.
  const PrefModelAssociatorClient* client() const { return client_; }

  // Returns true if the pref under the given name is pulled down from sync.
  // Note this does not refer to SYNCABLE_PREF.
  bool IsPrefSyncedForTesting(const std::string& name) const;

 private:
  // Create an association for a given preference. If |sync_pref| is valid,
  // signifying that sync has data for this preference, we reconcile their data
  // with ours and append a new UPDATE SyncChange to |sync_changes|. If
  // sync_pref is not set, we append an ADD SyncChange to |sync_changes| with
  // the current preference data.
  // Note: We do not modify the sync data for preferences that are either
  // controlled by policy (are not user modifiable) or have their default value
  // (are not user controlled).
  void InitPrefAndAssociate(const syncer::SyncData& sync_pref,
                            const std::string& pref_name,
                            syncer::SyncChangeList* sync_changes);

  static std::unique_ptr<base::Value> MergeListValues(
      const base::Value& from_value,
      const base::Value& to_value);

  static base::Value MergeDictionaryValues(const base::Value& from_value,
                                           const base::Value& to_value);

  // Extract preference value from sync specifics.
  static base::Value* ReadPreferenceSpecifics(
      const sync_pb::PreferenceSpecifics& specifics);

  void NotifySyncedPrefObservers(const std::string& path, bool from_sync) const;

  // Sets |pref_name| to |new_value| if |new_value| has an appropriate type for
  // this preference. Otherwise records metrics and logs a warning.
  void SetPrefWithTypeCheck(const std::string& pref_name,
                            const base::Value& new_value);

  // Returns true if the |new_value| for |pref_name| has the same type as the
  // existing value in the user's local pref store. If the types don't match,
  // records metrics and logs a warning.
  bool TypeMatchesUserPrefStore(const std::string& pref_name,
                                const base::Value& new_value) const;

  // Verifies that the type which preference |pref_name| was registered with
  // matches the type of any persisted value. On mismatch, the persisted value
  // gets removed.
  void EnforceRegisteredTypeInStore(const std::string& pref_name);

  // Do we have an active association between the preferences and sync models?
  // Set when start syncing, reset in StopSyncing. While this is not set, we
  // ignore any local preference changes (when we start syncing we will look
  // up the most recent values anyways).
  bool models_associated_ = false;

  // Whether we're currently processing changes from the syncer. While this is
  // true, we ignore any local preference changes, since we triggered them.
  bool processing_syncer_changes_ = false;

  // A set of preference names.
  typedef std::set<std::string> PreferenceSet;

  // All preferences that have registered as being syncable with this profile.
  PreferenceSet registered_preferences_;

  // The preferences that are currently synced (excludes those preferences
  // that have never had sync data and currently have default values or are
  // policy controlled).
  // Note: this set never decreases, only grows to eventually match
  // registered_preferences_ as more preferences are synced. It determines
  // whether a preference change should update an existing sync node or create
  // a new sync node.
  PreferenceSet synced_preferences_;

  // Preferences that have migrated to a new ModelType. They are included here
  // so updates can be sent back to older clients with this old ModelType.
  // Updates received from older clients will be ignored. The common case is
  // migration from PREFERENCES to OS_PREFERENCES. This field can be removed
  // after 10/2020.
  PreferenceSet legacy_model_type_preferences_;

  // The PrefService we are syncing to.
  PrefServiceSyncable* pref_service_ = nullptr;

  // Sync's syncer::SyncChange handler. We push all our changes through this.
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Sync's error handler. We use this to create sync errors.
  std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory_;

  // The datatype that this associator is responible for, either PREFERENCES or
  // PRIORITY_PREFERENCES or OS_PREFERENCES or OS_PRIORITY_PREFERENCES.
  syncer::ModelType type_;

  // Map prefs to lists of observers. Observers will receive notification when
  // a pref changes, including the detail of whether or not the change came
  // from sync.
  using SyncedPrefObserverList =
      base::ObserverList<SyncedPrefObserver>::Unchecked;
  std::unordered_map<std::string, std::unique_ptr<SyncedPrefObserverList>>
      synced_pref_observers_;
  const PrefModelAssociatorClient* client_;  // Weak.

  PersistentPrefStore* const user_pref_store_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PrefModelAssociator);
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_H_
