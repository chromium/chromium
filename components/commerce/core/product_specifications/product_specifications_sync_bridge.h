// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SYNC_BRIDGE_H_
#define COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SYNC_BRIDGE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
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
class ProductSpecificationsSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  class Delegate {
   public:
    // New specifics were added - either locally or from another browser and
    // then synced.
    virtual void OnSpecificsAdded(
        const std::vector<sync_pb::ProductComparisonSpecifics> specifics) {}

    // Specifics were updated
    virtual void OnSpecificsUpdated(
        const std::vector<std::pair<sync_pb::ProductComparisonSpecifics,
                                    sync_pb::ProductComparisonSpecifics>>
            specifics) {}

    // Specifics were removed
    virtual void OnSpecificsRemoved(
        const std::vector<sync_pb::ProductComparisonSpecifics> specifics) {}

    // Specifics for the multi specifics representation (where a
    // ProductSpecificationsSet is stored across multiple specifics)
    // changed.
    virtual void OnMultiSpecificsChanged(
        const std::vector<sync_pb::ProductComparisonSpecifics>
            changed_specifics,
        const std::map<std::string, sync_pb::ProductComparisonSpecifics>
            prev_entries) {}
  };

  ProductSpecificationsSyncBridge(
      syncer::OnceDataTypeStoreFactory create_store_callback,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      base::OnceCallback<void(void)> init_callback,
      Delegate* delegate);
  ~ProductSpecificationsSyncBridge() override;

  // syncer::DataTypeSyncBridge:
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

  // Return true if sync is enabled (in chrome://settings/syncSetup/advanced)
  // sync is enabled and the Product Specifications toggle is enabled).
  bool IsSyncEnabled();

 private:
  friend class commerce::ProductSpecificationsService;
  friend class commerce::ProductSpecificationsServiceTest;
  friend class commerce::ProductSpecificationsSyncBridgeMultiSpecsTest;
  friend class commerce::ProductSpecificationsSyncBridgeTest;

  std::map<std::string, sync_pb::ProductComparisonSpecifics>& entries() {
    return entries_;
  }

  void AddCompareSpecificsForTesting(
      const sync_pb::ProductComparisonSpecifics& product_comparison_specifics) {
    entries_.emplace(product_comparison_specifics.uuid(),
                     product_comparison_specifics);
  }

  virtual void AddSpecifics(
      const std::vector<sync_pb::ProductComparisonSpecifics> specifics);

  // Update the specifics to the value provided in both sync and the store. This
  // method assumes the specifics with the |uuid| already exists.
  void UpdateSpecifics(
      const sync_pb::ProductComparisonSpecifics& new_specifics);

  void DeleteSpecifics(
      const std::vector<sync_pb::ProductComparisonSpecifics> specifics);

  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);
  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> record_list,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void Commit(std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch);
  bool SyncMetadataCacheContainsSupportedFields(
      const syncer::EntityMetadataMap& metadata_map) const;

  void OnCommit(const std::optional<syncer::ModelError>& error);

  const sync_pb::ProductComparisonSpecifics&
  GetPossiblyTrimmedPasswordSpecificsData(const std::string& storage_key);

  std::unique_ptr<syncer::EntityData> CreateEntityData(
      const sync_pb::ProductComparisonSpecifics& specifics);

  const sync_pb::ProductComparisonSpecifics TrimSpecificsForCaching(
      const sync_pb::ProductComparisonSpecifics& comparison_specifics) const;

  void ApplyIncrementalSyncChangesForTesting(
      const std::vector<std::pair<sync_pb::ProductComparisonSpecifics,
                                  syncer::EntityChange::ChangeType>>&
          specifics_to_change);

  std::map<std::string, sync_pb::ProductComparisonSpecifics> entries_;

  std::unique_ptr<syncer::DataTypeStore> store_;

  base::OnceCallback<void(void)> init_callback_;

  raw_ptr<Delegate> const delegate_;

  base::WeakPtrFactory<ProductSpecificationsSyncBridge> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SYNC_BRIDGE_H_
