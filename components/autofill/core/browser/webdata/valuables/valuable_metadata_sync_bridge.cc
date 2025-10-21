// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuable_metadata_sync_bridge.h"

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/autofill_valuable_metadata_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/webdata/common/web_database.h"

namespace autofill {
namespace {
// The address of this variable is used as the user data key.
static const int kAutofillValuableMetadataSyncBridgeUserDataKey = 0;

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

  // TODO(crbug.com/40253286): Implement loading initial data.
}

ValuableMetadataSyncBridge::~ValuableMetadataSyncBridge() = default;

// static
void ValuableMetadataSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
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
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillValuableMetadataSyncBridgeUserDataKey));
}

AutofillSyncMetadataTable* ValuableMetadataSyncBridge::GetSyncMetadataStore() {
  return AutofillSyncMetadataTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

std::unique_ptr<syncer::MetadataChangeList>
ValuableMetadataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetSyncMetadataStore(), syncer::AUTOFILL_VALUABLE_METADATA,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError> ValuableMetadataSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40253286): Upload any initial local data.

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
  // TODO(crbug.com/436551488): Implement actual data retrieval.
  return batch;
}

std::unique_ptr<syncer::DataBatch> ValuableMetadataSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/436551488): Implement.
  return nullptr;
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
  // TODO(crbug.com/436551488): Implement.
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
  // TODO(crbug.com/436551488): Implement.
  return std::nullopt;
}

ValuablesTable* ValuableMetadataSyncBridge::GetValuablesTable() {
  return ValuablesTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

}  // namespace autofill
