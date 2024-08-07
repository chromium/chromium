// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_sync_bridge.h"

#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/window_info.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_storage_metrics_util.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/desks_storage/core/desk_template_util.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/lacros_startup_state.h"  // nogncheck
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace desks_storage {

using BrowserAppTab =
    sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow_BrowserAppTab;
using BrowserAppWindow = sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow;
using ArcApp = sync_pb::WorkspaceDeskSpecifics_ArcApp;
using ArcAppWindowSize = sync_pb::WorkspaceDeskSpecifics_ArcApp_WindowSize;
using ash::DeskTemplate;
using ash::DeskTemplateSource;
using ash::DeskTemplateType;
using SyncDeskType = sync_pb::WorkspaceDeskSpecifics_DeskType;
using WindowState = sync_pb::WorkspaceDeskSpecifics_WindowState;
using WindowBound = sync_pb::WorkspaceDeskSpecifics_WindowBound;
using LaunchContainer = sync_pb::WorkspaceDeskSpecifics_LaunchContainer;
// Use name prefixed with Sync here to avoid name collision with original class
// which isn't defined in a namespace.
using SyncWindowOpenDisposition =
    sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition;
using ProgressiveWebApp = sync_pb::WorkspaceDeskSpecifics_ProgressiveWebApp;
using ChromeApp = sync_pb::WorkspaceDeskSpecifics_ChromeApp;
using WorkspaceDeskSpecifics_App = sync_pb::WorkspaceDeskSpecifics_App;
using SyncTabGroup = sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow_TabGroup;
using SyncTabGroupColor = sync_pb::WorkspaceDeskSpecifics_TabGroupColor;
using TabGroupColor = tab_groups::TabGroupColorId;

namespace {

using syncer::DataTypeStore;

// The maximum number of templates the chrome sync storage can hold.
constexpr size_t kMaxTemplateCount = 6u;

// The maximum number of bytes a template can be.
// Sync server silently ignores large items. The client-side
// needs to check item size to avoid sending large items.
// This limit follows precedent set by the chrome extension API:
// chrome.storage.sync.QUOTA_BYTES_PER_ITEM.
constexpr size_t kMaxTemplateSize = 8192u;

// Allocate a EntityData and copies `specifics` into it.
std::unique_ptr<syncer::EntityData> CopyToEntityData(
    const sync_pb::WorkspaceDeskSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_workspace_desk() = specifics;
  entity_data->name = specifics.uuid();
  entity_data->creation_time = desk_template_conversion::ProtoTimeToTime(
      specifics.created_time_windows_epoch_micros());
  return entity_data;
}

// Parses the content of `record_list` into `*desk_templates`. The output
// parameters are first for binding purposes.
std::optional<syncer::ModelError> ParseDeskTemplatesOnBackendSequence(
    base::flat_map<base::Uuid, std::unique_ptr<DeskTemplate>>* desk_templates,
    std::unique_ptr<DataTypeStore::RecordList> record_list) {
  DCHECK(desk_templates);
  DCHECK(desk_templates->empty());
  DCHECK(record_list);

  for (const syncer::DataTypeStore::Record& r : *record_list) {
    auto specifics = std::make_unique<sync_pb::WorkspaceDeskSpecifics>();
    if (specifics->ParseFromString(r.value)) {
      const base::Uuid uuid =
          base::Uuid::ParseCaseInsensitive(specifics->uuid());
      if (!uuid.is_valid()) {
        return syncer::ModelError(
            FROM_HERE,
            base::StringPrintf("Failed to parse WorkspaceDeskSpecifics uuid %s",
                               specifics->uuid().c_str()));
      }

      std::unique_ptr<ash::DeskTemplate> entry =
          desk_template_conversion::FromSyncProto(*specifics);

      if (!entry)
        continue;
      (*desk_templates)[uuid] = std::move(entry);
    } else {
      return syncer::ModelError(
          FROM_HERE, "Failed to deserialize WorkspaceDeskSpecifics.");
    }
  }

  return std::nullopt;
}

}  // namespace

DeskSyncBridge::DeskSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    const AccountId& account_id)
    : DataTypeSyncBridge(std::move(change_processor)),
      is_ready_(false),
      account_id_(account_id) {
  std::move(create_store_callback)
      .Run(syncer::WORKSPACE_DESK,
           base::BindOnce(&DeskSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

DeskSyncBridge::~DeskSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
DeskSyncBridge::CreateMetadataChangeList() {
  return DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError> DeskSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // MergeFullSyncData will be called when Desk Template data type is enabled
  // to start syncing. There could be local desk templates that user has created
  // before enabling sync or during the time when Desk Template sync is
  // disabled. We should merge local and server data. We will send all
  // local-only templates to server and save server templates to local.

  UploadLocalOnlyData(metadata_change_list.get(), entity_data);

  // Apply server changes locally. Currently, if a template exists on both
  // local and server side, the server version will win.
  // TODO(yzd) We will add a template update timestamp and update this logic to
  // be: for templates that exist on both local and server side, we will keep
  // the one with later update timestamp.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<syncer::ModelError> DeskSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> added_or_updated;
  std::vector<base::Uuid> removed;
  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const base::Uuid uuid =
        base::Uuid::ParseCaseInsensitive(change->storage_key());
    if (!uuid.is_valid()) {
      // Skip invalid storage keys.
      continue;
    }

    switch (change->type()) {
      case syncer::EntityChange::ACTION_DELETE: {
        if (desk_template_entries_.find(uuid) != desk_template_entries_.end()) {
          desk_template_entries_.erase(uuid);
          batch->DeleteData(uuid.AsLowercaseString());
          removed.push_back(uuid);
        }
        break;
      }
      case syncer::EntityChange::ACTION_UPDATE:
      case syncer::EntityChange::ACTION_ADD: {
        const sync_pb::WorkspaceDeskSpecifics& specifics =
            change->data().specifics.workspace_desk();

        std::unique_ptr<DeskTemplate> remote_entry =
            desk_template_conversion::FromSyncProto(specifics);
        if (!remote_entry) {
          // Skip invalid entries.
          continue;
        }

        DCHECK_EQ(uuid, remote_entry->uuid());
        std::string serialized_remote_entry = specifics.SerializeAsString();

        // Add/update the remote_entry to the model.
        desk_template_entries_[uuid] = std::move(remote_entry);
        added_or_updated.push_back(GetUserEntryByUUID(uuid));

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

  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> DeskSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& uuid : storage_keys) {
    const DeskTemplate* entry =
        GetUserEntryByUUID(base::Uuid::ParseCaseInsensitive(uuid));
    if (!entry) {
      continue;
    }

    batch->Put(uuid, CopyToEntityData(desk_template_conversion::ToSyncProto(
                         entry, apps::AppRegistryCacheWrapper::Get()
                                    .GetAppRegistryCache(account_id_))));
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch> DeskSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& it : desk_template_entries_) {
    batch->Put(it.first.AsLowercaseString(),
               CopyToEntityData(desk_template_conversion::ToSyncProto(
                   it.second.get(),
                   apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
                       account_id_))));
  }
  return batch;
}

std::string DeskSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string DeskSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.workspace_desk().uuid();
}

DeskModel::GetAllEntriesResult DeskSyncBridge::GetAllEntries() {
  if (!IsReady()) {
    LOG(WARNING) << "Unable to get all entries: Not Ready";
    return GetAllEntriesResult(
        GetAllEntriesStatus::kFailure,
        std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>());
  }

  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> entries;

  for (const auto& it : policy_entries_)
    entries.push_back(it.get());

  for (const auto& it : desk_template_entries_) {
    DCHECK_EQ(it.first, it.second->uuid());
    entries.push_back(it.second.get());
  }

  return GetAllEntriesResult(GetAllEntriesStatus::kOk, std::move(entries));
}

DeskModel::GetEntryByUuidResult DeskSyncBridge::GetEntryByUUID(
    const base::Uuid& uuid) {
  if (!IsReady()) {
    LOG(WARNING) << "Unable to get entry by UUID: Not Ready";
    return GetEntryByUuidResult(GetEntryByUuidStatus::kFailure, nullptr);
  }

  if (!uuid.is_valid()) {
    LOG(WARNING) << "Unable to get entry by UUID: Invalid UUID";
    return GetEntryByUuidResult(GetEntryByUuidStatus::kInvalidUuid, nullptr);
  }

  auto it = desk_template_entries_.find(uuid);
  if (it == desk_template_entries_.end()) {
    std::unique_ptr<DeskTemplate> policy_entry =
        GetAdminDeskTemplateByUUID(uuid);

    if (policy_entry) {
      return GetEntryByUuidResult(GetEntryByUuidStatus::kOk,
                                  std::move(policy_entry));
    } else {
      LOG(WARNING) << "Unable to get entry by UUID: Entry not found";
      return GetEntryByUuidResult(GetEntryByUuidStatus::kNotFound, nullptr);
    }
  } else {
    return GetEntryByUuidResult(GetEntryByUuidStatus::kOk,
                                it->second.get()->Clone());
  }
}

void DeskSyncBridge::AddOrUpdateEntry(std::unique_ptr<DeskTemplate> new_entry,
                                      AddOrUpdateEntryCallback callback) {
  if (!IsReady()) {
    // This sync bridge has not finished initializing. Do not save the new entry
    // yet.
    LOG(WARNING) << "Unable to add or update entry: Not Ready";
    std::move(callback).Run(AddOrUpdateEntryStatus::kFailure,
                            std::move(new_entry));
    return;
  }

  if (!new_entry) {
    LOG(WARNING) << "Unable to add or update entry: No new entry";
    std::move(callback).Run(AddOrUpdateEntryStatus::kInvalidArgument,
                            std::move(new_entry));
    return;
  }

  base::Uuid uuid = new_entry->uuid();
  if (!uuid.is_valid()) {
    LOG(WARNING) << "Unable to add or update entry: Invalid UUID";
    std::move(callback).Run(AddOrUpdateEntryStatus::kInvalidArgument,
                            std::move(new_entry));
    return;
  }

  // When a user creates a desk template locally, the desk template has `kUser`
  // as its source. Only user desk templates should be saved to Sync.
  DCHECK_EQ(DeskTemplateSource::kUser, new_entry->source());
  new_entry->set_client_cache_guid(change_processor()->TrackedCacheGuid());
  auto entry = new_entry->Clone();
  entry->set_template_name(
      base::CollapseWhitespace(new_entry->template_name(), true));

  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();

  // Check the new entry size and ensure it is below the size limit.
  auto sync_proto = desk_template_conversion::ToSyncProto(
      entry.get(),
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_));
  RecordSavedDeskTemplateSizeHistogram(new_entry->type(),
                                       sync_proto.ByteSizeLong());
  if (sync_proto.ByteSizeLong() > kMaxTemplateSize) {
    LOG(WARNING) << "Unable to add or update entry: Entry is too large";
    std::move(callback).Run(AddOrUpdateEntryStatus::kEntryTooLarge,
                            std::move(new_entry));
    return;
  }

  // Add/update this entry to the store and model.
  change_processor()->Put(uuid.AsLowercaseString(),
                          CopyToEntityData(sync_proto),
                          batch->GetMetadataChangeList());

  desk_template_entries_[uuid] =
      desk_template_conversion::FromSyncProto(sync_proto);
  const DeskTemplate* result = GetUserEntryByUUID(uuid);

  batch->WriteData(
      uuid.AsLowercaseString(),
      desk_template_conversion::ToSyncProto(
          result,
          apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_))
          .SerializeAsString());

  Commit(std::move(batch));

  std::move(callback).Run(AddOrUpdateEntryStatus::kOk, std::move(new_entry));
}

