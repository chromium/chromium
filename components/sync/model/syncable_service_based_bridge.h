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
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model/sync_change_processor.h"

namespace sync_pb {
class EntitySpecifics;
}

namespace syncer {

class MetadataBatch;
class ModelTypeChangeProcessor;
class SyncableService;

// Implementation of ModelTypeSyncBridge that allows integrating legacy
// datatypes that implement SyncableService. Internally, it uses a database to
// persist and mimic the legacy Directory's behavior, but as opposed to the
// legacy Directory, it's not exposed anywhere outside this bridge, and is
// considered an implementation detail.
class SyncableServiceBasedBridge : public ModelTypeSyncBridge {
 public:
  using InMemoryStore = std::map<std::string, sync_pb::EntitySpecifics>;

  // Pointers must not be null and |syncable_service| must outlive this object.
  SyncableServiceBasedBridge(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      std::unique_ptr<ModelTypeChangeProcessor> change_processor,
      SyncableService* syncable_service);

  SyncableServiceBasedBridge(const SyncableServiceBasedBridge&) = delete;
  SyncableServiceBasedBridge& operator=(const SyncableServiceBasedBridge&) =
      delete;

  ~SyncableServiceBasedBridge() override;

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  absl::optional<ModelError> MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_change_list) override;
  absl::optional<ModelError> ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_change_list) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const EntityData& entity_data) override;
  std::string GetStorageKey(const EntityData& entity_data) override;
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  ConflictResolution ResolveConflict(
      const std::string& storage_key,
      const EntityData& remote_data) const override;
  void ApplyStopSyncChanges(
      std::unique_ptr<MetadataChangeList> delete_metadata_change_list) override;
  size_t EstimateSyncOverheadMemoryUsage() const override;

  // For testing.
  static std::unique_ptr<SyncChangeProcessor>
  CreateLocalChangeProcessorForTesting(ModelType type,
                                       ModelTypeStore* store,
                                       InMemoryStore* in_memory_store,
                                       ModelTypeChangeProcessor* other);

 private:
  void OnStoreCreated(const absl::optional<ModelError>& error,
                      std::unique_ptr<ModelTypeStore> store);
  void OnReadAllDataForInit(std::unique_ptr<InMemoryStore> in_memory_store,
                            const absl::optional<ModelError>& error);
  void OnReadAllMetadataForInit(const absl::optional<ModelError>& error,
                                std::unique_ptr<MetadataBatch> metadata_batch);
  void OnSyncableServiceReady(std::unique_ptr<MetadataBatch> metadata_batch);
  [[nodiscard]] absl::optional<ModelError> StartSyncableService();
  SyncChangeList StoreAndConvertRemoteChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList input_entity_change_list);
  void OnReadDataForProcessor(
      DataCallback callback,
      const absl::optional<ModelError>& error,
      std::unique_ptr<ModelTypeStore::RecordList> record_list,
      std::unique_ptr<ModelTypeStore::IdList> missing_id_list);
  void OnReadAllDataForProcessor(
      DataCallback callback,
      const absl::optional<ModelError>& error,
      std::unique_ptr<ModelTypeStore::RecordList> record_list);
  void ReportErrorIfSet(const absl::optional<ModelError>& error);

  const ModelType type_;
  const raw_ptr<SyncableService> syncable_service_;

  std::unique_ptr<ModelTypeStore> store_;
  bool syncable_service_started_;

  // In-memory copy of |store_|, needed for remote deletions, because we need to
  // provide specifics of the deleted entity to the SyncableService.
  InMemoryStore in_memory_store_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SyncableServiceBasedBridge> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_BASED_BRIDGE_H_
