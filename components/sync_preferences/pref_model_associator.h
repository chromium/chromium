// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_H_
#define COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_H_

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/prefs/transparent_unordered_string_map.h"
#include "components/prefs/writeable_pref_store.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/synced_pref_observer.h"

namespace base {
class Value;
}

namespace sync_pb {
class EntitySpecifics;
class PreferenceSpecifics;
}  // namespace sync_pb

namespace sync_preferences {

class DualLayerUserPrefStore;
class PrefModelAssociatorClient;

class PrefServiceForAssociator {
 public:
  virtual base::Value::Type GetRegisteredPrefType(
      std::string_view pref_name) const = 0;
  virtual void OnIsSyncingChanged() = 0;
  virtual uint32_t GetWriteFlags(std::string_view pref_name) const = 0;
};

// Contains all preference sync related logic.
class PrefModelAssociator final : public syncer::SyncableService,
                                  public PrefStore::Observer {
 public:
  // The |client| is not owned and must outlive this object.
  // |user_prefs| is the PrefStore to be hooked up to Sync.
  PrefModelAssociator(scoped_refptr<PrefModelAssociatorClient> client,
                      scoped_refptr<WriteablePrefStore> user_prefs,
                      syncer::DataType type);

  // The |client| is not owned and must outlive this object.
  // |user_prefs| is the PrefStore to be hooked up to Sync.
  // Note: This must be called iff EnablePreferencesAccountStorage feature is
  // enabled.
  PrefModelAssociator(
      scoped_refptr<PrefModelAssociatorClient> client,
      scoped_refptr<DualLayerUserPrefStore> dual_layer_user_prefs,
      syncer::DataType type);

  PrefModelAssociator(const PrefModelAssociator&) = delete;
  PrefModelAssociator& operator=(const PrefModelAssociator&) = delete;

  ~PrefModelAssociator() override;

  // Must be called before anything else.
  void SetPrefService(PrefServiceForAssociator* pref_service);

  // See description above field for details.
  bool models_associated() const { return models_associated_; }

  // Returns the mutable preference from |specifics| for a given model |type|.
  // Exposed for testing.
  static sync_pb::PreferenceSpecifics* GetMutableSpecifics(
      syncer::DataType type,
      sync_pb::EntitySpecifics* specifics);

  // syncer::SyncableService implementation.
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  std::optional<syncer::ModelError> MergeDataAndStartSyncing(
      syncer::DataType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) override;
  void StopSyncing(syncer::DataType type) override;
  void OnBrowserShutdown(syncer::DataType type) override;
  std::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  base::WeakPtr<SyncableService> AsWeakPtr() override;

  // PrefStore::Observer implementation.
  void OnPrefValueChanged(std::string_view name) override;
  void OnInitializationCompleted(bool succeeded) override;

  // Register a preference with the specified name for syncing. We do not care
  // about the type at registration time, but when changes arrive from the
  // syncer, we check if they can be applied and if not drop them.
  // Note: This should only be called at profile startup time (before sync
  // begins).
  void RegisterPref(std::string_view name);

  // Fills |sync_data| with a sync representation of the preference data
  // provided.
  // Exposed for testing.
  bool CreatePrefSyncData(std::string_view name,
                          const base::Value& value,
                          syncer::SyncData* sync_data) const;

  // Returns true if the specified preference is registered for syncing.
  bool IsPrefRegistered(std::string_view name) const;

  // Adds a SyncedPrefObserver to watch for changes to a specific pref.
  void AddSyncedPrefObserver(const std::string& name,
                             SyncedPrefObserver* observer);

  // Removes a SyncedPrefObserver from a pref's list of observers.
  void RemoveSyncedPrefObserver(const std::string& name,
                                SyncedPrefObserver* observer);

  // Returns the PrefModelAssociatorClient for this object.
  scoped_refptr<PrefModelAssociatorClient> client() const { return client_; }

  // Returns true if the pref under the given name is pulled down from sync.
  // Note this does not refer to SYNCABLE_PREF.
  bool IsPrefSyncedForTesting(const std::string& name) const;

  bool IsUsingDualLayerUserPrefStoreForTesting() const;

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
                            std::string_view pref_name,
                            syncer::SyncChangeList* sync_changes);

  void NotifySyncedPrefObservers(std::string_view path, bool from_sync) const;

  // Sets |pref_name| to |new_value| and returns true if |new_value| has an
  // appropriate type for this preference. Otherwise returns false.
  bool SetPrefWithTypeCheck(std::string_view pref_name,
                            const base::Value& new_value);

  // Notifies the synced pref observers that the pref for the given |path| is
  // synced.
  void NotifyStartedSyncing(const std::string& path) const;

  void Stop(bool is_browser_shutdown);

  // The datatype that this associator is responsible for, either PREFERENCES or
  // PRIORITY_PREFERENCES or OS_PREFERENCES or OS_PRIORITY_PREFERENCES.
  const syncer::DataType type_;

  scoped_refptr<PrefModelAssociatorClient> client_;

  // The PrefStore we are syncing to.
  scoped_refptr<WriteablePrefStore> user_prefs_;
  // This is set if EnablePreferencesAccountStorage is enabled. This points to
  // the DualLayerUserPrefStore instance, if one exists, which shares the
  // ownership of `user_prefs_`.
  scoped_refptr<DualLayerUserPrefStore> dual_layer_user_prefs_;

  // The interface to the PrefService.
  raw_ptr<PrefServiceForAssociator> pref_service_ = nullptr;

  // Do we have an active association between the preferences and sync models?
  // Set when start syncing, reset in StopSyncing. While this is not set, we
  // ignore any local preference changes (when we start syncing we will look
  // up the most recent values anyways).
  bool models_associated_ = false;

  // Whether we're currently processing changes from the syncer. While this is
  // true, we ignore any local preference changes, since we triggered them.
  bool processing_syncer_changes_ = false;

  // All preferences that have registered as being syncable with this profile.
  std::set<std::string, std::less<>> registered_preferences_;

  // The preferences that are currently synced (excludes those preferences
  // that have never had sync data and currently have default values).
  // Note: As long as Sync remains enabled, this set never decreases, only grows
  // to eventually match `registered_preferences_` as more preferences are
  // synced. It determines whether a preference change should update an existing
  // sync node or create a new sync node.
  std::set<std::string, std::less<>> synced_preferences_;

  // Sync's handler for outgoing changes. Non-null between
  // MergeDataAndStartSyncing() and StopSyncing().
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Map prefs to lists of observers. Observers will receive notification when
  // a pref changes, including the detail of whether or not the change came
  // from sync.
  using SyncedPrefObserverList =
      base::ObserverList<SyncedPrefObserver>::Unchecked;
  TransparentUnorderedStringMap<std::unique_ptr<SyncedPrefObserverList>>
      synced_pref_observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefModelAssociator> weak_ptr_factory_{this};
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_PREF_MODEL_ASSOCIATOR_H_
