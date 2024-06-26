// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SHARED_TAB_GROUP_DATA_SYNC_BRIDGE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SHARED_TAB_GROUP_DATA_SYNC_BRIDGE_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

class PrefService;

namespace syncer {
class MetadataChangeList;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace sync_pb {
class SharedTabGroupDataSpecifics;
}  // namespace sync_pb

namespace tab_groups {
class SavedTabGroup;
class SavedTabGroupModel;

// Sync bridge implementation for SHARED_TAB_GROUP_DATA model type.
class SharedTabGroupDataSyncBridge : public syncer::ModelTypeSyncBridge,
                                     public SavedTabGroupModelObserver {
 public:
  SharedTabGroupDataSyncBridge(
      SavedTabGroupModel* model,
      syncer::OnceModelTypeStoreFactory create_store_callback,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      PrefService* pref_service);

  SharedTabGroupDataSyncBridge(const SharedTabGroupDataSyncBridge&) = delete;
  SharedTabGroupDataSyncBridge& operator=(const SharedTabGroupDataSyncBridge&) =
      delete;
  ~SharedTabGroupDataSyncBridge() override;

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetDataForCommit(StorageKeyList storage_keys,
                        DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  bool SupportsIncrementalUpdates() const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

  // SavedTabGroupModelObserver overrides.
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override;
  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupRemovedLocally(const SavedTabGroup& removed_group) override;
  // TODO(crbug.com/319521964): implement the following methods.
  // void SavedTabGroupTabsReorderedLocally(const base::Uuid& group_guid)
  // override;
  // void SavedTabGroupReorderedLocally() override;
  // void SavedTabGroupLocalIdChanged(const base::Uuid& group_guid) override;

 private:
  // Loads the data already stored in the ModelTypeStore.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);

  // Calls ModelReadyToSync if there are no errors to report and loads the
  // stored entries into `model_`.
  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  // React to store failures if a save was not successful.
  void OnDatabaseSave(const std::optional<syncer::ModelError>& error);

  // Adds `specifics` into local storage (SavedTabGroupModel, and
  // ModelTypeStore) and resolves any conflicts if `specifics` already exists
  // locally.  Additionally, the list of changes may not be complete and tabs
  // may have been sent before their groups have arrived. In this case, the tabs
  // are saved in the ModelTypeStore but not in the model (and instead cached in
  // this class).
  void AddGroupToLocalStorage(
      const sync_pb::SharedTabGroupDataSpecifics& specifics,
      const std::string& collaboration_id,
      syncer::MetadataChangeList* metadata_change_list,
      syncer::ModelTypeStore::WriteBatch* write_batch);
  void AddTabToLocalStorage(
      const sync_pb::SharedTabGroupDataSpecifics& specifics,
      syncer::MetadataChangeList* metadata_change_list,
      syncer::ModelTypeStore::WriteBatch* write_batch);

  // Removes all data assigned to `storage_key` from local storage
  // (SavedTabGroupModel, and ModelTypeStore). If a group is removed, all its
  // tabs will be removed in addition to the group.
  void DeleteDataFromLocalStorage(
      const std::string& storage_key,
      syncer::ModelTypeStore::WriteBatch* write_batch);

  // Inform the processor of a new or updated Shared Tab Group or Tab.
  void SendToSync(sync_pb::SharedTabGroupDataSpecifics specific,
                  const std::string& collaboration_id,
                  syncer::MetadataChangeList* metadata_change_list);

  // Updates or adds the `specifics` into the `store_` and populates it to the
  // processor.
  void UpsertEntitySpecifics(
      const sync_pb::SharedTabGroupDataSpecifics& specifics,
      const std::string& collaboration_id,
      syncer::ModelTypeStore::WriteBatch* write_batch);

  // Process local tab changes (add, remove, update), excluding changing tab's
  // position.
  void ProcessTabLocalUpdate(const SavedTabGroup& group,
                             const base::Uuid& tab_id,
                             syncer::ModelTypeStore::WriteBatch* write_batch);

  // Removes the specifics pointed to by `guid` from the `store_`.
  void RemoveEntitySpecifics(const base::Uuid& guid,
                             syncer::ModelTypeStore::WriteBatch* write_batch);

  SEQUENCE_CHECKER(sequence_checker_);

  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  // The Model used to represent the current state of saved and shared tab
  // groups.
  raw_ptr<SavedTabGroupModel> model_;

  // Observes changes to the `model_`.
  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      observation_{this};

  // Allows safe temporary use of the SharedTabGroupDataSyncBridge object if it
  // exists at the time of use.
  base::WeakPtrFactory<SharedTabGroupDataSyncBridge> weak_ptr_factory_{this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SHARED_TAB_GROUP_DATA_SYNC_BRIDGE_H_
