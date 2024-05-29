// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/plus_address_setting_sync_bridge.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/plus_address_setting_specifics.pb.h"

namespace plus_addresses {

// Macro to simplify reporting errors raised by ModelTypeStore operations.
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
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory store_factory)
    : ModelTypeSyncBridge(std::move(change_processor)) {
  std::move(store_factory)
      .Run(syncer::PLUS_ADDRESS_SETTING,
           base::BindOnce(&PlusAddressSettingSyncBridge::OnStoreCreated,
                          weak_factory_.GetWeakPtr()));
}

PlusAddressSettingSyncBridge::~PlusAddressSettingSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
PlusAddressSettingSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError>
PlusAddressSettingSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<syncer::ModelError>
PlusAddressSettingSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

void PlusAddressSettingSyncBridge::GetData(StorageKeyList storage_keys,
                                           DataCallback callback) {
  // PLUS_ADDRESS_SETTING is read-only, so `GetData()` is not needed.
  NOTREACHED();
}

void PlusAddressSettingSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [name, specifics] : settings_) {
    batch->Put(name, CreateEntityData(specifics));
  }
  std::move(callback).Run(std::move(batch));
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
    std::unique_ptr<syncer::ModelTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RETURN_IF_ERROR(error);
  store_ = std::move(store);
  store_->ReadAllData(
      base::BindOnce(&PlusAddressSettingSyncBridge::OnReadAllData,
                     weak_factory_.GetWeakPtr()));
}

void PlusAddressSettingSyncBridge::OnReadAllData(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RETURN_IF_ERROR(error);
  store_->ReadAllMetadata(base::BindOnce(
      &PlusAddressSettingSyncBridge::StartSyncingWithDataAndMetadata,
      weak_factory_.GetWeakPtr(), std::move(data)));
}

void PlusAddressSettingSyncBridge::StartSyncingWithDataAndMetadata(
    std::unique_ptr<syncer::ModelTypeStore::RecordList> data,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RETURN_IF_ERROR(error);
  // Initialize the `settings_` with the `data`.
  std::vector<std::pair<std::string, sync_pb::PlusAddressSettingSpecifics>>
      processed_entries;
  for (const syncer::ModelTypeStore::Record& record : *data) {
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

}  // namespace plus_addresses
