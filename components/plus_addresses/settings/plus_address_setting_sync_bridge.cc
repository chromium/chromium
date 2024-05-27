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
#include "components/sync/model/model_type_store.h"

namespace plus_addresses {

// Macro to simplify reporting errors raised by ModelTypeStore operations.
#define RETURN_IF_ERROR(error)               \
  if (error) {                               \
    change_processor()->ReportError(*error); \
    return;                                  \
  }

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
  NOTIMPLEMENTED();
  return nullptr;
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
  NOTIMPLEMENTED();
}

void PlusAddressSettingSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string PlusAddressSettingSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

std::string PlusAddressSettingSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

void PlusAddressSettingSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RETURN_IF_ERROR(error);
  store_ = std::move(store);
  store_->ReadAllMetadata(
      base::BindOnce(&PlusAddressSettingSyncBridge::StartSyncingWithMetadata,
                     weak_factory_.GetWeakPtr()));
}

void PlusAddressSettingSyncBridge::StartSyncingWithMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RETURN_IF_ERROR(error);
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

}  // namespace plus_addresses
