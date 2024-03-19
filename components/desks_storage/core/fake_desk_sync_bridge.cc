// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/fake_desk_sync_bridge.h"

#include "ash/public/cpp/desk_template.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "build/chromeos_buildflags.h"
#include "components/app_constants/constants.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_template_util.h"
#include "ui/base/ui_base_types.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/lacros_startup_state.h"  // nogncheck
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace desks_storage {

FakeDeskSyncBridge::FakeDeskSyncBridge() : cache_guid_("test_guid") {}

FakeDeskSyncBridge::~FakeDeskSyncBridge() = default;

DeskModel::GetAllEntriesResult FakeDeskSyncBridge::GetAllEntries() {
  if (!IsReady()) {
    return GetAllEntriesResult(
        GetAllEntriesStatus::kFailure,
        std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>());
  }

  std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>> entries;

  for (const auto& it : policy_entries_) {
    entries.push_back(it.get());
  }

  for (const auto& it : desk_template_entries_) {
    DCHECK_EQ(it.first, it.second->uuid());
    entries.push_back(it.second.get());
  }

  return GetAllEntriesResult(GetAllEntriesStatus::kOk, std::move(entries));
}

DeskModel::GetEntryByUuidResult FakeDeskSyncBridge::GetEntryByUUID(
    const base::Uuid& uuid) {
  if (!IsReady()) {
    return GetEntryByUuidResult(GetEntryByUuidStatus::kFailure, nullptr);
  }

  if (!uuid.is_valid()) {
    return GetEntryByUuidResult(GetEntryByUuidStatus::kInvalidUuid, nullptr);
  }

  auto it = desk_template_entries_.find(uuid);
  if (it == desk_template_entries_.end()) {
    std::unique_ptr<ash::DeskTemplate> policy_entry =
        GetAdminDeskTemplateByUUID(uuid);

    if (policy_entry) {
      return GetEntryByUuidResult(GetEntryByUuidStatus::kOk,
                                  std::move(policy_entry));
    } else {
      return GetEntryByUuidResult(GetEntryByUuidStatus::kNotFound, nullptr);
    }
  } else {
    return GetEntryByUuidResult(GetEntryByUuidStatus::kOk,
                                it->second.get()->Clone());
  }
}

void FakeDeskSyncBridge::AddOrUpdateEntry(
    std::unique_ptr<ash::DeskTemplate> new_entry,
    AddOrUpdateEntryCallback callback) {
  if (!IsReady()) {
    // This sync bridge has not finished initializing. Do not save the new entry
    // yet.
    std::move(callback).Run(AddOrUpdateEntryStatus::kFailure,
                            std::move(new_entry));
    return;
  }

  if (!new_entry) {
    std::move(callback).Run(AddOrUpdateEntryStatus::kInvalidArgument,
                            std::move(new_entry));
    return;
  }

  base::Uuid uuid = new_entry->uuid();
  if (!uuid.is_valid()) {
    std::move(callback).Run(AddOrUpdateEntryStatus::kInvalidArgument,
                            std::move(new_entry));
    return;
  }
  std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>
      added_or_updated;
  // When a user creates a desk template locally, the desk template has `kUser`
  // as its source. Only user desk templates should be saved to Sync.
  DCHECK_EQ(ash::DeskTemplateSource::kUser, new_entry->source());
  auto entry = new_entry->Clone();

  entry->set_template_name(
      base::CollapseWhitespace(new_entry->template_name(), true));

  desk_template_entries_[uuid] = std::move(entry);
  added_or_updated.push_back(GetUserEntryByUUID(uuid));
  NotifyRemoteDeskTemplateAddedOrUpdated(added_or_updated);
  std::move(callback).Run(AddOrUpdateEntryStatus::kOk, std::move(new_entry));
}

void FakeDeskSyncBridge::DeleteEntry(const base::Uuid& uuid,
                                     DeleteEntryCallback callback) {
  if (!IsReady()) {
    // This sync bridge has not finished initializing.
    // Cannot delete anything.
    std::move(callback).Run(DeleteEntryStatus::kFailure);
    return;
  }

  if (GetUserEntryByUUID(uuid) == nullptr) {
    // Consider the deletion successful if the entry does not exist.
    std::move(callback).Run(DeleteEntryStatus::kOk);
    return;
  }

  desk_template_entries_.erase(uuid);
  std::move(callback).Run(DeleteEntryStatus::kOk);
}

void FakeDeskSyncBridge::DeleteAllEntries(DeleteEntryCallback callback) {
  desk_template_entries_.clear();
  std::move(callback).Run(DeleteEntryStatus::kOk);
}

size_t FakeDeskSyncBridge::GetEntryCount() const {
  return GetSaveAndRecallDeskEntryCount() + GetDeskTemplateEntryCount();
}

// Return 0 for now since chrome sync does not support save and recall desks.
size_t FakeDeskSyncBridge::GetSaveAndRecallDeskEntryCount() const {
  return 0u;
}

size_t FakeDeskSyncBridge::GetDeskTemplateEntryCount() const {
  size_t template_count = std::count_if(
      desk_template_entries_.begin(), desk_template_entries_.end(),
      [](const std::pair<base::Uuid, std::unique_ptr<ash::DeskTemplate>>&
             entry) {
        return entry.second->type() == ash::DeskTemplateType::kTemplate;
      });
  return template_count + policy_entries_.size();
}

// Chrome sync does not support save and recall desks yet. Return 0 for max
// count.
size_t FakeDeskSyncBridge::GetMaxSaveAndRecallDeskEntryCount() const {
  return 0u;
}

size_t FakeDeskSyncBridge::GetMaxDeskTemplateEntryCount() const {
  return 6u + policy_entries_.size();
}

std::set<base::Uuid> FakeDeskSyncBridge::GetAllEntryUuids() const {
  std::set<base::Uuid> keys;

  for (const auto& it : policy_entries_) {
    keys.emplace(it.get()->uuid());
  }

  for (const auto& it : desk_template_entries_) {
    DCHECK_EQ(it.first, it.second->uuid());
    keys.emplace(it.first);
  }
  return keys;
}

bool FakeDeskSyncBridge::IsReady() const {
  return true;
}

bool FakeDeskSyncBridge::IsSyncing() const {
  return false;
}

ash::DeskTemplate* FakeDeskSyncBridge::FindOtherEntryWithName(
    const std::u16string& name,
    ash::DeskTemplateType type,
    const base::Uuid& uuid) const {
  return desk_template_util::FindOtherEntryWithName(name, uuid,
                                                    desk_template_entries_);
}

const ash::DeskTemplate* FakeDeskSyncBridge::GetUserEntryByUUID(
    const base::Uuid& uuid) const {
  auto it = desk_template_entries_.find(uuid);
  if (it == desk_template_entries_.end()) {
    return nullptr;
  }
  return it->second.get();
}

void FakeDeskSyncBridge::NotifyRemoteDeskTemplateAddedOrUpdated(
    const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
        new_entries) {
  if (new_entries.empty()) {
    return;
  }

  for (DeskModelObserver& observer : observers_) {
    observer.EntriesAddedOrUpdatedRemotely(new_entries);
  }
}

std::string FakeDeskSyncBridge::GetCacheGuid() {
  return cache_guid_;
}

}  // namespace desks_storage
