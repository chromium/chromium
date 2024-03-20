// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_COLLABORATION_GROUP_SYNC_BRIDGE_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_COLLABORATION_GROUP_SYNC_BRIDGE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"

namespace data_sharing {

// Sync bridge implementation for COLLABORATION_GROUP model type.
class CollaborationGroupSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  CollaborationGroupSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory store_factory);

  CollaborationGroupSyncBridge(const CollaborationGroupSyncBridge&) = delete;
  CollaborationGroupSyncBridge& operator=(const CollaborationGroupSyncBridge&) =
      delete;
  CollaborationGroupSyncBridge(CollaborationGroupSyncBridge&&) = delete;
  CollaborationGroupSyncBridge& operator=(CollaborationGroupSyncBridge&&) =
      delete;

  ~CollaborationGroupSyncBridge() override;

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_change_list) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Methods used as callbacks given to `model_type_store_`.
  void OnModelTypeStoreCreated(const std::optional<syncer::ModelError>& error,
                               std::unique_ptr<syncer::ModelTypeStore> store);
  void OnReadAllData(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> record_list);
  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnModelTypeStoreCommit(const std::optional<syncer::ModelError>& error);

  // In charge of actually persisting data to disk, or loading previous data.
  std::unique_ptr<syncer::ModelTypeStore> model_type_store_;

  // Maps `collaboration_id` (also known as group id) to specifics. Used as
  // in-memory cache of the data.
  std::unordered_map<std::string, sync_pb::CollaborationGroupSpecifics>
      ids_to_specifics_;

  base::WeakPtrFactory<CollaborationGroupSyncBridge> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_COLLABORATION_GROUP_SYNC_BRIDGE_H_
