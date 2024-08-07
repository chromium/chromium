// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_SYNC_BRIDGE_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_SYNC_BRIDGE_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/account_id/account_id.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
}  // namespace syncer

namespace ash {
class DeskTemplate;
enum class DeskTemplateType;
}  // namespace ash

namespace desks_storage {

// A Sync-backed persistence layer for Workspace Desk.
class DeskSyncBridge : public syncer::DataTypeSyncBridge, public DeskModel {
 public:
  DeskSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory create_store_callback,
      const AccountId& account_id);
  DeskSyncBridge(const DeskSyncBridge&) = delete;
  DeskSyncBridge& operator=(const DeskSyncBridge&) = delete;
  ~DeskSyncBridge() override;

  // syncer::DataTypeSyncBridge overrides.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // DeskModel overrides.
  DeskModel::GetAllEntriesResult GetAllEntries() override;
  DeskModel::GetEntryByUuidResult GetEntryByUUID(
      const base::Uuid& uuid) override;

  void AddOrUpdateEntry(std::unique_ptr<ash::DeskTemplate> new_entry,
                        AddOrUpdateEntryCallback callback) override;
  void DeleteEntry(const base::Uuid& uuid,
                   DeleteEntryCallback callback) override;
  void DeleteAllEntries(DeleteEntryCallback callback) override;
  size_t GetEntryCount() const override;
  size_t GetSaveAndRecallDeskEntryCount() const override;
  size_t GetDeskTemplateEntryCount() const override;
  size_t GetMaxSaveAndRecallDeskEntryCount() const override;
  size_t GetMaxDeskTemplateEntryCount() const override;
  std::set<base::Uuid> GetAllEntryUuids() const override;
  bool IsReady() const override;
  // Whether this sync bridge is syncing local data to sync. This sync bridge
  // still allows user to save desk templates locally when users disable syncing
  // for Workspace Desk data type.
  bool IsSyncing() const override;

  ash::DeskTemplate* FindOtherEntryWithName(
      const std::u16string& name,
      ash::DeskTemplateType type,
      const base::Uuid& uuid) const override;

  std::string GetCacheGuid() override;

  // Other helper methods.
  bool HasUuid(const base::Uuid& uuid) const;
  const ash::DeskTemplate* GetUserEntryByUUID(const base::Uuid& uuid) const;

 private:
  friend class DeskModelWrapper;

  using DeskEntries =
      base::flat_map<base::Uuid, std::unique_ptr<ash::DeskTemplate>>;

  DeskModel::DeleteEntryStatus DeleteAllEntriesSync();

  // Notify all observers that the model is loaded;
  void NotifyDeskModelLoaded();

  // Notify all observers of any `new_entries` when they are added/updated
  // via sync.
  void NotifyRemoteDeskTemplateAddedOrUpdated(
      const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
          new_entries);

  // Notify all observers when the entries with `uuids` have been removed via
  // sync or disabling sync locally.
  void NotifyRemoteDeskTemplateDeleted(const std::vector<base::Uuid>& uuids);

  // Methods used as callbacks given to DataTypeStore.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);
  void OnReadAllData(std::unique_ptr<DeskEntries> initial_entries,
                     const std::optional<syncer::ModelError>& error);
  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnCommit(const std::optional<syncer::ModelError>& error);

  // Persists changes in sync store.
  void Commit(std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch);

  // Uploads data that only exists locally to Sync during MergeFullSyncData().
  void UploadLocalOnlyData(syncer::MetadataChangeList* metadata_change_list,
                           const syncer::EntityChangeList& entity_data);

  // Returns true if `templates_` contains a desk template with `name`.
  bool HasUserTemplateWithName(const std::u16string& name);

  // `desk_template_entries_` is keyed by UUIDs.
  DeskEntries desk_template_entries_;

  // Whether local data and metadata have finished loading and this sync bridge
  // is ready to be accessed.
  bool is_ready_;

  // In charge of actually persisting changes to disk, or loading previous
  // data.
  std::unique_ptr<syncer::DataTypeStore> store_;

  // Account ID of the user this class will sync data for.
  const AccountId account_id_;

  base::WeakPtrFactory<DeskSyncBridge> weak_ptr_factory_{this};
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_SYNC_BRIDGE_H_
