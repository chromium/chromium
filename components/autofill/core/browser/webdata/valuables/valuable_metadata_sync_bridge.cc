// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuable_metadata_sync_bridge.h"

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/features.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/autofill_valuable_metadata_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/webdata/common/web_database.h"

namespace autofill {
namespace {
// The address of this variable is used as the user data key.
static const int kAutofillValuableMetadataSyncBridgeUserDataKey = 0;

// Returns whether the orphan metadata entry is old enough to be deleted.
bool IsOrphanValuableMetadataEntryDeletable(
    const EntityInstance::EntityMetadata& metadata) {
  static constexpr base::TimeDelta kDisusedEntityMetadataDeletionTimeDelta =
      base::Days(365);
  return metadata.use_date <
         base::Time::Now() - kDisusedEntityMetadataDeletionTimeDelta;
}

}  // namespace

ValuableMetadataSyncBridge::ValuableMetadataSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    AutofillWebDataBackend* backend)
    : DataTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(backend) {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase()) {
    DataTypeSyncBridge::change_processor()->ReportError(
        {FROM_HERE, syncer::ModelError::Type::
                        kAutofillValuableMetadataFailedToLoadDatabase});
    return;
  }

  CHECK(base::FeatureList::IsEnabled(syncer::kSyncAutofillValuableMetadata));
  LoadMetadata();
  DeleteOrphanMetadata();
  scoped_observation_.Observe(web_data_backend_.get());
}

ValuableMetadataSyncBridge::~ValuableMetadataSyncBridge() = default;

// static
void ValuableMetadataSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData().SetUserData(
      &kAutofillValuableMetadataSyncBridgeUserDataKey,
      std::make_unique<ValuableMetadataSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::AUTOFILL_VALUABLE_METADATA,
              /*dump_stack=*/base::DoNothing()),
          web_data_backend));
}

// static
syncer::DataTypeSyncBridge* ValuableMetadataSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<ValuableMetadataSyncBridge*>(
      web_data_service->GetDBUserData().GetUserData(
          &kAutofillValuableMetadataSyncBridgeUserDataKey));
}

AutofillSyncMetadataTable* ValuableMetadataSyncBridge::GetSyncMetadataStore() {
  return AutofillSyncMetadataTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

std::unique_ptr<syncer::MetadataChangeList>
ValuableMetadataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

void ValuableMetadataSyncBridge::UploadInitialLocalData(
    syncer::MetadataChangeList* metadata_change_list,
    const syncer::EntityChangeList& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::map<EntityInstance::EntityId, EntityInstance::EntityMetadata>
      stored_metadata = GetEntityTable()->GetSyncedMetadata();

  // First, make a copy of all local storage keys.
  std::set<EntityInstance::EntityId> local_keys_to_upload;
  for (const auto& [storage_key, metadata] : stored_metadata) {
    local_keys_to_upload.insert(storage_key);
  }

  // Strip |local_keys_to_upload| of the keys of data provided by the server.
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    DCHECK_EQ(change->type(), syncer::EntityChange::ACTION_ADD)
        << "Illegal change; can only be called during initial "
           "MergeFullSyncData()";
    local_keys_to_upload.erase(EntityInstance::EntityId(change->storage_key()));
  }
  // Upload the remaining storage keys
  for (const EntityInstance::EntityId& storage_key : local_keys_to_upload) {
    change_processor()->Put(
        *storage_key,
        CreateEntityDataFromEntityMetadata(stored_metadata[storage_key]),
        metadata_change_list);
  }
}

void ValuableMetadataSyncBridge::DeleteOrphanMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetEntityTable()) {
    // We have a problem with the database, not an issue, we clean up next time.
    return;
  }

  // Load up (metadata) ids for which data exists; we do not delete those.
  std::vector<EntityInstance> server_entities =
      GetEntityTable()->GetEntityInstances(
          EntityInstance::RecordType::kServerWallet);
  auto non_orphan_ids = base::MakeFlatSet<EntityInstance::EntityId>(
      server_entities, {}, &EntityInstance::guid);

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  std::unique_ptr<sql::Transaction> transaction =
      web_data_backend_->GetDatabase()->AcquireTransaction();
  int removed_count = 0;
  for (const auto& [storage_key, metadata] :
       GetEntityTable()->GetSyncedMetadata()) {
    // Identify storage keys of old orphans, remove them from the local storage
    // and the server.
    if (!non_orphan_ids.contains(storage_key) &&
        IsOrphanValuableMetadataEntryDeletable(metadata)) {
      if (GetEntityTable()->RemoveEntityMetadata(storage_key)) {
        change_processor()->Delete(
            *storage_key, syncer::DeletionOrigin::FromLocation(FROM_HERE),
            metadata_change_list.get());
        removed_count++;
      }
    }
  }

  base::UmaHistogramCounts100(
      "Autofill.ValuableMetadata.OrphanEntriesRemovedCount", removed_count);

  // Commits changes through CommitChanges(...) or through the scoped
  // sql::Transaction `transaction` depending on the
  // 'SqlScopedTransactionWebDatabase' Finch experiment.
  web_data_backend_->CommitChanges();
  if (transaction) {
    transaction->Commit();
  }
  // We do not need to NotifyOnAutofillChangedBySync() because this change is
  // invisible for the EntityDataManager - it does not change metadata for any
  // existing data.
}

