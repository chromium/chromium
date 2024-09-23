// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_MODEL_WRAPPER_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_MODEL_WRAPPER_H_

#include <stddef.h>

#include <map>
#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
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
// TODO(crbug.com/40224854): Add unittests for this class.
class DeskModelWrapper : public DeskModel {
 public:
  DeskModelWrapper(desks_storage::DeskModel* save_and_recall_desks_model);

  DeskModelWrapper(const DeskModelWrapper&) = delete;
  DeskModelWrapper& operator=(const DeskModelWrapper&) = delete;
  ~DeskModelWrapper() override;

  // DeskModel:
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
  bool IsSyncing() const override;
  ash::DeskTemplate* FindOtherEntryWithName(
      const std::u16string& name,
      ash::DeskTemplateType type,
      const base::Uuid& uuid) const override;
  std::string GetCacheGuid() override;

  // Setter method to set `desk_template_model_` to the correct `bridge`.
  void SetDeskSyncBridge(desks_storage::DeskSyncBridge* bridge) {
    desk_template_model_ = bridge;
  }

 private:
  desks_storage::DeskSyncBridge* GetDeskTemplateModel() const;

  // Wrapper for DeleteEntryCallback to consolidate deleting all entries from
  // both storage models.
  void OnDeleteAllEntries(DeskModel::DeleteEntryCallback callback,
                          desks_storage::DeskModel::DeleteEntryStatus status);

  raw_ptr<desks_storage::DeskModel> save_and_recall_desks_model_;

  raw_ptr<desks_storage::DeskSyncBridge> desk_template_model_;

  base::WeakPtrFactory<DeskModelWrapper> weak_ptr_factory_{this};
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_MODEL_WRAPPER_H_
