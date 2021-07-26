// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_sync_bridge.h"

#include <algorithm>

#include "ash/public/cpp/desk_template.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace desks_storage {

using ash::DeskTemplate;

namespace {

using syncer::ModelTypeStore;

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_t) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(proto_t));
}

// Converts a time object to the format used in sync protobufs
// (Microseconds since the Windows epoch).
int64_t TimeToProtoTime(const base::Time& t) {
  return t.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

// Allocate a EntityData and copies |specifics| into it.
std::unique_ptr<syncer::EntityData> CopyToEntityData(
    const sync_pb::WorkspaceDeskSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_workspace_desk() = specifics;
  entity_data->name = specifics.uuid();
  entity_data->creation_time = ProtoTimeToTime(specifics.created_time_usec());
  return entity_data;
}

// Parses the content of |record_list| into |*desk_templates|.
absl::optional<syncer::ModelError> ParseDeskTemplatesOnBackendSequence(
    std::map<base::GUID, std::unique_ptr<DeskTemplate>>* desk_templates,
    std::unique_ptr<ModelTypeStore::RecordList> record_list) {
  DCHECK(desk_templates);
  DCHECK(desk_templates->empty());
  DCHECK(record_list);

  for (const syncer::ModelTypeStore::Record& r : *record_list) {
    auto specifics = std::make_unique<sync_pb::WorkspaceDeskSpecifics>();
    if (specifics->ParseFromString(r.value)) {
      const base::GUID uuid =
          base::GUID::ParseCaseInsensitive(specifics->uuid());
      if (!uuid.is_valid()) {
        return syncer::ModelError(
            FROM_HERE,
            base::StringPrintf("Failed to parse WorkspaceDeskSpecifics uuid %s",
                               specifics->uuid().c_str()));
      }

      std::unique_ptr<ash::DeskTemplate> entry =
          DeskSyncBridge::FromProto(*specifics);

      if (!entry)
        continue;

      (*desk_templates)[uuid] = std::move(entry);
    } else {
      return syncer::ModelError(
          FROM_HERE, "Failed to deserialize WorkspaceDeskSpecifics.");
    }
  }

  return absl::nullopt;
}

}  // namespace

DeskSyncBridge::DeskSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory create_store_callback)
    : ModelTypeSyncBridge(std::move(change_processor)), is_ready_(false) {
  std::move(create_store_callback)
      .Run(syncer::WORKSPACE_DESK,
           base::BindOnce(&DeskSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

DeskSyncBridge::~DeskSyncBridge() = default;

// Static
sync_pb::WorkspaceDeskSpecifics DeskSyncBridge::AsSyncProto(
    const DeskTemplate* desk_template) {
  sync_pb::WorkspaceDeskSpecifics pb_entry;

  pb_entry.set_uuid(desk_template->uuid().AsLowercaseString());
  pb_entry.set_name(base::UTF16ToUTF8(desk_template->template_name()));
  pb_entry.set_created_time_usec(
      TimeToProtoTime(desk_template->created_time()));

  // TODO(yzd) copy other data fields
  return pb_entry;
}

// Static
std::unique_ptr<DeskTemplate> DeskSyncBridge::FromProto(
    const sync_pb::WorkspaceDeskSpecifics& pb_entry) {
  const std::string uuid(pb_entry.uuid());
  if (uuid.empty() || !base::GUID::ParseCaseInsensitive(uuid).is_valid())
    return nullptr;

  const base::Time created_time = ProtoTimeToTime(pb_entry.created_time_usec());

  // Protobuf parsing enforces utf8 encoding for all strings.
  // TODO(yzd) copy other data fields
  return std::make_unique<DeskTemplate>(uuid, pb_entry.name(), created_time);
}

std::unique_ptr<syncer::MetadataChangeList>
DeskSyncBridge::CreateMetadataChangeList() {
  return ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

absl::optional<syncer::ModelError> DeskSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // MergeSyncData will be called when Desk Template model type is enabled to
  // start syncing. There could be local desk templates that user has created
  // before enabling sync or during the time when Desk Template sync is
  // disabled. We should merge local and server data. We will send all
  // local-only templates to server and save server templates to local.

  UploadLocalOnlyData(metadata_change_list.get(), entity_data);

  // Apply server changes locally. Currently, if a template exists on both
  // local and server side, the server version will win.
  // TODO(yzd) We will add a template update timestamp and update this logic to
  // be: for templates that exist on both local and server side, we will keep
  // the one with later update timestamp.
  return ApplySyncChanges(std::move(metadata_change_list),
                          std::move(entity_data));
}

absl::optional<syncer::ModelError> DeskSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::vector<const DeskTemplate*> added_or_updated;
  std::vector<std::string> removed;
  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const base::GUID uuid =
        base::GUID::ParseCaseInsensitive(change->storage_key());
    if (!uuid.is_valid()) {
      // Skip invalid storage keys.
      continue;
    }

    switch (change->type()) {
      case syncer::EntityChange::ACTION_DELETE: {
        if (entries_.find(uuid) != entries_.end()) {
          entries_.erase(uuid);
          batch->DeleteData(uuid.AsLowercaseString());
          removed.push_back(uuid.AsLowercaseString());
        }
        break;
      }
      case syncer::EntityChange::ACTION_UPDATE:
      case syncer::EntityChange::ACTION_ADD: {
        const sync_pb::WorkspaceDeskSpecifics& specifics =
            change->data().specifics.workspace_desk();

        std::unique_ptr<DeskTemplate> remote_entry =
            DeskSyncBridge::FromProto(specifics);
        if (!remote_entry) {
          // Skip invalid entries.
          continue;
        }

        DCHECK_EQ(uuid, remote_entry->uuid());
        std::string serialized_remote_entry =
            DeskSyncBridge::AsSyncProto(remote_entry.get()).SerializeAsString();

        // Add/update the remote_entry to the model.
        entries_[uuid] = std::move(remote_entry);
        added_or_updated.push_back(GetEntryByUUID(uuid));

        // Write to the store.
        batch->WriteData(uuid.AsLowercaseString(), serialized_remote_entry);
        break;
      }
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  Commit(std::move(batch));

  NotifyRemoteDeskTemplateAddedOrUpdated(added_or_updated);
  NotifyRemoteDeskTemplateDeleted(removed);

  return absl::nullopt;
}

void DeskSyncBridge::GetData(StorageKeyList storage_keys,
                             DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& uuid : storage_keys) {
    const DeskTemplate* entry =
        GetEntryByUUID(base::GUID::ParseCaseInsensitive(uuid));
    if (!entry) {
      continue;
    }

    batch->Put(uuid, CopyToEntityData(DeskSyncBridge::AsSyncProto(entry)));
  }
  std::move(callback).Run(std::move(batch));
}

void DeskSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& it : entries_) {
    batch->Put(it.first.AsLowercaseString(),
               CopyToEntityData(DeskSyncBridge::AsSyncProto(it.second.get())));
  }
  std::move(callback).Run(std::move(batch));
}

