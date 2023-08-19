// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_credential_sync_bridge.h"

#include <utility>

#include "base/check.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
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
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
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
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(web_data_backend) {
  CHECK(web_data_backend_);
  CHECK(GetAutofillTable());
  LoadMetadata();
}

AutofillWalletCredentialSyncBridge::~AutofillWalletCredentialSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletCredentialSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::AUTOFILL_WALLET_CREDENTIAL,
      base::BindRepeating(&syncer::ModelTypeChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

absl::optional<syncer::ModelError>
AutofillWalletCredentialSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

absl::optional<syncer::ModelError>
AutofillWalletCredentialSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

void AutofillWalletCredentialSyncBridge::GetData(StorageKeyList storage_keys,
                                                 DataCallback callback) {
  NOTIMPLEMENTED();
}

void AutofillWalletCredentialSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  NOTIMPLEMENTED();
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
  // Storage key and client tag are equivalent for this ModelType.
  return GetClientTag(entity_data);
}

void AutofillWalletCredentialSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  NOTIMPLEMENTED();
}

bool AutofillWalletCredentialSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.has_autofill_wallet_credential() &&
         !entity_data.specifics.autofill_wallet_credential()
              .instrument_id()
              .empty() &&
         !entity_data.specifics.autofill_wallet_credential().cvc().empty() &&
         entity_data.specifics.autofill_wallet_credential()
             .has_last_updated_time_unix_epoch_millis() &&
         entity_data.specifics.autofill_wallet_credential()
                 .last_updated_time_unix_epoch_millis() != 0;
}

AutofillTable* AutofillWalletCredentialSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

void AutofillWalletCredentialSyncBridge::LoadMetadata() {
  CHECK(web_data_backend_->GetDatabase()) << "Failed to get database.";
  CHECK(GetAutofillTable()) << "Failed to load Autofill table.";

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetAutofillTable()->GetAllSyncMetadata(
          syncer::AUTOFILL_WALLET_CREDENTIAL, batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading Autofill Wallet Credential data from WebDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

}  // namespace autofill
