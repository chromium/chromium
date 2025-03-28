// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_ACCOUNT_DATA_SYNC_BRIDGE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_ACCOUNT_DATA_SYNC_BRIDGE_H_

#include <optional>

#include "base/sequence_checker.h"
#include "components/saved_tab_groups/internal/sync_data_type_configuration.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/model_error.h"

namespace syncer {
class MetadataChangeList;
}  // namespace syncer

namespace tab_groups {

// Sync bridge implementation for SHARED_TAB_GROUP_ACCOUNT_DATA data type.
class SharedTabGroupAccountDataSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  explicit SharedTabGroupAccountDataSyncBridge(
      std::unique_ptr<SyncDataTypeConfiguration> configuration);

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
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

  // Returns whether the sync bridge has initialized by reading data
  // from the on-disk store.
  bool IsInitialized() const;

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

  SEQUENCE_CHECKER(sequence_checker_);

  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<syncer::DataTypeStore> store_;

  // Set to true once data is loaded from disk into the in-memory cache.
  bool is_initialized_ = false;

  // In-memory data cache of specifics, keyed by its storage key.
  std::unordered_map<std::string, sync_pb::SharedTabGroupAccountDataSpecifics>
      specifics_;

  // Allows safe temporary use of the SharedTabGroupAccountDataSyncBridge
  // object if it exists at the time of use.
  base::WeakPtrFactory<SharedTabGroupAccountDataSyncBridge> weak_ptr_factory_{
      this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_ACCOUNT_DATA_SYNC_BRIDGE_H_