std::optional<syncer::ModelError> ValuableMetadataSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First upload local entities that are not mentioned in `entity_data`.
  // Because Valuable Metadata is deleted when Sync (for this data type) is
  // turned off, there should usually not be any pre-existing local data here,
  // but it can happen in some corner cases such as when `ValuableSyncBridge`
  // manages to change metadata during the initial sync procedure (e.g. the
  // remote sync data was just downloaded, but first passed to the
  // AUTOFILL_VALUABLE bridge, with the side effect of creating
  // valuable metadata entries immediately before this function is invoked).
  UploadInitialLocalData(metadata_change_list.get(), entity_data);

  return MergeRemoteChanges(std::move(metadata_change_list),
                            std::move(entity_data));
}

std::optional<syncer::ModelError>
ValuableMetadataSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return MergeRemoteChanges(std::move(metadata_change_list),
                            std::move(entity_changes));
}

std::unique_ptr<syncer::MutableDataBatch>
ValuableMetadataSyncBridge::GetAllData() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [storage_key, metadata] :
       GetEntityTable()->GetSyncedMetadata()) {
    batch->Put(*storage_key, CreateEntityDataFromEntityMetadata(metadata));
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch> ValuableMetadataSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  absl::flat_hash_set<std::string> keys_set(storage_keys.begin(),
                                            storage_keys.end());
  std::unique_ptr<syncer::DataBatch> all_data = GetAllData();
  while (all_data->HasNext()) {
    syncer::KeyAndData item = all_data->Next();
    if (keys_set.contains(item.first)) {
      batch->Put(item.first, std::move(item.second));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
ValuableMetadataSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetAllData();
}

bool ValuableMetadataSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK(entity_data.specifics.has_autofill_valuable_metadata());
  const sync_pb::AutofillValuableMetadataSpecifics& autofill_valuable_metadata =
      entity_data.specifics.autofill_valuable_metadata();

  // Valuable metadata must contain a non-empty valuable_id.
  return !autofill_valuable_metadata.valuable_id().empty();
}

std::string ValuableMetadataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string ValuableMetadataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  CHECK(IsEntityDataValid(entity_data));
  return entity_data.specifics.autofill_valuable_metadata().valuable_id();
}

void ValuableMetadataSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  std::unique_ptr<sql::Transaction> transaction =
      web_data_backend_->GetDatabase()->AcquireTransaction();
  EntityTable* table = GetEntityTable();
  // When sync is disabled, the metadata should be cleared.
  for (const auto& [storage_key, metadata] :
       GetEntityTable()->GetSyncedMetadata()) {
    table->RemoveEntityMetadata(storage_key);
  }

  web_data_backend_->CommitChanges();
  if (transaction) {
    transaction->Commit();
  }
}

sync_pb::EntitySpecifics
ValuableMetadataSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  sync_pb::AutofillValuableMetadataSpecifics
      trimmed_autofill_valuable_metadata_specifics =
          TrimAutofillValuableMetadataSpecificsDataForCaching(
              entity_specifics.autofill_valuable_metadata());

  // If all fields are cleared from the valuable metadata specifics, return a
  // fresh EntitySpecifics to avoid caching a few residual bytes.
  if (trimmed_autofill_valuable_metadata_specifics.ByteSizeLong() == 0u) {
    return sync_pb::EntitySpecifics();
  }

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  *trimmed_entity_specifics.mutable_autofill_valuable_metadata() =
      std::move(trimmed_autofill_valuable_metadata_specifics);

  return trimmed_entity_specifics;
}

