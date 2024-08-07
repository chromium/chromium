// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_FAKE_DESK_SYNC_BRIDGE_H_
#define COMPONENTS_DESKS_STORAGE_CORE_FAKE_DESK_SYNC_BRIDGE_H_

#include "base/memory/raw_ptr.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {
class DeskTemplate;
enum class DeskTemplateType;
}  // namespace ash

namespace desks_storage {

// A fake desk sync bridge used for testing.
class FakeDeskSyncBridge : public DeskModel {
 public:
  FakeDeskSyncBridge();
  FakeDeskSyncBridge(const FakeDeskSyncBridge&) = delete;
  FakeDeskSyncBridge& operator=(const FakeDeskSyncBridge&) = delete;
  ~FakeDeskSyncBridge() override;

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

  void SetCacheGuid(std::string cache_guid) { cache_guid_ = cache_guid; }
  std::string GetCacheGuid() override;

 private:
  using DeskEntries =
      base::flat_map<base::Uuid, std::unique_ptr<ash::DeskTemplate>>;

  // Notify all observers of any `new_entries` when they are added/updated via
  // sync.
  void NotifyRemoteDeskTemplateAddedOrUpdated(
      const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
          new_entries);

  const ash::DeskTemplate* GetUserEntryByUUID(const base::Uuid& uuid) const;

  // `desk_template_entries_` is keyed by UUIDs.
  DeskEntries desk_template_entries_;

  std::string cache_guid_;

  base::WeakPtrFactory<FakeDeskSyncBridge> weak_ptr_factory_{this};
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_FAKE_DESK_SYNC_BRIDGE_H_
