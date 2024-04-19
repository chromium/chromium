// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SHARED_TAB_GROUP_DATA_SYNC_BRIDGE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SHARED_TAB_GROUP_DATA_SYNC_BRIDGE_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class MetadataChangeList;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace tab_groups {
class SavedTabGroupModel;

// Sync bridge implementation for SHARED_TAB_GROUP_DATA model type.
class SharedTabGroupDataSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  SharedTabGroupDataSyncBridge(
      SavedTabGroupModel* model,
      syncer::OnceModelTypeStoreFactory create_store_callback,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);

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
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
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

 private:
  // Loads the data already stored in the ModelTypeStore.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);

  // Loads all sync_pb::SharedTabGroupDataSpecifics stored in `entries`
  // passing the specifics into OnReadAllMetadata.
  void OnDatabaseLoad(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries);

  // Calls ModelReadyToSync if there are no errors to report and loads the
  // stored entries into `model_`.
  void OnReadAllMetadata(
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries,
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  SEQUENCE_CHECKER(sequence_checker_);
  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  // The Model used to represent the current state of saved and shared tab
  // groups.
  raw_ptr<SavedTabGroupModel> model_;

  // Allows safe temporary use of the SharedTabGroupDataSyncBridge object if it
  // exists at the time of use.
  base::WeakPtrFactory<SharedTabGroupDataSyncBridge> weak_ptr_factory_{this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SHARED_TAB_GROUP_DATA_SYNC_BRIDGE_H_
