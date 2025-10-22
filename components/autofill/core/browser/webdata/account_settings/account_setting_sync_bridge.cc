// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_bridge.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/account_setting_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {

namespace {

// Macro to simplify reporting errors raised by ModelTypeStore operations.
#define RETURN_IF_ERROR(error)               \
  if (error) {                               \
    change_processor()->ReportError(*error); \
    return;                                  \
  }

std::unique_ptr<syncer::EntityData> CreateEntityData(
    const sync_pb::AccountSettingSpecifics& specifics) {
  auto entity = std::make_unique<syncer::EntityData>();
  entity->name = specifics.name();
  entity->specifics.mutable_account_setting()->CopyFrom(specifics);
  return entity;
}

}  // namespace

AccountSettingSyncBridge::AccountSettingSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : DataTypeSyncBridge(std::move(change_processor)) {
  std::move(store_factory)
      .Run(syncer::ACCOUNT_SETTING,
           base::BindOnce(&AccountSettingSyncBridge::OnStoreCreated,
                          weak_factory_.GetWeakPtr()));
}

AccountSettingSyncBridge::~AccountSettingSyncBridge() = default;

void AccountSettingSyncBridge::AddObserver(
    AccountSettingSyncBridge::Observer* observer) {
  observers_.AddObserver(observer);
}

void AccountSettingSyncBridge::RemoveObserver(
    AccountSettingSyncBridge::Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::optional<bool> AccountSettingSyncBridge::GetBoolSetting(
    std::string_view name) const {
  auto it = settings_.find(name);
  if (it == settings_.end() || !it->second.has_bool_value()) {
    return std::nullopt;
  }
  return it->second.bool_value();
}

std::optional<int> AccountSettingSyncBridge::GetIntSetting(
    std::string_view name) const {
  auto it = settings_.find(name);
  if (it == settings_.end() || !it->second.has_int_value()) {
    return std::nullopt;
  }
  return it->second.int_value();
}

std::optional<std::string> AccountSettingSyncBridge::GetStringSetting(
    std::string_view name) const {
  auto it = settings_.find(name);
  if (it == settings_.end() || !it->second.has_string_value()) {
    return std::nullopt;
  }
  return it->second.string_value();
}

std::unique_ptr<syncer::MetadataChangeList>
AccountSettingSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError> AccountSettingSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // Since ACCOUNT_SETTING is read-only, merging local and sync data is the same
  // as applying changes from sync locally.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<syncer::ModelError>
AccountSettingSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        const sync_pb::AccountSettingSpecifics& specifics =
            change->data().specifics.account_setting();
        batch->WriteData(change->storage_key(), specifics.SerializeAsString());
        settings_.insert_or_assign(change->storage_key(), specifics);
        break;
      }
      case syncer::EntityChange::ACTION_DELETE: {
        batch->DeleteData(change->storage_key());
        settings_.erase(change->storage_key());
        break;
      }
    }
  }
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&AccountSettingSyncBridge::ReportErrorIfSet,
                     weak_factory_.GetWeakPtr()));
  return std::nullopt;
}

void AccountSettingSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  store_->DeleteAllDataAndMetadata(base::BindOnce(
      &AccountSettingSyncBridge::ReportErrorIfSet, weak_factory_.GetWeakPtr()));
  settings_.clear();
}

std::unique_ptr<syncer::DataBatch> AccountSettingSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  // ACCOUNT_SETTING is read-only, so `GetDataForCommit()` is not needed.
  NOTREACHED();
}

std::unique_ptr<syncer::DataBatch>
AccountSettingSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [name, specifics] : settings_) {
    batch->Put(name, CreateEntityData(specifics));
  }
  return batch;
}

bool AccountSettingSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  CHECK(entity_data.specifics.has_account_setting());
  const sync_pb::AccountSettingSpecifics& specifics =
      entity_data.specifics.account_setting();
  return !specifics.name().empty();
}

std::string AccountSettingSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string AccountSettingSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.account_setting().name();
}

void AccountSettingSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RETURN_IF_ERROR(error);
  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(
      base::BindOnce(&AccountSettingSyncBridge::StartSyncingWithDataAndMetadata,
                     weak_factory_.GetWeakPtr()));
}

void AccountSettingSyncBridge::StartSyncingWithDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> data,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RETURN_IF_ERROR(error);
  // Initialize the `settings_` with the `data`.
  std::vector<std::pair<std::string, sync_pb::AccountSettingSpecifics>>
      processed_entries;
  for (const syncer::DataTypeStore::Record& record : *data) {
    if (sync_pb::AccountSettingSpecifics specifics;
        specifics.ParseFromString(record.value)) {
      processed_entries.emplace_back(record.id, std::move(specifics));
    }
  }
  settings_ = base::flat_map<std::string, sync_pb::AccountSettingSpecifics>(
      std::move(processed_entries));
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  observers_.Notify(&Observer::OnDataLoadedFromDisk);
}

void AccountSettingSyncBridge::ReportErrorIfSet(
    const std::optional<syncer::ModelError>& error) {
  RETURN_IF_ERROR(error);
}

}  // namespace autofill
