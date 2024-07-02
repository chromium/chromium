// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SYNC_BRIDGE_H_
#define COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SYNC_BRIDGE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"
#include "url/gurl.h"

namespace syncer {
class MetadataChangeList;
class ModelError;
}  // namespace syncer

namespace commerce {

class ProductSpecificationsService;
class ProductSpecificationsServiceTest;
class ProductSpecificationsSyncBridgeMultiSpecsTest;
class ProductSpecificationsSyncBridgeTest;

// Integration point between sync and ProductSpecificationService.
class ProductSpecificationsSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  class Delegate {
   public:
    // New specifics were added - either locally or from another browser and
    // then synced.
    virtual void OnSpecificsAdded(
        const std::vector<sync_pb::ProductComparisonSpecifics> specifics) {}

    // TODO(crbug.com/350345285) Implement OnSpecificsRemoved
    // TODO(crbug.com/350349095) Implement OnSpecificsUpdated
  };

  ProductSpecificationsSyncBridge(
      syncer::OnceModelTypeStoreFactory create_store_callback,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      base::OnceCallback<void(void)> init_callback,
      Delegate* delegate);
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
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

 private:
  friend class commerce::ProductSpecificationsService;
  friend class commerce::ProductSpecificationsServiceTest;
  friend class commerce::ProductSpecificationsSyncBridgeMultiSpecsTest;
  friend class commerce::ProductSpecificationsSyncBridgeTest;

  using CompareSpecificsEntries =
      std::map<std::string, sync_pb::ProductComparisonSpecifics>;

  const CompareSpecificsEntries& entries() { return entries_; }

  void AddCompareSpecificsForTesting(
      const sync_pb::ProductComparisonSpecifics& product_comparison_specifics) {
    entries_.emplace(product_comparison_specifics.uuid(),
                     product_comparison_specifics);
  }

  virtual void AddSpecifics(
      const std::vector<sync_pb::ProductComparisonSpecifics> specifics);

  // Update the specifics for the provided ProductSpecificationsSet based on its
  // UUID. If no specifics for a UUID are found, this method is a noop and
  // nullopt is returned.
  sync_pb::ProductComparisonSpecifics UpdateProductSpecificationsSet(
      const ProductSpecificationsSet& set);

  void DeleteProductSpecificationsSet(const std::string& uuid);

  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);
  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> record_list,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void Commit(std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch);
  bool SyncMetadataCacheContainsSupportedFields(
      const syncer::EntityMetadataMap& metadata_map) const;

  void OnCommit(const std::optional<syncer::ModelError>& error);

  void AddObserver(commerce::ProductSpecificationsSet::Observer* observer);
  void RemoveObserver(commerce::ProductSpecificationsSet::Observer* observer);

  void OnSpecificsUpdated(const sync_pb::ProductComparisonSpecifics& before,
                          const sync_pb::ProductComparisonSpecifics& after);
  void OnSpecificsRemoved(const ProductSpecificationsSet& removed_set);

  const sync_pb::ProductComparisonSpecifics&
  GetPossiblyTrimmedPasswordSpecificsData(const std::string& storage_key);

  std::unique_ptr<syncer::EntityData> CreateEntityData(
      const sync_pb::ProductComparisonSpecifics& specifics);

  CompareSpecificsEntries entries_;

  std::unique_ptr<syncer::ModelTypeStore> store_;

  base::ObserverList<commerce::ProductSpecificationsSet::Observer> observers_;

  base::OnceCallback<void(void)> init_callback_;

  raw_ptr<Delegate> const delegate_;

  base::WeakPtrFactory<ProductSpecificationsSyncBridge> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SYNC_BRIDGE_H_