void DeskSyncBridge::DeleteEntry(const base::Uuid& uuid,
                                 DeleteEntryCallback callback) {
  if (!IsReady()) {
    // This sync bridge has not finished initializing.
    // Cannot delete anything.
    LOG(WARNING) << "Unable to delete entry: Not Ready";
    std::move(callback).Run(DeleteEntryStatus::kFailure);
    return;
  }

  if (GetUserEntryByUUID(uuid) == nullptr) {
    // Consider the deletion successful if the entry does not exist.
    std::move(callback).Run(DeleteEntryStatus::kOk);
    return;
  }

  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();

  change_processor()->Delete(uuid.AsLowercaseString(),
                             syncer::DeletionOrigin::Unspecified(),
                             batch->GetMetadataChangeList());

  desk_template_entries_.erase(uuid);

  batch->DeleteData(uuid.AsLowercaseString());

  Commit(std::move(batch));

  std::move(callback).Run(DeleteEntryStatus::kOk);
}

void DeskSyncBridge::DeleteAllEntries(DeleteEntryCallback callback) {
  DeleteEntryStatus status = DeleteAllEntriesSync();
  std::move(callback).Run(status);
}

DeskModel::DeleteEntryStatus DeskSyncBridge::DeleteAllEntriesSync() {
  if (!IsReady()) {
    // This sync bridge has not finished initializing.
    // Cannot delete anything.
    LOG(WARNING) << "Unable to delete entries: Not Ready";
    return DeleteEntryStatus::kFailure;
  }

  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();

  std::set<base::Uuid> all_uuids = GetAllEntryUuids();

  for (const auto& uuid : all_uuids) {
    change_processor()->Delete(uuid.AsLowercaseString(),
                               syncer::DeletionOrigin::Unspecified(),
                               batch->GetMetadataChangeList());
    batch->DeleteData(uuid.AsLowercaseString());
  }
  desk_template_entries_.clear();
  return DeleteEntryStatus::kOk;
}