std::string DeskSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string DeskSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.workspace_desk().uuid();
}

void DeskSyncBridge::GetAllEntries(GetAllEntriesCallback callback) {
  std::vector<DeskTemplate*> entries;

  if (!IsReady()) {
    std::move(callback).Run(GetAllEntriesStatus::kFailure, std::move(entries));
    return;
  }

  for (const auto& it : entries_) {
    DCHECK_EQ(it.first, it.second->uuid());
    entries.push_back(it.second.get());
  }

  std::move(callback).Run(GetAllEntriesStatus::kOk, std::move(entries));
}

void DeskSyncBridge::GetEntryByUUID(const std::string& uuid_str,
                                    GetEntryByUuidCallback callback) {
  if (!IsReady()) {
    std::move(callback).Run(GetEntryByUuidStatus::kFailure,
                            std::unique_ptr<DeskTemplate>());
    return;
  }

  const base::GUID uuid = base::GUID::ParseCaseInsensitive(uuid_str);
  if (uuid.is_valid()) {
    std::move(callback).Run(GetEntryByUuidStatus::kInvalidUuid,
                            std::unique_ptr<DeskTemplate>());
    return;
  }

  auto it = entries_.find(uuid);
  if (it == entries_.end()) {
    std::move(callback).Run(GetEntryByUuidStatus::kNotFound,
                            std::unique_ptr<DeskTemplate>());
  } else {
    std::move(callback).Run(GetEntryByUuidStatus::kOk,
                            it->second.get()->Clone());
  }
}

void DeskSyncBridge::AddOrUpdateEntry(std::unique_ptr<DeskTemplate> new_entry,
                                      AddOrUpdateEntryCallback callback) {
  if (!IsReady()) {
    // This sync bridge has not finished initializing. Do not save the new entry
    // yet.
    std::move(callback).Run(AddOrUpdateEntryStatus::kFailure);
    return;
  }

  base::GUID uuid = new_entry->uuid();
  if (!uuid.is_valid()) {
    std::move(callback).Run(AddOrUpdateEntryStatus::kInvalidArgument);
    return;
  }

  std::string trimmed_name = base::UTF16ToUTF8(
      base::CollapseWhitespace(new_entry->template_name(), true));

  auto entry = std::make_unique<DeskTemplate>(
      uuid.AsLowercaseString(), trimmed_name, new_entry->created_time());

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  // Add/update this entry to the store and model.
  auto entity_data = CopyToEntityData(DeskSyncBridge::AsSyncProto(entry.get()));

  change_processor()->Put(uuid.AsLowercaseString(), std::move(entity_data),
                          batch->GetMetadataChangeList());

  entries_[uuid] = std::move(entry);
  const DeskTemplate* result = GetEntryByUUID(uuid);

  batch->WriteData(uuid.AsLowercaseString(),
                   DeskSyncBridge::AsSyncProto(result).SerializeAsString());

  Commit(std::move(batch));

  std::move(callback).Run(AddOrUpdateEntryStatus::kOk);
}

