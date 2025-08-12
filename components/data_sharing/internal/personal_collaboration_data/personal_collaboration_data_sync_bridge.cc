// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_sync_bridge.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_account_data_specifics.pb.h"

namespace data_sharing::personal_collaboration_data {
namespace {

// Trim specifics for use in TrimAllSupportedFieldsFromRemoteSpecifics.
// LINT.IfChange(TrimSpecifics)
sync_pb::SharedTabGroupAccountDataSpecifics TrimSpecifics(
    const sync_pb::SharedTabGroupAccountDataSpecifics& account_specifics) {
  sync_pb::SharedTabGroupAccountDataSpecifics trimmed_account_specifics =
      sync_pb::SharedTabGroupAccountDataSpecifics(account_specifics);
  trimmed_account_specifics.clear_guid();
  trimmed_account_specifics.clear_collaboration_id();
  trimmed_account_specifics.clear_update_time_windows_epoch_micros();
  trimmed_account_specifics.clear_version();

  if (trimmed_account_specifics.has_shared_tab_details()) {
    sync_pb::SharedTabDetails* tab =
        trimmed_account_specifics.mutable_shared_tab_details();
    tab->clear_shared_tab_group_guid();
    tab->clear_last_seen_timestamp_windows_epoch();

    if (tab->ByteSizeLong() == 0) {
      trimmed_account_specifics.clear_shared_tab_details();
    }
  }

  if (trimmed_account_specifics.has_shared_tab_group_details()) {
    sync_pb::SharedTabGroupDetails* tab_group =
        trimmed_account_specifics.mutable_shared_tab_group_details();
    tab_group->clear_pinned_position();

    if (tab_group->ByteSizeLong() == 0) {
      trimmed_account_specifics.clear_shared_tab_group_details();
    }
  }

  return trimmed_account_specifics;
}
// LINT.ThenChange(//components/sync/protocol/shared_tab_group_account_data_specifics.proto:SharedTabGroupAccountDataSpecifics)

// Create new EntityData object to contain specifics for writing changes.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromSpecifics(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_shared_tab_group_account_data() = specifics;
  entity_data->name = specifics.guid();
  return entity_data;
}

bool IsTabDetailsValid(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  if (!specifics.has_shared_tab_details()) {
    // Non-tab account specifics should be handled here.
    return false;
  }

  const sync_pb::SharedTabDetails& tab_details = specifics.shared_tab_details();
  if (!base::Uuid::ParseCaseInsensitive(tab_details.shared_tab_group_guid())
           .is_valid() ||
      !tab_details.has_last_seen_timestamp_windows_epoch()) {
    return false;
  }

  return true;
}

bool IsTabGroupDetailsValid(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  return specifics.has_shared_tab_group_details();
}

}  // namespace

PersonalCollaborationDataSyncBridge::PersonalCollaborationDataSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory data_type_store_factory)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  std::move(data_type_store_factory)
      .Run(syncer::SHARED_TAB_GROUP_ACCOUNT_DATA,
           base::BindOnce(&PersonalCollaborationDataSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

PersonalCollaborationDataSyncBridge::~PersonalCollaborationDataSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PersonalCollaborationDataSyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PersonalCollaborationDataSyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::unique_ptr<syncer::MetadataChangeList>
PersonalCollaborationDataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError>
PersonalCollaborationDataSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Since this data type is controlled along with shared tab group data,
  // there will never be any shared tab groups in the model, therefore no
  // data to merge, when this data type is enabled.

  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_change_list));
}

std::optional<syncer::ModelError>
PersonalCollaborationDataSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change :
       entity_change_list) {
    const sync_pb::EntitySpecifics& entity_specifics = change->data().specifics;

    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        CHECK(entity_specifics.has_shared_tab_group_account_data());
        const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
            entity_specifics.shared_tab_group_account_data();

        specifics_[change->storage_key()] = specifics;
        batch->WriteData(change->storage_key(), specifics.SerializeAsString());
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        specifics_.erase(change->storage_key());
        batch->DeleteData(change->storage_key());
        break;
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(
          &PersonalCollaborationDataSyncBridge::OnDataTypeStoreCommit,
          weak_ptr_factory_.GetWeakPtr()));

  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
PersonalCollaborationDataSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    if (specifics_.contains(storage_key)) {
      batch->Put(storage_key,
                 CreateEntityDataFromSpecifics(specifics_[storage_key]));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
PersonalCollaborationDataSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [storage_key, specifics] : specifics_) {
    batch->Put(storage_key, CreateEntityDataFromSpecifics(specifics));
  }
  return batch;
}

std::string PersonalCollaborationDataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  // Client tags are not computed from the specifics.
  NOTREACHED();
}

