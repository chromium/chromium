// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASK_SYNC_BRIDGE_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASK_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/contextual_tasks/internal/proto/contextual_task_entity.pb.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"

namespace contextual_tasks {

// Sync bridge implementation for AI thread data type.
class ContextualTaskSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    virtual void OnContextualTaskDataStoreLoaded(
        const std::vector<proto::ContextualTaskEntity>& task_entities) = 0;
    virtual void OnTaskAddedOrUpdatedRemotely(
        const std::vector<proto::ContextualTaskEntity>& task_entities) = 0;
    virtual void OnTaskRemovedRemotely(
        const std::vector<base::Uuid>& task_entities) = 0;
  };

  ContextualTaskSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory);

  ContextualTaskSyncBridge(const ContextualTaskSyncBridge&) = delete;
  ContextualTaskSyncBridge& operator=(const ContextualTaskSyncBridge&) = delete;
  ContextualTaskSyncBridge(ContextualTaskSyncBridge&&) = delete;
  ContextualTaskSyncBridge& operator=(ContextualTaskSyncBridge&&) = delete;

  ~ContextualTaskSyncBridge() override;

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
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class ContextualTaskSyncBridgeTest;

  void OnDataTypeStoreCreated(const std::optional<syncer::ModelError>& error,
                              std::unique_ptr<syncer::DataTypeStore> store);

  void OnReadAllData(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> entries);

  std::optional<proto::ContextualTaskEntity> GetEntityProto(
      const std::string& guid);

  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void OnDataTypeStoreCommit(const std::optional<syncer::ModelError>& error);

  base::ObserverList<Observer> observers_;

  // In charge of actually persisting data to disk, or loading previous data.
  std::unique_ptr<syncer::DataTypeStore> data_type_store_;

  base::WeakPtrFactory<ContextualTaskSyncBridge> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASK_SYNC_BRIDGE_H_
