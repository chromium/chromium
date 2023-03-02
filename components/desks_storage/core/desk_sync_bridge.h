// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_SYNC_BRIDGE_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_SYNC_BRIDGE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace ash {
class DeskTemplate;
enum class DeskTemplateType;
}  // namespace ash

namespace desks_storage {

// A Sync-backed persistence layer for Workspace Desk.
class DeskSyncBridge : public syncer::ModelTypeSyncBridge, public DeskModel {
 public:
  DeskSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory create_store_callback,
      const AccountId& account_id);
  DeskSyncBridge(const DeskSyncBridge&) = delete;
  DeskSyncBridge& operator=(const DeskSyncBridge&) = delete;
  ~DeskSyncBridge() override;

  // syncer::ModelTypeSyncBridge overrides.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  absl::optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // DeskModel overrides.
  DeskModel::GetAllEntriesResult GetAllEntries() override;
  DeskModel::GetEntryByUuidResult GetEntryByUUID(
      const base::GUID& uuid) override;

  void AddOrUpdateEntry(std::unique_ptr<ash::DeskTemplate> new_entry,
                        AddOrUpdateEntryCallback callback) override;
  void DeleteEntry(const base::GUID& uuid,
                   DeleteEntryCallback callback) override;
  void DeleteAllEntries(DeleteEntryCallback callback) override;
  size_t GetEntryCount() const override;
  size_t GetSaveAndRecallDeskEntryCount() const override;
  size_t GetDeskTemplateEntryCount() const override;
  size_t GetMaxSaveAndRecallDeskEntryCount() const override;
  size_t GetMaxDeskTemplateEntryCount() const override;
  std::vector<base::GUID> GetAllEntryUuids() const override;
  bool IsReady() const override;
  // Whether this sync bridge is syncing local data to sync. This sync bridge
  // still allows user to save desk templates locally when users disable syncing
  // for Workspace Desk model type.
  bool IsSyncing() const override;

  ash::DeskTemplate* FindOtherEntryWithName(
      const std::u16string& name,
      ash::DeskTemplateType type,
      const base::GUID& uuid) const override;

  // Other helper methods.
  bool HasUuid(const base::GUID& uuid) const;

  const ash::DeskTemplate* GetUserEntryByUUID(const base::GUID& uuid) const;

 private:
  friend class DeskModelWrapper;

  using DeskEntries =
      base::flat_map<base::GUID, std::unique_ptr<ash::DeskTemplate>>;

  DeskModel::DeleteEntryStatus DeleteAllEntriesSync();

  // Notify all observers that the model is loaded;
  void NotifyDeskModelLoaded();

  // Notify all observers of any `new_entries` when they are added/updated via
  // sync.
  void NotifyRemoteDeskTemplateAddedOrUpdated(
      const std::vector<const ash::DeskTemplate*>& new_entries);

  // Notify all observers when the entries with `uuids` have been removed via
  // sync or disabling sync locally.
  void NotifyRemoteDeskTemplateDeleted(const std::vector<base::GUID>& uuids);

  // Methods used as callbacks given to DataTypeStore.
  void OnStoreCreated(const absl::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);
  void OnReadAllData(std::unique_ptr<DeskEntries> initial_entries,
                     const absl::optional<syncer::ModelError>& error);
  void OnReadAllMetadata(const absl::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnCommit(const absl::optional<syncer::ModelError>& error);

  // Persists changes in sync store.
  void Commit(std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch);

  // Uploads data that only exists locally to Sync during MergeSyncData().
  void UploadLocalOnlyData(syncer::MetadataChangeList* metadata_change_list,
                           const syncer::EntityChangeList& entity_data);

  // Returns true if `templates_` contains a desk template with `name`.
  bool HasUserTemplateWithName(const std::u16string& name);

  // `desk_template_entries_` is keyed by UUIDs.
  DeskEntries desk_template_entries_;

  // Whether local data and metadata have finished loading and this sync bridge
  // is ready to be accessed.
  bool is_ready_;

  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  // Account ID of the user this class will sync data for.
  const AccountId account_id_;

  base::WeakPtrFactory<DeskSyncBridge> weak_ptr_factory_{this};
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_SYNC_BRIDGE_H_
