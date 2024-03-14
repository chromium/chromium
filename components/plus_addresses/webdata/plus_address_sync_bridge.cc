// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_sync_bridge.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/webdata/plus_address_sync_util.h"
#include "components/plus_addresses/webdata/plus_address_table.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/webdata/common/web_database_backend.h"

namespace plus_addresses {

PlusAddressSyncBridge::PlusAddressSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    scoped_refptr<WebDatabaseBackend> db_backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      db_backend_(std::move(db_backend)) {
  CHECK(db_backend_);
  // Initializing the database from disk can fail.
  if (!db_backend_->database()) {
    ModelTypeSyncBridge::change_processor()->ReportError(
        {FROM_HERE, "Failed to initialize database."});
    return;
  }
  CHECK(GetPlusAddressTable());
  // Load metadata and start syncing.
  auto metadata = std::make_unique<syncer::MetadataBatch>();
  if (!GetPlusAddressTable()->GetAllSyncMetadata(syncer::PLUS_ADDRESS,
                                                 *metadata)) {
    ModelTypeSyncBridge::change_processor()->ReportError(
        {FROM_HERE, "Failed to read PLUS_ADDRESS metadata."});
    return;
  }
  ModelTypeSyncBridge::change_processor()->ModelReadyToSync(
      std::move(metadata));
}

PlusAddressSyncBridge::~PlusAddressSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
PlusAddressSyncBridge::CreateMetadataChangeList() {
  // `PlusAddressTable` implements `syncer::SyncMetadataStore`. Before any
  // changes written to the metadata change list are persisted on disk, the
  // pending database transaction needs to be committed.
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetPlusAddressTable(), syncer::PLUS_ADDRESS,
      base::BindRepeating(&syncer::ModelTypeChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError> PlusAddressSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<syncer::ModelError>
PlusAddressSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

void PlusAddressSyncBridge::GetData(StorageKeyList storage_keys,
                                    DataCallback callback) {
  // PLUS_ADDRESS is ready-only, so `GetData()` is not needed.
  NOTREACHED();
}

void PlusAddressSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const PlusProfile& profile : GetPlusAddressTable()->GetPlusProfiles()) {
    auto entity = std::make_unique<syncer::EntityData>(
        EntityDataFromPlusProfile(profile));
    std::string storage_key = GetStorageKey(*entity);
    batch->Put(storage_key, std::move(entity));
  }
  std::move(callback).Run(std::move(batch));
}

bool PlusAddressSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  CHECK(entity_data.specifics.has_plus_address());
  return entity_data.specifics.plus_address().has_profile_id();
}

std::string PlusAddressSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string PlusAddressSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return base::NumberToString(
      entity_data.specifics.plus_address().profile_id());
}

PlusAddressTable* PlusAddressSyncBridge::GetPlusAddressTable() {
  return PlusAddressTable::FromWebDatabase(db_backend_->database());
}

}  // namespace plus_addresses
