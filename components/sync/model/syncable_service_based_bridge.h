// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_BASED_BRIDGE_H_
#define COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_BASED_BRIDGE_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_change_processor.h"

namespace sync_pb {
class PersistedEntityData;
}

namespace syncer {

class DataTypeLocalChangeProcessor;
class MetadataBatch;
class SyncableService;

// Implementation of DataTypeSyncBridge that allows integrating legacy
// datatypes that implement SyncableService. Internally, it uses a database to
// persist and mimic the legacy Directory's behavior, but as opposed to the
// legacy Directory, it's not exposed anywhere outside this bridge, and is
// considered an implementation detail.
class SyncableServiceBasedBridge : public DataTypeSyncBridge {
 public:
  using InMemoryStore = std::map<std::string, sync_pb::PersistedEntityData>;

  // Pointers must not be null and |syncable_service| must outlive this object.
  SyncableServiceBasedBridge(
      DataType type,
      OnceDataTypeStoreFactory store_factory,
      std::unique_ptr<DataTypeLocalChangeProcessor> change_processor,
      SyncableService* syncable_service);

  SyncableServiceBasedBridge(const SyncableServiceBasedBridge&) = delete;
  SyncableServiceBasedBridge& operator=(const SyncableServiceBasedBridge&) =
      delete;

  ~SyncableServiceBasedBridge() override;

  // DataTypeSyncBridge implementation.
  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  std::optional<ModelError> MergeFullSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_change_list) override;
  std::optional<ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_change_list) override;
  std::unique_ptr<DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(const EntityData& entity_data) override;
  std::string GetStorageKey(const EntityData& entity_data) override;
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  ConflictResolution ResolveConflict(
      const std::string& storage_key,
      const EntityData& remote_data) const override;
  void ApplyDisableSyncChanges(
      std::unique_ptr<MetadataChangeList> delete_metadata_change_list) override;
  size_t EstimateSyncOverheadMemoryUsage() const override;

  // For testing.
  static std::unique_ptr<SyncChangeProcessor>
  CreateLocalChangeProcessorForTesting(DataType type,
                                       DataTypeStore* store,
                                       InMemoryStore* in_memory_store,
                                       DataTypeLocalChangeProcessor* other);

 private:
  void OnStoreCreated(const std::optional<ModelError>& error,
                      std::unique_ptr<DataTypeStore> store);
  void OnReadAllDataForInit(std::unique_ptr<InMemoryStore> in_memory_store,
                            const std::optional<ModelError>& error);
  void OnReadAllMetadataForInit(const std::optional<ModelError>& error,
                                std::unique_ptr<MetadataBatch> metadata_batch);
  void OnSyncableServiceReady(std::unique_ptr<MetadataBatch> metadata_batch);
  [[nodiscard]] std::optional<ModelError> StartSyncableService();
  SyncChangeList StoreAndConvertRemoteChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList input_entity_change_list);
  void ReportErrorIfSet(const std::optional<ModelError>& error);

  const DataType type_;
  const raw_ptr<SyncableService> syncable_service_;

  std::unique_ptr<DataTypeStore> store_;
  bool syncable_service_started_ = false;

  // In-memory copy of |store_|.
  InMemoryStore in_memory_store_;

  // Time when this object was created, and store creation/loading was started.
  base::Time init_start_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SyncableServiceBasedBridge> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_BASED_BRIDGE_H_
