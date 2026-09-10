// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace os_crypt_async {
class Encryptor;
}  // namespace os_crypt_async

namespace syncer {
class DataTypeControllerDelegate;
class DataTypeLocalChangeProcessor;
class MetadataChangeList;
class MutableDataBatch;
}  // namespace syncer

namespace autofill {

// Sync bridge for the `AUTOFILL_ENTITY_SUPPRESSION` data type.
// Handles encrypted persistence at rest and Sync integration.
// Owned by `EntitySuppressionManager`, one instance per profile.
// TODO(crbug.com/501036619): Move under webdata/personal_context.
class EntitySuppressionSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when suppressions have been loaded or changed.
    virtual void OnSuppressionsChanged() {}
  };

  EntitySuppressionSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory,
      scoped_refptr<const os_crypt_async::Encryptor> encryptor);
  EntitySuppressionSyncBridge(const EntitySuppressionSyncBridge&) = delete;
  EntitySuppressionSyncBridge& operator=(const EntitySuppressionSyncBridge&) =
      delete;
  ~EntitySuppressionSyncBridge() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Adds a suppression `entry`, generates a GUID, encrypts and persists it,
  // and notifies Sync. Returns `false` if Sync is not tracking metadata or the
  // `entry` was already suppressed. Returns `true` if suppression status was
  // modified.
  bool Suppress(const EntitySuppressionEntry& entry);

  // Deletes all suppression records matching `entry`. Returns `false` if Sync
  // is not tracking metadata or the `entry` was not suppressed. Returns `true`
  // if suppression status was modified.
  bool Unsuppress(const EntitySuppressionEntry& entry);

  // Returns all currently active suppression entries.
  base::flat_set<EntitySuppressionEntry> GetSuppressions() const;

  bool IsLoaded() const;

  // Returns a weak pointer to the sync controller delegate.
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate();

  // syncer::DataTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

 private:
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);
  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void ReportErrorIfSet(const std::optional<syncer::ModelError>& error);

  // Queries all suppression data and returns it as a
  // `syncer::MutableDataBatch`.
  std::unique_ptr<syncer::MutableDataBatch> GetAllData();

  void NotifySuppressionsChanged();

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<const os_crypt_async::Encryptor> encryptor_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  bool is_loaded_ = false;

  base::ObserverList<Observer> observers_;

  // In-memory mapping from `EntitySuppressionEntry` to storage key.
  base::flat_map<EntitySuppressionEntry, std::string> guids_by_entry_;

  base::WeakPtrFactory<EntitySuppressionSyncBridge> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_SYNC_BRIDGE_H_
