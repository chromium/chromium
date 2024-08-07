// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/autofill_wallet_credential_sync_bridge.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/autofill_wallet_credential_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {

namespace {

// Address to this variable used as the user data key.
const char kAutofillWalletCredentialSyncBridgeUserDataKey[] =
    "AutofillWalletCredentialSyncBridgeUserDataKey";

}  // namespace

// static
void AutofillWalletCredentialSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillWalletCredentialSyncBridgeUserDataKey,
      std::make_unique<AutofillWalletCredentialSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::AUTOFILL_WALLET_CREDENTIAL,
              /*dump_stack=*/base::RepeatingClosure()),
          web_data_backend));
}

// static
AutofillWalletCredentialSyncBridge*
AutofillWalletCredentialSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletCredentialSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillWalletCredentialSyncBridgeUserDataKey));
}

AutofillWalletCredentialSyncBridge::AutofillWalletCredentialSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : DataTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(web_data_backend) {
  // Report an error for the wallet credential sync data type if the web
  // database isn't loaded.
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable()) {
    DataTypeSyncBridge::change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }
  scoped_observation_.Observe(web_data_backend_.get());
  LoadMetadata();
}

AutofillWalletCredentialSyncBridge::~AutofillWalletCredentialSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletCredentialSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetSyncMetadataStore(), syncer::AUTOFILL_WALLET_CREDENTIAL,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError>
AutofillWalletCredentialSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // When this data type is disabled, all the local data gets deleted, so there
  // is never anything to merge.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<syncer::ModelError>
AutofillWalletCredentialSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PaymentsAutofillTable* table = GetAutofillTable();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    sync_pb::AutofillWalletCredentialSpecifics wallet_credential_specifics =
        change->data().specifics.autofill_wallet_credential();
    switch (change->type()) {
      case syncer::EntityChange::ACTION_DELETE:
        int64_t storage_key;
        if (!table || change->storage_key().empty() ||
            !base::StringToInt64(change->storage_key(), &storage_key) ||
            !table->RemoveServerCvc(storage_key)) {
          return syncer::ModelError(
              FROM_HERE,
              "Failed to delete the Wallet credential data from the table");
        }
        break;
      // TODO(crbug.com/40926464): Merge the Add and Update APIs for
      // PaymentsAutofillTable.
      case syncer::EntityChange::ACTION_ADD:
        if (!table ||
            !table->AddServerCvc(
                AutofillWalletCvcStructDataFromWalletCredentialSpecifics(
                    wallet_credential_specifics))) {
          return syncer::ModelError(
              FROM_HERE,
              "Failed to add the Wallet credential data to the table");
        }
        break;
      case syncer::EntityChange::ACTION_UPDATE:
        if (!table ||
            !table->UpdateServerCvc(
                AutofillWalletCvcStructDataFromWalletCredentialSpecifics(
                    wallet_credential_specifics))) {
          return syncer::ModelError(
              FROM_HERE,
              "Failed to update the Wallet credential data to the table");
        }
        break;
    }
  }
  // Commit the transaction to make sure the data and the metadata with the
  // new progress marker is written down.
  web_data_backend_->CommitChanges();

  // There can be cases where `ApplyIncrementalSyncChanges` is called with
  // empty `entity_data`, where only the metadata needs to be updated. This
  // check helps check that and prevent any false positives.
  if (!entity_data.empty()) {
    web_data_backend_->NotifyOnAutofillChangedBySync(
        syncer::AUTOFILL_WALLET_CREDENTIAL);
  }
  return change_processor()->GetError();
}

std::unique_ptr<syncer::DataBatch>
AutofillWalletCredentialSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ranges::sort(storage_keys);
  std::vector<std::unique_ptr<ServerCvc>> filtered_server_cvc_list;
  for (std::unique_ptr<ServerCvc>& server_cvc_from_list :
       GetAutofillTable()->GetAllServerCvcs()) {
    if (base::ranges::binary_search(
            storage_keys,
            base::NumberToString(server_cvc_from_list->instrument_id))) {
      filtered_server_cvc_list.push_back(std::move(server_cvc_from_list));
    }
  }
  return ConvertToDataBatch(filtered_server_cvc_list);
}

