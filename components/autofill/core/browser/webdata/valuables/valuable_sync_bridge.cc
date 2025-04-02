// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuable_sync_bridge.h"

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/features.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/webdata/common/web_database.h"

namespace autofill {
namespace {

// The address of this variable is used as the user data key.
static const int kAutofillValuableSyncBridgeUserDataKey = 0;
}  // namespace

ValuableSyncBridge::ValuableSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    AutofillWebDataBackend* backend)
    : DataTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(backend) {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetValuablesTable()) {
    DataTypeSyncBridge::change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }
  LoadMetadata();
}

ValuableSyncBridge::~ValuableSyncBridge() = default;

// static
void ValuableSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillValuableSyncBridgeUserDataKey,
      std::make_unique<ValuableSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::AUTOFILL_VALUABLE,
              /*dump_stack=*/base::DoNothing()),
          web_data_backend));
}

// static
syncer::DataTypeSyncBridge* ValuableSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<ValuableSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillValuableSyncBridgeUserDataKey));
}

bool ValuableSyncBridge::SupportsIncrementalUpdates() const {
  // This type does not support incremental updates server side.
  return false;
}

AutofillSyncMetadataTable* ValuableSyncBridge::GetSyncMetadataStore() {
  return AutofillSyncMetadataTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

std::unique_ptr<syncer::MetadataChangeList>
ValuableSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetSyncMetadataStore(), syncer::AUTOFILL_VALUABLE,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError> ValuableSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  auto transaction = web_data_backend_->GetDatabase()->AcquireTransaction();

  // Remove all stored loyalty cards and replace them with new cards.
  if (!GetValuablesTable()->ClearLoyaltyCards()) {
    return syncer::ModelError(FROM_HERE,
                              "Failed to delete loyalty cards from table.");
  }
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD: {
        DCHECK(change->data().specifics.has_autofill_valuable());
        // Deserialize the LoyaltyCardSpecifics and add them in the DB.
        std::optional<LoyaltyCard> remote =
            CreateAutofillLoyaltyCardFromSpecifics(
                change->data().specifics.autofill_valuable());
        // Since the specifics are guaranteed to be valid by
        // `IsEntityDataValid()`, the conversion will succeed.
        DCHECK(remote);
        if (!GetValuablesTable()->AddOrUpdateLoyaltyCard(std::move(*remote))) {
          return syncer::ModelError(FROM_HERE,
                                    "Failed to add loyalty card to the table.");
        }
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
      case syncer::EntityChange::ACTION_UPDATE: {
        // LoyaltyCards sync does not support incremental updates server side.
        return syncer::ModelError(FROM_HERE,
                                  "Received unsupported action type.");
      }
    }
  }

  // Commits changes through CommitChanges(...) or through the scoped
  // sql::Transaction `transaction` depending on the
  // 'SqlScopedTransactionWebDatabase' Finch experiment.
  web_data_backend_->CommitChanges();
  if (transaction) {
    transaction->Commit();
  }
  web_data_backend_->NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE);
  return std::nullopt;
}

std::optional<syncer::ModelError>
ValuableSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  // This bridge does not support incremental updates, so whenever this is
  // called, the change list should be empty.
  CHECK(entity_changes.empty())
      << "Received an unsupported incremental update.";
  return std::nullopt;
}

std::unique_ptr<syncer::MutableDataBatch> ValuableSyncBridge::GetData() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const LoyaltyCard& card : GetValuablesTable()->GetLoyaltyCards()) {
    const std::string& id = card.id().value();
    batch->Put(id, CreateEntityDataFromLoyaltyCard(card));
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch> ValuableSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  // This type never commits to the server.
  NOTREACHED();
}

std::unique_ptr<syncer::DataBatch>
ValuableSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetData();
}

bool ValuableSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK(entity_data.specifics.has_autofill_valuable());
  return AreAutofillLoyaltyCardSpecificsValid(
      entity_data.specifics.autofill_valuable());
}

std::string ValuableSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string ValuableSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(IsEntityDataValid(entity_data));
  return entity_data.specifics.autofill_valuable().id();
}

void ValuableSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  auto transaction = web_data_backend_->GetDatabase()->AcquireTransaction();

  if (!GetValuablesTable()->ClearLoyaltyCards()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to delete loyalty cards from table."});
    return;
  }

  // Commits changes through CommitChanges(...) or through the scoped
  // sql::Transaction `transaction` depending on the
  // 'SqlScopedTransactionWebDatabase' Finch experiment.
  web_data_backend_->CommitChanges();
  if (transaction) {
    transaction->Commit();
  }

  // False positives can occur here if there were no loyalty cards to begin
  // with.
  web_data_backend_->NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE);
}

sync_pb::EntitySpecifics
ValuableSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  sync_pb::AutofillValuableSpecifics trimmed_autofill_valuable_specifics =
      TrimAutofillValuableSpecificsDataForCaching(
          entity_specifics.autofill_valuable());

  // If all fields are cleared from the valuable specifics, return a fresh
  // EntitySpecifics to avoid caching a few residual bytes.
  if (trimmed_autofill_valuable_specifics.ByteSizeLong() == 0u) {
    return sync_pb::EntitySpecifics();
  }

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  *trimmed_entity_specifics.mutable_autofill_valuable() =
      std::move(trimmed_autofill_valuable_specifics);

  return trimmed_entity_specifics;
}

// TODO(crbug.com/40253286): Consider moving this logic to processor.
bool ValuableSyncBridge::SyncMetadataCacheContainsSupportedFields(
    const syncer::EntityMetadataMap& metadata_map) const {
  for (const auto& metadata_entry : metadata_map) {
    // Serialize the cached specifics and parse them back to a proto. Any fields
    // that were cached as unknown and are known in the current browser version
    // should be parsed correctly.
    std::string serialized_specifics;
    metadata_entry.second->possibly_trimmed_base_specifics().SerializeToString(
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

void ValuableSyncBridge::LoadMetadata() {
  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetSyncMetadataStore()->GetAllSyncMetadata(syncer::AUTOFILL_VALUABLE,
                                                  batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading AUTOFILL_VALUABLE metadata from WebDatabase."});
    return;
  } else if (SyncMetadataCacheContainsSupportedFields(
                 batch->GetAllMetadata())) {
    // Caching entity specifics is meant to preserve fields not supported in a
    // given browser version during commits to the server. If the cache
    // contains supported fields, this means that the browser was updated and
    // we should force the initial sync flow to propagate the cached data into
    // the local model.
    GetSyncMetadataStore()->DeleteAllSyncMetadata(
        syncer::DataType::AUTOFILL_VALUABLE);

    batch = std::make_unique<syncer::MetadataBatch>();
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

ValuablesTable* ValuableSyncBridge::GetValuablesTable() {
  return ValuablesTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

}  // namespace autofill
