// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_GEMINI_THREAD_SYNC_BRIDGE_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_GEMINI_THREAD_SYNC_BRIDGE_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/gemini_thread_specifics.pb.h"

namespace contextual_tasks {

// Sync bridge implementation for Gemini thread data type.
class GeminiThreadSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    // Invoked when the store containing the Gemini Threads is loaded.
    virtual void OnGeminiThreadDataStoreLoaded() = 0;
  };
  GeminiThreadSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory);

  GeminiThreadSyncBridge(const GeminiThreadSyncBridge&) = delete;
  GeminiThreadSyncBridge& operator=(const GeminiThreadSyncBridge&) = delete;
  GeminiThreadSyncBridge(GeminiThreadSyncBridge&&) = delete;
  GeminiThreadSyncBridge& operator=(GeminiThreadSyncBridge&&) = delete;

  ~GeminiThreadSyncBridge() override;

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

  // Returns all threads.
  virtual std::vector<Thread> GetThreads() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  std::unordered_map<std::string, sync_pb::GeminiThreadSpecifics>&
  gemini_thread_specifics_for_testing() {
    return gemini_thread_specifics_;
  }

 private:
  void OnDataTypeStoreCreated(const std::optional<syncer::ModelError>& error,
                              std::unique_ptr<syncer::DataTypeStore> store);

  void OnReadAllData(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> entries);

  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void OnDataTypeStoreCommit(const std::optional<syncer::ModelError>& error);

  std::unique_ptr<syncer::DataTypeStore> data_type_store_;

  std::unordered_map<std::string, sync_pb::GeminiThreadSpecifics>
      gemini_thread_specifics_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<GeminiThreadSyncBridge> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_GEMINI_THREAD_SYNC_BRIDGE_H_
