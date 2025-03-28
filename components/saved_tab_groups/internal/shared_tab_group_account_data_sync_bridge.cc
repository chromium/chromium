// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/shared_tab_group_account_data_sync_bridge.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_account_data_specifics.pb.h"

namespace tab_groups {
namespace {

std::unique_ptr<syncer::EntityData> SpecificsToEntityData(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_shared_tab_group_account_data() = specifics;
  entity_data->name = specifics.guid();
  return entity_data;
}

std::string GetClientTagFromSpecifics(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  return specifics.guid() + "|" + specifics.collaboration_id();
}

sync_pb::SharedTabGroupAccountDataSpecifics TrimSpecifics(
    const sync_pb::SharedTabGroupAccountDataSpecifics& account_specifics) {
  sync_pb::SharedTabGroupAccountDataSpecifics trimmed_account_specifics =
      sync_pb::SharedTabGroupAccountDataSpecifics(account_specifics);
  trimmed_account_specifics.clear_guid();
  trimmed_account_specifics.clear_collaboration_id();
  trimmed_account_specifics.mutable_shared_tab_details()
      ->clear_shared_tab_group_guid();
  trimmed_account_specifics.mutable_shared_tab_details()
      ->clear_last_seen_timestamp_windows_epoch();
  return trimmed_account_specifics;
}

}  // namespace

SharedTabGroupAccountDataSyncBridge::SharedTabGroupAccountDataSyncBridge(
    std::unique_ptr<SyncDataTypeConfiguration> configuration)
    : syncer::DataTypeSyncBridge(std::move(configuration->change_processor)) {
  std::move(configuration->data_type_store_factory)
      .Run(syncer::SHARED_TAB_GROUP_ACCOUNT_DATA,
           base::BindOnce(&SharedTabGroupAccountDataSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

SharedTabGroupAccountDataSyncBridge::~SharedTabGroupAccountDataSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
SharedTabGroupAccountDataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError>
SharedTabGroupAccountDataSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(specifics_.empty());

  // Since this data type is grouped with shared tab group data, there
  // will never be any shared tab groups in the model, therefore no data
  // to merge, when this data type is enabled.

  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_change_list));
}

std::optional<syncer::ModelError>
SharedTabGroupAccountDataSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change :
       entity_change_list) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        const sync_pb::EntitySpecifics& entity_specifics =
            change->data().specifics;
        // Guaranteed by ClientTagBasedDataTypeProcessor, based on
        // IsEntityDataValid().
        CHECK(entity_specifics.has_shared_tab_group_account_data());
        const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
            entity_specifics.shared_tab_group_account_data();

        batch->WriteData(change->storage_key(), specifics.SerializeAsString());
        specifics_[change->storage_key()] = specifics;
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        specifics_.erase(change->storage_key());
        batch->DeleteData(change->storage_key());
        break;
    }
  }

  // TODO(crbug.com/397767033): Notify observers

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(
          &SharedTabGroupAccountDataSyncBridge::OnDataTypeStoreCommit,
          weak_ptr_factory_.GetWeakPtr()));

  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
SharedTabGroupAccountDataSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    if (specifics_.contains(storage_key)) {
      batch->Put(storage_key, SpecificsToEntityData(specifics_[storage_key]));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
SharedTabGroupAccountDataSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [storage_key, specifics] : specifics_) {
    batch->Put(storage_key, SpecificsToEntityData(specifics));
  }
  return batch;
}

std::string SharedTabGroupAccountDataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClientTagFromSpecifics(
      entity_data.specifics.shared_tab_group_account_data());
}

std::string SharedTabGroupAccountDataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClientTag(entity_data);
}

bool SharedTabGroupAccountDataSyncBridge::SupportsGetClientTag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

bool SharedTabGroupAccountDataSyncBridge::SupportsGetStorageKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void SharedTabGroupAccountDataSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/397767033): Get guids before clearing to notify
  // observer.

  specifics_.clear();
  store_->DeleteAllDataAndMetadata(base::DoNothing());
  weak_ptr_factory_.InvalidateWeakPtrs();

  // TODO(crbug.com/397767033): Notify observers
}

bool SharedTabGroupAccountDataSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
      entity_data.specifics.shared_tab_group_account_data();
  if (!base::Uuid::ParseCaseInsensitive(specifics.guid()).is_valid() ||
      specifics.collaboration_id().empty()) {
    return false;
  }

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

sync_pb::EntitySpecifics
SharedTabGroupAccountDataSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sync_pb::SharedTabGroupAccountDataSpecifics trimmed_specifics =
      TrimSpecifics(entity_specifics.shared_tab_group_account_data());

  if (trimmed_specifics.ByteSizeLong() == 0u) {
    return sync_pb::EntitySpecifics();
  }

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  *trimmed_entity_specifics.mutable_shared_tab_group_account_data() =
      trimmed_specifics;
  return trimmed_entity_specifics;
}

bool SharedTabGroupAccountDataSyncBridge::IsInitialized() const {
  return is_initialized_;
}

void SharedTabGroupAccountDataSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(base::BindOnce(
      &SharedTabGroupAccountDataSyncBridge::OnReadAllDataAndMetadata,
      weak_ptr_factory_.GetWeakPtr()));
}

void SharedTabGroupAccountDataSyncBridge::OnReadAllDataAndMetadata(
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
    if (specifics.guid() != r.id) {
      // GUID is used as a storage key, so it should always match.
      continue;
    }
    specifics_[GetClientTagFromSpecifics(specifics)] = std::move(specifics);
  }

  is_initialized_ = true;

  // TODO(crbug.com/397767033): Notify observers

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void SharedTabGroupAccountDataSyncBridge::OnDataTypeStoreCommit(
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    change_processor()->ReportError(*error);
  }
}

}  // namespace tab_groups
