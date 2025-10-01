// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_AI_THREAD_SYNC_BRIDGE_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_AI_THREAD_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
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
class AiThreadSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    virtual void OnThreadAddedOrUpdatedRemotely(
        const std::vector<Thread>& threads) = 0;
    virtual void OnThreadRemovedRemotely(
        const std::vector<Thread>& threads) = 0;
  };

  explicit AiThreadSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  AiThreadSyncBridge(const AiThreadSyncBridge&) = delete;
  AiThreadSyncBridge& operator=(const AiThreadSyncBridge&) = delete;
  AiThreadSyncBridge(AiThreadSyncBridge&&) = delete;
  AiThreadSyncBridge& operator=(AiThreadSyncBridge&&) = delete;

  ~AiThreadSyncBridge() override;

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

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_AI_THREAD_SYNC_BRIDGE_H_
