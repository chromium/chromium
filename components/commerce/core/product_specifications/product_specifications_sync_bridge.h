// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SYNC_BRIDGE_H_
#define COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SYNC_BRIDGE_H_

#include "base/memory/weak_ptr.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/entity_data.h"

namespace syncer {
class MetadataChangeList;
class ModelError;
}  // namespace syncer

namespace commerce {

class ProductSpecificationsSyncBridgeTest;

// Integration point between sync and ProductSpecificationService.
class ProductSpecificationsSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  ProductSpecificationsSyncBridge(
      syncer::OnceModelTypeStoreFactory create_store_callback,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  ~ProductSpecificationsSyncBridge() override;

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;

 private:
  friend class commerce::ProductSpecificationsSyncBridgeTest;
  using CompareSpecificsEntries =
      std::map<std::string, sync_pb::CompareSpecifics>;
  CompareSpecificsEntries entries_;
  std::unique_ptr<syncer::ModelTypeStore> store_;

  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);
  void OnReadAllData(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> record_list);
  void OnReadAllMetadata(
      std::unique_ptr<syncer::ModelTypeStore::RecordList> record_list,
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  base::WeakPtrFactory<ProductSpecificationsSyncBridge> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SYNC_BRIDGE_H_