size_t DeskSyncBridge::GetEntryCount() const {
  return GetSaveAndRecallDeskEntryCount() + GetDeskTemplateEntryCount();
}

// Return 0 for now since chrome sync does not support save and recall desks.
size_t DeskSyncBridge::GetSaveAndRecallDeskEntryCount() const {
  return 0u;
}

size_t DeskSyncBridge::GetDeskTemplateEntryCount() const {
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
size_t DeskSyncBridge::GetMaxSaveAndRecallDeskEntryCount() const {
  return 0u;
}

size_t DeskSyncBridge::GetMaxDeskTemplateEntryCount() const {
  return kMaxTemplateCount + policy_entries_.size();
}

std::set<base::Uuid> DeskSyncBridge::GetAllEntryUuids() const {
  std::set<base::Uuid> keys;

  for (const auto& it : policy_entries_)
    keys.emplace(it.get()->uuid());

  for (const auto& it : desk_template_entries_) {
    DCHECK_EQ(it.first, it.second->uuid());
    keys.emplace(it.first);
  }
  return keys;
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

// TODO(zhumatthew): Once desk sync bridge supports save and recall desk type,
// update this method to search the correct cache for the entry.
ash::DeskTemplate* DeskSyncBridge::FindOtherEntryWithName(
    const std::u16string& name,
    ash::DeskTemplateType type,
    const base::Uuid& uuid) const {
  return desk_template_util::FindOtherEntryWithName(name, uuid,
                                                    desk_template_entries_);
}

const DeskTemplate* DeskSyncBridge::GetUserEntryByUUID(
    const base::Uuid& uuid) const {
  auto it = desk_template_entries_.find(uuid);
  if (it == desk_template_entries_.end())
    return nullptr;
  return it->second.get();
}

void DeskSyncBridge::NotifyDeskModelLoaded() {
  for (DeskModelObserver& observer : observers_) {
    observer.DeskModelLoaded();
  }
}

void DeskSyncBridge::NotifyRemoteDeskTemplateAddedOrUpdated(
    const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>&
        new_entries) {
  if (new_entries.empty()) {
    return;
  }

  for (DeskModelObserver& observer : observers_) {
    observer.EntriesAddedOrUpdatedRemotely(new_entries);
  }
}

void DeskSyncBridge::NotifyRemoteDeskTemplateDeleted(
    const std::vector<base::Uuid>& uuids) {
  if (uuids.empty()) {
    return;
  }

  for (DeskModelObserver& observer : observers_) {
    observer.EntriesRemovedRemotely(uuids);
  }
}

void DeskSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
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
    const std::optional<syncer::ModelError>& error) {
  DCHECK(stored_desk_templates);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  desk_template_entries_ = std::move(*stored_desk_templates);
  store_->ReadAllMetadata(base::BindOnce(&DeskSyncBridge::OnReadAllMetadata,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void DeskSyncBridge::OnReadAllMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("ui", "DeskSyncBridge::OnReadAllMetadata");
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  is_ready_ = true;
  NotifyDeskModelLoaded();
}

void DeskSyncBridge::OnCommit(const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void DeskSyncBridge::Commit(std::unique_ptr<DataTypeStore::WriteBatch> batch) {
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&DeskSyncBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void DeskSyncBridge::UploadLocalOnlyData(
    syncer::MetadataChangeList* metadata_change_list,
    const syncer::EntityChangeList& entity_data) {
  std::set<base::Uuid> local_keys_to_upload;
  for (const auto& it : desk_template_entries_) {
    DCHECK_EQ(DeskTemplateSource::kUser, it.second->source());
    local_keys_to_upload.insert(it.first);
  }

  // Strip `local_keys_to_upload` of any key (UUID) that is already known to the
  // server.
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    local_keys_to_upload.erase(
        base::Uuid::ParseCaseInsensitive(change->storage_key()));
  }

  // Upload the local-only templates.
  for (const base::Uuid& uuid : local_keys_to_upload) {
    change_processor()->Put(
        uuid.AsLowercaseString(),
        CopyToEntityData(desk_template_conversion::ToSyncProto(
            desk_template_entries_[uuid].get(),
            apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
                account_id_))),
        metadata_change_list);
  }
}

bool DeskSyncBridge::HasUserTemplateWithName(const std::u16string& name) {
  return base::Contains(desk_template_entries_, name,
                        [](const DeskEntries::value_type& entry) {
                          return entry.second->template_name();
                        });
}

bool DeskSyncBridge::HasUuid(const base::Uuid& uuid) const {
  return uuid.is_valid() && base::Contains(desk_template_entries_, uuid);
}

std::string DeskSyncBridge::GetCacheGuid() {
  return change_processor()->TrackedCacheGuid();
}

}  // namespace desks_storage