std::optional<syncer::ModelError>
ValuableMetadataSyncBridge::MergeRemoteChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<sql::Transaction> transaction =
      web_data_backend_->GetDatabase()->AcquireTransaction();
  EntityTable* table = GetEntityTable();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        const sync_pb::AutofillValuableMetadataSpecifics& specifics =
            change->data().specifics.autofill_valuable_metadata();
        EntityInstance::EntityMetadata remote =
            CreateValuableMetadataFromSpecifics(specifics);
        if (!table->AddOrUpdateEntityMetadata(remote)) {
          // TODO(crbug.com/436551488): Update to the correct error type.
          return syncer::ModelError(
              FROM_HERE, syncer::ModelError::Type::
                             kAutofillValuableMetadataFailedToLoadDatabase);
        }
        break;
      }
      case syncer::EntityChange::ACTION_DELETE: {
        // Similar to AutofillWalletMetadataSyncBridge, ignore remote deletions
        // to avoid delete-create ping pongs. The metadata will be deleted when
        // the valuable itself is deleted. A cleanup mechanism for orphan
        // metadata might be needed.
        break;
      }
    }
  }

  if (std::optional<syncer::ModelError> error =
          ApplyMetadataChanges(std::move(metadata_change_list))) {
    return error;
  }

  // Commits changes through CommitChanges(...) or through the scoped
  // sql::Transaction `transaction` depending on the
  // 'SqlScopedTransactionWebDatabase' Finch experiment.
  web_data_backend_->CommitChanges();
  if (transaction && !transaction->Commit()) {
    return syncer::ModelError(
        FROM_HERE,
        syncer::ModelError::Type::
            kAutofillValuableMetadataTransactionCommitFailedOnIncrementalSync);
  }

  web_data_backend_->NotifyOnAutofillChangedBySync(
      syncer::AUTOFILL_VALUABLE_METADATA);

  return std::nullopt;
}

void ValuableMetadataSyncBridge::ServerEntityInstanceMetadataChanged(
    const EntityInstanceMetadataChange& change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(base::FeatureList::IsEnabled(syncer::kSyncAutofillValuableMetadata));
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  switch (change.type()) {
    case EntityInstanceMetadataChange::ADD:
    case EntityInstanceMetadataChange::UPDATE:
      change_processor()->Put(
          *change.key(),
          CreateEntityDataFromEntityMetadata(change.data_model()),
          metadata_change_list.get());
      break;
    case EntityInstanceMetadataChange::REMOVE:
      change_processor()->Delete(
          *change.key(), syncer::DeletionOrigin::FromLocation(FROM_HERE),
          metadata_change_list.get());
      break;
    case EntityInstanceMetadataChange::HIDE_IN_AUTOFILL:
      NOTREACHED();
  }
}

std::optional<syncer::ModelError>
ValuableMetadataSyncBridge::ApplyMetadataChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list) {
  syncer::SyncMetadataStoreChangeList sync_metadata_store_change_list(
      GetSyncMetadataStore(), syncer::AUTOFILL_VALUABLE_METADATA,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
  metadata_change_list->TransferChangesTo(&sync_metadata_store_change_list);
  return change_processor()->GetError();
}

bool ValuableMetadataSyncBridge::SyncMetadataCacheContainsSupportedFields(
    const syncer::EntityMetadataMap& metadata_map) const {
  for (const auto& [_, metadata_entry] : metadata_map) {
    // Serialize the cached specifics and parse them back to a proto. Any fields
    // that were cached as unknown and are known in the current browser version
    // should be parsed correctly.
    std::string serialized_specifics;
    metadata_entry->possibly_trimmed_base_specifics().SerializeToString(
        &serialized_specifics);
    sync_pb::EntitySpecifics parsed_specifics;
    parsed_specifics.ParseFromString(serialized_specifics);

    // If `parsed_specifics` contain any supported fields, they would be cleared
    // by the trimming function.
    if (parsed_specifics.ByteSizeLong() !=
        TrimAllSupportedFieldsFromRemoteSpecifics(parsed_specifics)
            .ByteSizeLong()) {
      return true;
    }
  }

  return false;
}

void ValuableMetadataSyncBridge::LoadMetadata() {
  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetSyncMetadataStore()->GetAllSyncMetadata(
          syncer::AUTOFILL_VALUABLE_METADATA, batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE, syncer::ModelError::Type::
                        kAutofillValuableMetadataFailedToLoadMetadata});
    return;
  }

  if (SyncMetadataCacheContainsSupportedFields(batch->GetAllMetadata())) {
    // Caching entity specifics is meant to preserve fields not supported in a
    // given browser version during commits to the server. If the cache
    // contains supported fields, this means that the browser was updated and
    // we should force the initial sync flow to propagate the cached data into
    // the local model.
    GetSyncMetadataStore()->DeleteAllSyncMetadata(
        syncer::DataType::AUTOFILL_VALUABLE_METADATA);

    batch = std::make_unique<syncer::MetadataBatch>();
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

EntityTable* ValuableMetadataSyncBridge::GetEntityTable() {
  return EntityTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

const EntityTable* ValuableMetadataSyncBridge::GetEntityTable() const {
  return const_cast<const EntityTable*>(
      const_cast<ValuableMetadataSyncBridge*>(this)->GetEntityTable());
}

}  // namespace autofill