std::string PersonalCollaborationDataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  // Storage keys are not computed from the specifics.
  NOTREACHED();
}

bool PersonalCollaborationDataSyncBridge::SupportsGetClientTag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

bool PersonalCollaborationDataSyncBridge::SupportsGetStorageKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

void PersonalCollaborationDataSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  specifics_.clear();
  store_->DeleteAllDataAndMetadata(base::DoNothing());
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool PersonalCollaborationDataSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
      entity_data.specifics.shared_tab_group_account_data();
  if (!base::Uuid::ParseCaseInsensitive(specifics.guid()).is_valid() ||
      specifics.collaboration_id().empty()) {
    return false;
  }

  return IsTabDetailsValid(specifics) || IsTabGroupDetailsValid(specifics);
}

sync_pb::EntitySpecifics
PersonalCollaborationDataSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const sync_pb::SharedTabGroupAccountDataSpecifics trimmed_specifics =
      TrimSpecifics(entity_specifics.shared_tab_group_account_data());

  if (trimmed_specifics.ByteSizeLong() == 0u) {
    return sync_pb::EntitySpecifics();
  }

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  *trimmed_entity_specifics.mutable_shared_tab_group_account_data() =
      trimmed_specifics;
  return trimmed_entity_specifics;
}

syncer::ConflictResolution PersonalCollaborationDataSyncBridge::ResolveConflict(
    const std::string& storage_key,
    const syncer::EntityData& remote_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we are not tracking this storage_key, accept the remote change.
  if (!specifics_.contains(storage_key)) {
    return syncer::ConflictResolution::kUseRemote;
  }

  const sync_pb::SharedTabGroupAccountDataSpecifics local_specifics =
      specifics_.at(storage_key);
  const sync_pb::SharedTabGroupAccountDataSpecifics remote_specifics =
      remote_data.specifics.shared_tab_group_account_data();

  // The account data specifics can contain different types of details.
  // If these specifics both contain TabDetails, compare the timestamps
  // to see which one should be retained.
  if (local_specifics.has_shared_tab_details() &&
      remote_specifics.has_shared_tab_details()) {
    const int64_t local_timestamp = local_specifics.shared_tab_details()
                                        .last_seen_timestamp_windows_epoch();
    const int64_t remote_timestamp = remote_specifics.shared_tab_details()
                                         .last_seen_timestamp_windows_epoch();

    if (local_timestamp == remote_timestamp) {
      // Timestamps match.
      return syncer::ConflictResolution::kChangesMatch;
    } else if (local_timestamp > remote_timestamp) {
      // A local change has been Put to sync containing the more recent
      // time. Discard the remote change as it is outdated.
      return syncer::ConflictResolution::kUseLocal;
    } else {
      // A remote change contains a more recent timestamp than a change
      // that was Put locally. Discard the outdated local change.
      return syncer::ConflictResolution::kUseRemote;
    }
  }

  return syncer::ConflictResolution::kUseRemote;
}

bool PersonalCollaborationDataSyncBridge::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_initialized_;
}

std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>
PersonalCollaborationDataSyncBridge::GetSpecificsForStorageKey(
    const std::string& storage_key) const {
  return specifics_.contains(storage_key)
             ? std::make_optional<>(specifics_.at(storage_key))
             : std::nullopt;
}

void PersonalCollaborationDataSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(base::BindOnce(
      &PersonalCollaborationDataSyncBridge::OnReadAllDataAndMetadata,
      weak_ptr_factory_.GetWeakPtr()));
}

void PersonalCollaborationDataSyncBridge::OnReadAllDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  specifics_.reserve(entries->size());
  for (const syncer::DataTypeStore::Record& r : *entries) {
    sync_pb::SharedTabGroupAccountDataSpecifics specifics;
    if (!specifics.ParseFromString(r.value)) {
      // Ignore invalid entries.
      continue;
    }
    const std::string storage_key = r.id;
    specifics_[storage_key] = specifics;
  }

  is_initialized_ = true;
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void PersonalCollaborationDataSyncBridge::OnDataTypeStoreCommit(
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    change_processor()->ReportError(*error);
  }
}

void PersonalCollaborationDataSyncBridge::WriteEntityToSync(
    std::unique_ptr<syncer::EntityData> entity) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
      entity->specifics.shared_tab_group_account_data();
  const std::string storage_key = GetStorageKey(*entity);

  specifics_[storage_key] = specifics;
  batch->WriteData(storage_key, specifics.SerializeAsString());

  change_processor()->Put(storage_key, std::move(entity),
                          batch->GetMetadataChangeList());

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(
          &PersonalCollaborationDataSyncBridge::OnDataTypeStoreCommit,
          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace data_sharing::personal_collaboration_data
