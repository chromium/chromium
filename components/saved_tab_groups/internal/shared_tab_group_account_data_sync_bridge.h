// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_ACCOUNT_DATA_SYNC_BRIDGE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_ACCOUNT_DATA_SYNC_BRIDGE_H_

#include <optional>
#include <unordered_map>

#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/internal/sync_data_type_configuration.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/model_error.h"

namespace syncer {
class MetadataChangeList;
}  // namespace syncer

namespace tab_groups {

// Sync bridge implementation for SHARED_TAB_GROUP_ACCOUNT_DATA data type.
class SharedTabGroupAccountDataSyncBridge : public syncer::DataTypeSyncBridge,
                                            public SavedTabGroupModelObserver {
 public:
  explicit SharedTabGroupAccountDataSyncBridge(
      std::unique_ptr<SyncDataTypeConfiguration> configuration,
      SavedTabGroupModel& model);

  SharedTabGroupAccountDataSyncBridge(
      const SharedTabGroupAccountDataSyncBridge&) = delete;
  SharedTabGroupAccountDataSyncBridge& operator=(
      const SharedTabGroupAccountDataSyncBridge&) = delete;
  ~SharedTabGroupAccountDataSyncBridge() override;

  // DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_change_list) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;
  syncer::ConflictResolution ResolveConflict(
      const std::string& storage_key,
      const syncer::EntityData& remote_data) const override;

  // SavedTabGroupModelObserver implementation.
  void SavedTabGroupModelLoaded() override;
  void SavedTabGroupTabLastSeenTimeUpdated(const base::Uuid& saved_tab_id,
                                           TriggerSource source) override;
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override;
  void SavedTabGroupAddedFromSync(const base::Uuid& guid) override;
  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupRemovedLocally(const SavedTabGroup& removed_group) override;
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup& removed_group) override;
  void SavedTabGroupReorderedLocally() override;

  // Returns whether the sync bridge has initialized by reading data
  // from the on-disk store.
  bool IsInitialized() const;

  // Returns whether the bridge is tracking the storage key for this tab.
  bool HasSpecificsForTab(const SavedTabGroupTab& tab) const;
  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>
  GetSpecificsForStorageKey(const std::string& storage_key) const;

 private:
  // Loads the data already stored in the DataTypeStore.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);

  // Calls ModelReadyToSync if there are no errors to report and propagators the
  // stored entries to `on_load_callback`.
  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> entries,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void OnDataTypeStoreCommit(const std::optional<syncer::ModelError>& error);

  // Update the TabDetails model. If the SavedTabGroupTab cannot be
  // found, this tab is added to `storage_keys_for_missing_tab_` so the
  // model can be updated when it does exist.
  void UpdateTabDetailsModel(
      const sync_pb::SharedTabGroupAccountDataSpecifics& specifics);

  void UpdateTabGroupDetailsModel(
      const sync_pb::SharedTabGroupAccountDataSpecifics& specifics);

  // Look for tabs specified in `storage_keys_for_missing_tabs_` and
  // apply their corresponding model updates.
  void ApplyMissingTabData();

  // Look for tab groups specified in `storage_keys_for_missing_tab_groups_` and
  // apply their corresponding model updates.
  void ApplyMissingTabGroupData();

  // Write a new entity to sync. This is used when the model is updated
  // with a new value and sync needs to be triggered.
  void WriteEntityToSync(std::unique_ptr<syncer::EntityData> entity);

  // Delete an entity from sync. Also deletes from local storage and in-memory
  // cache.
  void RemoveEntitySpecifics(const std::string& storage_key);

  // Conversion method to create a EntityData object for a given
  // SavedTabGroupTab. Tab group must exist and be shared, and tab must have a
  // "last seen" time set.
  std::unique_ptr<syncer::EntityData> CreateEntityDataFromSavedTabGroupTab(
      const SavedTabGroupModel& model,
      const SavedTabGroupTab& tab);

  // Conversion method to create a EntityData object for a given
  // SavedTabGroup.
  std::unique_ptr<syncer::EntityData> CreateEntityDataFromSharedTabGroup(
      const SavedTabGroupModel& model,
      const SavedTabGroup& tab_group);

  // Remove tab details on tab group update locally or from sync if available.
  void MaybeRemoveTabDetailsOnGroupUpdate(
      const SavedTabGroup& group,
      const std::optional<base::Uuid>& tab_guid);

  // Write tab group detail to sync only if the tab group details has changed.
  void WriteTabGroupDetailToSyncIfPositionChanged(const SavedTabGroup& group);

  SEQUENCE_CHECKER(sequence_checker_);

  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<syncer::DataTypeStore> store_;

  const raw_ref<SavedTabGroupModel> model_;

  // Set to true once data is loaded from disk into the in-memory cache.
  bool is_initialized_ = false;

  // In-memory data cache of specifics, keyed by its storage key.
  std::unordered_map<std::string, sync_pb::SharedTabGroupAccountDataSpecifics>
      specifics_;

  // Storage keys for specifics where the SavedTabGroupTab does not
  // currently exist. The tab may be created later by
  // SharedTabGroupDataSyncBridge, so specifics for these keys are
  // still stored in sync bridge cache as well as written to disk.
  std::set<std::string> storage_keys_for_missing_tabs_;

  std::set<std::string> storage_keys_for_missing_tab_groups_;

  // Observes the SavedTabGroupModel.
  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      saved_tab_group_model_observation_{this};

  // Allows safe temporary use of the SharedTabGroupAccountDataSyncBridge
  // object if it exists at the time of use.
  base::WeakPtrFactory<SharedTabGroupAccountDataSyncBridge> weak_ptr_factory_{
      this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_ACCOUNT_DATA_SYNC_BRIDGE_H_