void DeskSyncBridge::DeleteEntry(const std::string& uuid_str,
                                 DeleteEntryCallback callback) {
  if (!IsReady()) {
    // This sync bridge has not finished initializing.
    // Cannot delete anything.
    std::move(callback).Run(DeleteEntryStatus::kFailure);
    return;
  }

  const base::GUID uuid = base::GUID::ParseCaseInsensitive(uuid_str);

  if (GetEntryByUUID(uuid) == nullptr) {
    // Consider the deletion successful if the entry does not exist.
    std::move(callback).Run(DeleteEntryStatus::kOk);
    return;
  }

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  change_processor()->Delete(uuid.AsLowercaseString(),
                             batch->GetMetadataChangeList());

  entries_.erase(uuid);

  batch->DeleteData(uuid.AsLowercaseString());

  Commit(std::move(batch));

  std::move(callback).Run(DeleteEntryStatus::kOk);
}

void DeskSyncBridge::DeleteAllEntries(DeleteEntryCallback callback) {
  if (!IsReady()) {
    // This sync bridge has not finished initializing.
    // Cannot delete anything.
    std::move(callback).Run(DeleteEntryStatus::kFailure);
    return;
  }

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  std::vector<std::string> all_uuids = GetAllUuids();

  for (const auto& uuid : all_uuids) {
    change_processor()->Delete(uuid, batch->GetMetadataChangeList());
    batch->DeleteData(uuid);
  }
  entries_.clear();

  std::move(callback).Run(DeleteEntryStatus::kOk);
}

bool DeskSyncBridge::IsReady() const {
  if (is_ready_) {
    DCHECK(store_);
  }
  return is_ready_;
}

bool DeskSyncBridge::IsSyncing() const {
  return change_processor()->IsTrackingMetadata();
}

std::vector<std::string> DeskSyncBridge::GetAllUuids() const {
  std::vector<std::string> keys;
  for (const auto& it : entries_) {
    DCHECK_EQ(it.first, it.second->uuid());
    keys.push_back(it.first.AsLowercaseString());
  }
  return keys;
}

const DeskTemplate* DeskSyncBridge::GetEntryByUUID(
    const base::GUID& uuid) const {
  auto it = entries_.find(uuid);
  if (it == entries_.end())
    return nullptr;
  return it->second.get();
}

void DeskSyncBridge::NotifyRemoteDeskTemplateAddedOrUpdated(
    const std::vector<const DeskTemplate*>& new_entries) {
  if (new_entries.empty()) {
    return;
  }

  for (DeskModelObserver& observer : observers_) {
    observer.EntriesAddedOrUpdatedRemotely(new_entries);
  }
}

void DeskSyncBridge::NotifyRemoteDeskTemplateDeleted(
    const std::vector<std::string>& uuids) {
  if (uuids.empty()) {
    return;
  }

  for (DeskModelObserver& observer : observers_) {
    observer.EntriesRemovedRemotely(uuids);
  }
}

void DeskSyncBridge::OnStoreCreated(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  auto stored_desk_templates = std::make_unique<DeskEntries>();
  DeskEntries* stored_desk_templates_copy = stored_desk_templates.get();

  store_ = std::move(store);
  store_->ReadAllDataAndPreprocess(
      base::BindOnce(&ParseDeskTemplatesOnBackendSequence,
                     base::Unretained(stored_desk_templates_copy)),
      base::BindOnce(&DeskSyncBridge::OnReadAllData,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(stored_desk_templates)));
}

void DeskSyncBridge::OnReadAllData(
    std::unique_ptr<DeskEntries> stored_desk_templates,
    const absl::optional<syncer::ModelError>& error) {
  DCHECK(stored_desk_templates);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  entries_ = std::move(*stored_desk_templates);

  store_->ReadAllMetadata(base::BindOnce(&DeskSyncBridge::OnReadAllMetadata,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void DeskSyncBridge::OnReadAllMetadata(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  is_ready_ = true;
}

void DeskSyncBridge::OnCommit(const absl::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void DeskSyncBridge::Commit(std::unique_ptr<ModelTypeStore::WriteBatch> batch) {
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&DeskSyncBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void DeskSyncBridge::UploadLocalOnlyData(
    syncer::MetadataChangeList* metadata_change_list,
    const syncer::EntityChangeList& entity_data) {
  std::set<base::GUID> local_keys_to_upload;
  for (const auto& it : entries_) {
    local_keys_to_upload.insert(it.first);
  }

  // Strip |local_keys_to_upload| of any key (UUID) that is already known to the
  // server.
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    local_keys_to_upload.erase(
        base::GUID::ParseCaseInsensitive(change->storage_key()));
  }

  // Upload the local-only templates.
  for (const base::GUID& uuid : local_keys_to_upload) {
    change_processor()->Put(uuid.AsLowercaseString(),
                            CopyToEntityData(AsSyncProto(entries_[uuid].get())),
                            metadata_change_list);
  }
}

}  // namespace desks_storage