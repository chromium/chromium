// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOK_SYNC_BRIDGE_H_
#define COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOK_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/notebooks/internal/notebooks_model.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/notebook_specifics.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace notebooks {

// DataTypeSyncBridge implementation for NOTEBOOK sync data.
class NotebookSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  NotebookSyncBridge(
      NotebooksModel* model,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory);

  NotebookSyncBridge(const NotebookSyncBridge&) = delete;
  NotebookSyncBridge& operator=(const NotebookSyncBridge&) = delete;

  ~NotebookSyncBridge() override;

  // syncer::DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
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
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

  const absl::flat_hash_map<std::string, sync_pb::NotebookSpecifics>&
  entries_for_testing() const {
    return entries_;
  }

 private:
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);
  void OnReadAllData(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> records);
  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnCommit(const std::optional<syncer::ModelError>& error);

  std::unique_ptr<syncer::DataTypeStore> store_;
  absl::flat_hash_map<std::string, sync_pb::NotebookSpecifics> entries_;

  const raw_ref<NotebooksModel> model_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NotebookSyncBridge> weak_ptr_factory_{this};
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOK_SYNC_BRIDGE_H_
