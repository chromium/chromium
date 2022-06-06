// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_MODEL_WRAPPER_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_MODEL_WRAPPER_H_

#include <map>
#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "base/guid.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_sync_bridge.h"

namespace ash {
class DeskTemplate;
}

namespace desks_storage {
// The DeskModelWrapper handles storage operations for chrome sync
// desk templates and local storage save and recall desks backends.
//
// TODO(crbug.com/1323946): Add unittests for this class.
class DeskModelWrapper : public DeskModel {
 public:
  DeskModelWrapper(desks_storage::DeskModel* save_and_recall_desks_model);

  DeskModelWrapper(const DeskModelWrapper&) = delete;
  DeskModelWrapper& operator=(const DeskModelWrapper&) = delete;
  ~DeskModelWrapper() override;

  // DeskModel:
  void GetAllEntries(GetAllEntriesCallback callback) override;
  void GetEntryByUUID(const std::string& uuid,
                      GetEntryByUuidCallback callback) override;
  void AddOrUpdateEntry(std::unique_ptr<ash::DeskTemplate> new_entry,
                        AddOrUpdateEntryCallback callback) override;
  void DeleteEntry(const std::string& uuid,
                   DeleteEntryCallback callback) override;
  void DeleteAllEntries(DeleteEntryCallback callback) override;
  std::size_t GetEntryCount() const override;
  std::size_t GetMaxEntryCount() const override;
  std::size_t GetSaveAndRecallDeskEntryCount() const override;
  std::size_t GetDeskTemplateEntryCount() const override;
  std::size_t GetMaxSaveAndRecallDeskEntryCount() const override;
  std::size_t GetMaxDeskTemplateEntryCount() const override;
  std::vector<base::GUID> GetAllEntryUuids() const override;
  bool IsReady() const override;
  bool IsSyncing() const override;
  ash::DeskTemplate* FindOtherEntryWithName(
      const std::u16string& name,
      ash::DeskTemplateType type,
      const base::GUID& uuid) const override;

  // Setter method to set `desk_template_model_` to the correct `bridge`.
  void SetDeskSyncBridge(desks_storage::DeskSyncBridge* bridge) {
    desk_template_model_ = bridge;
  }

 private:
  desks_storage::DeskSyncBridge* GetDeskTemplateModel() const;

  // Wrapper for GetAllEntriesCallback to consolidate entries from both storage
  // models into one.
  void OnGetAllEntries(
      const std::vector<const ash::DeskTemplate*>& template_entries,
      DeskModel::GetAllEntriesCallback callback,
      desks_storage::DeskModel::GetAllEntriesStatus status,
      const std::vector<const ash::DeskTemplate*>& entries);

  // Wrapper for DeleteEntryCallback to consolidate deleting all entries from
  // both storage models.
  void OnDeleteAllEntries(DeskModel::DeleteEntryCallback callback,
                          desks_storage::DeskModel::DeleteEntryStatus status);

  desks_storage::DeskModel* save_and_recall_desks_model_;

  desks_storage::DeskSyncBridge* desk_template_model_;

  base::WeakPtrFactory<DeskModelWrapper> weak_ptr_factory_{this};
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_
