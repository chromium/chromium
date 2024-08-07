// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/plus_address_setting_sync_bridge.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/plus_address_setting_specifics.pb.h"

namespace plus_addresses {

// Macro to simplify reporting errors raised by DataTypeStore operations.
#undef RETURN_IF_ERROR
#define RETURN_IF_ERROR(error)               \
  if (error) {                               \
    change_processor()->ReportError(*error); \
    return;                                  \
  }

namespace {

std::unique_ptr<syncer::EntityData> CreateEntityData(
    const sync_pb::PlusAddressSettingSpecifics& specifics) {
  auto entity = std::make_unique<syncer::EntityData>();
  entity->name = specifics.name();
  entity->specifics.mutable_plus_address_setting()->CopyFrom(specifics);
  return entity;
}

}  // namespace

PlusAddressSettingSyncBridge::PlusAddressSettingSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : DataTypeSyncBridge(std::move(change_processor)) {
  std::move(store_factory)
      .Run(syncer::PLUS_ADDRESS_SETTING,
           base::BindOnce(&PlusAddressSettingSyncBridge::OnStoreCreated,
                          weak_factory_.GetWeakPtr()));
}

PlusAddressSettingSyncBridge::~PlusAddressSettingSyncBridge() = default;

// static
std::unique_ptr<PlusAddressSettingSyncBridge>
PlusAddressSettingSyncBridge::CreateBridge(
    syncer::OnceDataTypeStoreFactory store_factory) {
  if (!base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting)) {
    return nullptr;
  }
  return std::make_unique<plus_addresses::PlusAddressSettingSyncBridge>(
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::PLUS_ADDRESS_SETTING,
          /*dump_stack=*/base::DoNothing()),
      std::move(store_factory));
}

std::optional<sync_pb::PlusAddressSettingSpecifics>
PlusAddressSettingSyncBridge::GetSetting(std::string_view name) const {
  auto it = settings_.find(name);
  if (it == settings_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void PlusAddressSettingSyncBridge::WriteSetting(
    const sync_pb::PlusAddressSettingSpecifics& specifics) {
  if (!store_ || !change_processor()->IsTrackingMetadata()) {
    // If initialized hasn't finished yet, no changes can be uploaded. In this
    // case, writes will fail silently. In practice, this shouldn't happen,
    // since the feature can only be considered enabled after the enabled
    // setting was loaded from the `store_`.
    return;
  }
  std::unique_ptr<syncer::EntityData> entity_data = CreateEntityData(specifics);
  const std::string storage_key = GetStorageKey(*entity_data);
  // Update the cache.
  settings_.insert_or_assign(storage_key, specifics);
  // Commit the write.
  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  change_processor()->Put(storage_key, std::move(entity_data),
                          metadata_change_list.get());
  // Update the `store_`'s data and metadata.
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(storage_key, specifics.SerializeAsString());
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&PlusAddressSettingSyncBridge::ReportErrorIfSet,
                     weak_factory_.GetWeakPtr()));
}

std::unique_ptr<syncer::MetadataChangeList>
PlusAddressSettingSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError>
PlusAddressSettingSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // Since the local storage is cleared when the data type is disabled in
  // `ApplyDisableSyncChanges()`, no local data exists during
  // `MergeFullSyncData()`.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<syncer::ModelError>
PlusAddressSettingSyncBridge::ApplyIncrementalSyncChanges(
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
        const sync_pb::PlusAddressSettingSpecifics& specifics =
            change->data().specifics.plus_address_setting();
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
      base::BindOnce(&PlusAddressSettingSyncBridge::ReportErrorIfSet,
                     weak_factory_.GetWeakPtr()));
  return std::nullopt;
}

void PlusAddressSettingSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  store_->DeleteAllDataAndMetadata(
      base::BindOnce(&PlusAddressSettingSyncBridge::ReportErrorIfSet,
                     weak_factory_.GetWeakPtr()));
  settings_.clear();
}

std::unique_ptr<syncer::DataBatch>
PlusAddressSettingSyncBridge::GetDataForCommit(StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& key : storage_keys) {
    if (auto setting = GetSetting(key)) {
      batch->Put(key, CreateEntityData(*setting));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
PlusAddressSettingSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [name, specifics] : settings_) {
    batch->Put(name, CreateEntityData(specifics));
  }
  return batch;
}

bool PlusAddressSettingSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  CHECK(entity_data.specifics.has_plus_address_setting());
  const sync_pb::PlusAddressSettingSpecifics& specifics =
      entity_data.specifics.plus_address_setting();
  return specifics.has_name() && !specifics.name().empty();
}

std::string PlusAddressSettingSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string PlusAddressSettingSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.plus_address_setting().name();
}

void PlusAddressSettingSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RETURN_IF_ERROR(error);
  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(base::BindOnce(
      &PlusAddressSettingSyncBridge::StartSyncingWithDataAndMetadata,
      weak_factory_.GetWeakPtr()));
}

void PlusAddressSettingSyncBridge::StartSyncingWithDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> data,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RETURN_IF_ERROR(error);
  // Initialize the `settings_` with the `data`.
  std::vector<std::pair<std::string, sync_pb::PlusAddressSettingSpecifics>>
      processed_entries;
  for (const syncer::DataTypeStore::Record& record : *data) {
    sync_pb::PlusAddressSettingSpecifics specifics;
    if (!specifics.ParseFromString(record.value)) {
      change_processor()->ReportError({FROM_HERE, "Couldn't parse specifics"});
      return;
    }
    processed_entries.emplace_back(record.id, std::move(specifics));
  }
  settings_ =
      base::MakeFlatMap<std::string, sync_pb::PlusAddressSettingSpecifics>(
          std::move(processed_entries));
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void PlusAddressSettingSyncBridge::ReportErrorIfSet(
    const std::optional<syncer::ModelError>& error) {
  RETURN_IF_ERROR(error);
}

}  // namespace plus_addresses