std::unique_ptr<syncer::DataBatch>
AutofillWalletCredentialSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::vector<std::unique_ptr<ServerCvc>>& server_cvc_list =
      GetAutofillTable()->GetAllServerCvcs();
  return ConvertToDataBatch(server_cvc_list);
}

std::string AutofillWalletCredentialSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  CHECK(IsEntityDataValid(entity_data));
  const sync_pb::AutofillWalletCredentialSpecifics&
      autofill_wallet_credential_data =
          entity_data.specifics.autofill_wallet_credential();

  return autofill_wallet_credential_data.instrument_id();
}

std::string AutofillWalletCredentialSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  // Storage key and client tag are equivalent for this DataType.
  return GetClientTag(entity_data);
}

void AutofillWalletCredentialSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PaymentsAutofillTable* table = GetAutofillTable();
  // Check if we have data to delete.
  if (table->GetAllServerCvcs().size() == 0) {
    return;
  }
  // For this data type, we want to delete all the data (not just the metadata)
  // when the type is disabled!
  // Note- This only clears the data from the local database and doesn't trigger
  // a `REMOVE` call to the Chrome Sync server.
  if (!table || !table->ClearServerCvcs()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to delete wallet credential data from the table."});
  }
  web_data_backend_->CommitChanges();
  web_data_backend_->NotifyOnAutofillChangedBySync(
      syncer::AUTOFILL_WALLET_CREDENTIAL);
}

bool AutofillWalletCredentialSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.has_autofill_wallet_credential() &&
         IsAutofillWalletCredentialDataSpecificsValid(
             entity_data.specifics.autofill_wallet_credential());
}

void AutofillWalletCredentialSyncBridge::ServerCvcChanged(
    const ServerCvcChange& change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ActOnLocalChange(change);
}

PaymentsAutofillTable* AutofillWalletCredentialSyncBridge::GetAutofillTable() {
  return PaymentsAutofillTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

AutofillSyncMetadataTable*
AutofillWalletCredentialSyncBridge::GetSyncMetadataStore() {
  return AutofillSyncMetadataTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

void AutofillWalletCredentialSyncBridge::ActOnLocalChange(
    const ServerCvcChange& change) {
  // If sync isn't ready yet (most likely because the data type is disabled),
  // ignore the change.
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  auto data = std::make_unique<syncer::EntityData>();

  std::string key_str = base::NumberToString(change.key());
  switch (change.type()) {
    case ServerCvcChange::ADD:
    case ServerCvcChange::UPDATE:
      data->name = base::NumberToString(change.data_model().instrument_id);
      *data->specifics.mutable_autofill_wallet_credential() =
          AutofillWalletCredentialSpecificsFromStructData(change.data_model());
      change_processor()->Put(std::move(key_str), std::move(data),
                              metadata_change_list.get());
      break;
    case ServerCvcChange::REMOVE:
      change_processor()->Delete(std::move(key_str),
                                 syncer::DeletionOrigin::Unspecified(),
                                 metadata_change_list.get());
      break;
  }
}

void AutofillWalletCredentialSyncBridge::LoadMetadata() {
  CHECK(web_data_backend_->GetDatabase()) << "Failed to get database.";
  CHECK(GetSyncMetadataStore()) << "Failed to load metadata table.";

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetSyncMetadataStore()->GetAllSyncMetadata(
          syncer::AUTOFILL_WALLET_CREDENTIAL, batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading Autofill Wallet Credential data from WebDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

std::unique_ptr<syncer::MutableDataBatch>
AutofillWalletCredentialSyncBridge::ConvertToDataBatch(
    const std::vector<std::unique_ptr<ServerCvc>>& server_cvc_list) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::unique_ptr<ServerCvc>& server_cvc_from_list :
       server_cvc_list) {
    auto entity_data = std::make_unique<syncer::EntityData>();
    sync_pb::AutofillWalletCredentialSpecifics wallet_credential_specifics =
        AutofillWalletCredentialSpecificsFromStructData(*server_cvc_from_list);
    *entity_data->specifics.mutable_autofill_wallet_credential() =
        wallet_credential_specifics;

    const std::string& storage_key = GetStorageKey(*entity_data);
    entity_data->name = storage_key;
    batch->Put(storage_key, std::move(entity_data));
  }
  return batch;
}

}  // namespace autofill
