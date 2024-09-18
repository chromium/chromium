// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_COLLABORATION_GROUP_SYNC_BRIDGE_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_COLLABORATION_GROUP_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/data_sharing/public/group_data.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"

namespace data_sharing {

// Sync bridge implementation for COLLABORATION_GROUP data type.
class CollaborationGroupSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    virtual void OnGroupsUpdated(
        const std::vector<GroupId>& added_group_ids,
        const std::vector<GroupId>& updated_group_ids,
        const std::vector<GroupId>& deleted_group_ids) = 0;
    virtual void OnDataLoaded() = 0;
  };

  CollaborationGroupSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory);

  CollaborationGroupSyncBridge(const CollaborationGroupSyncBridge&) = delete;
  CollaborationGroupSyncBridge& operator=(const CollaborationGroupSyncBridge&) =
      delete;
  CollaborationGroupSyncBridge(CollaborationGroupSyncBridge&&) = delete;
  CollaborationGroupSyncBridge& operator=(CollaborationGroupSyncBridge&&) =
      delete;

  ~CollaborationGroupSyncBridge() override;

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
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

  // Own methods.
  // Returns ids of all synced (not deleted) collaboration groups.
  std::vector<GroupId> GetCollaborationGroupIds() const;
  std::optional<sync_pb::CollaborationGroupSpecifics> GetSpecifics(
      const GroupId& group_id) const;
  bool IsDataLoaded() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Methods used as callbacks given to `data_type_store_`.
  void OnDataTypeStoreCreated(const std::optional<syncer::ModelError>& error,
                              std::unique_ptr<syncer::DataTypeStore> store);
  void OnReadAllData(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> record_list);
  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnDataTypeStoreCommit(const std::optional<syncer::ModelError>& error);

  // In charge of actually persisting data to disk, or loading previous data.
  std::unique_ptr<syncer::DataTypeStore> data_type_store_;

  // Set to true once data is loaded from disk and `ids_to_specifics_` contains
  // the actual data.
  bool is_data_loaded_ = false;

  // Maps `collaboration_id` (also known as group id) to specifics. Used as
  // in-memory cache of the data.
  std::unordered_map<std::string, sync_pb::CollaborationGroupSpecifics>
      ids_to_specifics_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CollaborationGroupSyncBridge> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_COLLABORATION_GROUP_SYNC_BRIDGE_H_
