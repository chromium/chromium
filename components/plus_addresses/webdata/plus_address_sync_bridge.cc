// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_sync_bridge.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/webdata/plus_address_sync_util.h"
#include "components/plus_addresses/webdata/plus_address_table.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_backend.h"
#include "sql/transaction.h"

namespace plus_addresses {

PlusAddressSyncBridge::PlusAddressSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    scoped_refptr<WebDatabaseBackend> db_backend,
    DataChangedBySyncCallback notify_data_changed_by_sync)
    : DataTypeSyncBridge(std::move(change_processor)),
      db_backend_(std::move(db_backend)),
      notify_data_changed_by_sync_(std::move(notify_data_changed_by_sync)) {
  CHECK(db_backend_);
  // Initializing the database from disk can fail.
  if (!db_backend_->database()) {
    DataTypeSyncBridge::change_processor()->ReportError(
        {FROM_HERE, "Failed to initialize database."});
    return;
  }
  CHECK(GetPlusAddressTable());
  // Load metadata and start syncing.
  auto metadata = std::make_unique<syncer::MetadataBatch>();
  if (!GetPlusAddressTable()->GetAllSyncMetadata(syncer::PLUS_ADDRESS,
                                                 *metadata)) {
    DataTypeSyncBridge::change_processor()->ReportError(
        {FROM_HERE, "Failed to read PLUS_ADDRESS metadata."});
    return;
  }
  DataTypeSyncBridge::change_processor()->ModelReadyToSync(std::move(metadata));
}

PlusAddressSyncBridge::~PlusAddressSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
PlusAddressSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError> PlusAddressSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // Since PLUS_ADDRESS is read-only, merging local and sync data is the same as
  // applying changes from sync locally.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<syncer::ModelError>
PlusAddressSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  sql::Transaction transaction(db_backend_->database()->GetSQLConnection());
  if (!transaction.Begin()) {
    return syncer::ModelError(FROM_HERE, "Failed to begin transaction.");
  }

  std::vector<PlusAddressDataChange> profile_changes;
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        std::optional<PlusProfile> existing_profile =
            GetPlusAddressTable()->GetPlusProfileForId(change->storage_key());
        PlusProfile profile = PlusProfileFromEntityData(change->data());
        if (!GetPlusAddressTable()->AddOrUpdatePlusProfile(profile)) {
          return syncer::ModelError(
              FROM_HERE, "Failed to add/update profile in database.");
        }
        // When a plus address entry is updated, `profile_changes` will contain
        // both a REMOVE and ADD change for the old and new profiles
        // respectively.
        if (existing_profile) {
          profile_changes.emplace_back(PlusAddressDataChange::Type::kRemove,
                                       std::move(*existing_profile));
        }
        profile_changes.emplace_back(PlusAddressDataChange::Type::kAdd,
                                     std::move(profile));
        break;
      }
      case syncer::EntityChange::ACTION_DELETE: {
        std::optional<PlusProfile> profile =
            GetPlusAddressTable()->GetPlusProfileForId(change->storage_key());
        if (!GetPlusAddressTable()->RemovePlusProfile(change->storage_key())) {
          return syncer::ModelError(FROM_HERE,
                                    "Failed to remove profile in database.");
        }
        if (profile) {
          profile_changes.emplace_back(PlusAddressDataChange::Type::kRemove,
                                       std::move(*profile));
        }
        break;
      }
    }
  }

  if (auto error = TransferMetadataChanges(std::move(metadata_change_list))) {
    return error;
  }

  if (!transaction.Commit()) {
    return syncer::ModelError(FROM_HERE, "Failed to commit transaction.");
  }
  notify_data_changed_by_sync_.Run(std::move(profile_changes));
  return std::nullopt;
}

void PlusAddressSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  sql::Transaction transaction(db_backend_->database()->GetSQLConnection());
  if (!transaction.Begin()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to begin transaction."});
  }

  std::vector<PlusAddressDataChange> profile_changes;
  for (PlusProfile& profile : GetPlusAddressTable()->GetPlusProfiles()) {
    profile_changes.emplace_back(PlusAddressDataChange::Type::kRemove,
                                 std::move(profile));
  }

  if (!GetPlusAddressTable()->ClearPlusProfiles()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to remove profiles from database."});
    return;
  }
  // `TransferMetadataChanges()` returns an optional<ModelError>.
  if (TransferMetadataChanges(std::move(delete_metadata_change_list))) {
    // The error was already reported to the change processor.
    return;
  }

  if (!transaction.Commit()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to commit transaction."});
    return;
  }
  notify_data_changed_by_sync_.Run(std::move(profile_changes));
}

std::unique_ptr<syncer::DataBatch> PlusAddressSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  // PLUS_ADDRESS is read-only, so `GetDataForCommit()` is not needed.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

std::unique_ptr<syncer::DataBatch>
PlusAddressSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const PlusProfile& profile : GetPlusAddressTable()->GetPlusProfiles()) {
    auto entity = std::make_unique<syncer::EntityData>(
        EntityDataFromPlusProfile(profile));
    std::string storage_key = GetStorageKey(*entity);
    batch->Put(storage_key, std::move(entity));
  }
  return batch;
}

bool PlusAddressSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  CHECK(entity_data.specifics.has_plus_address());
  const sync_pb::PlusAddressSpecifics& plus_address =
      entity_data.specifics.plus_address();
  if (!plus_address.has_profile_id()) {
    return false;
  }
  return affiliations::FacetURI::FromPotentiallyInvalidSpec(
             plus_address.facet())
      .is_valid();
}

std::string PlusAddressSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string PlusAddressSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.plus_address().profile_id();
}

PlusAddressTable* PlusAddressSyncBridge::GetPlusAddressTable() {
  return PlusAddressTable::FromWebDatabase(db_backend_->database());
}

std::optional<syncer::ModelError>
PlusAddressSyncBridge::TransferMetadataChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list) {
  syncer::SyncMetadataStoreChangeList sync_metadata_store_change_list(
      GetPlusAddressTable(), syncer::PLUS_ADDRESS,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
  static_cast<syncer::InMemoryMetadataChangeList*>(metadata_change_list.get())
      ->TransferChangesTo(&sync_metadata_store_change_list);
  return change_processor()->GetError();
}

}  // namespace plus_addresses
