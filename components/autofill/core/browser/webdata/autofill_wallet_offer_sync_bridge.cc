// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_offer_sync_bridge.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"

namespace autofill {

namespace {

// Address to this variable used as the user data key.
static int kAutofillWalletOfferSyncBridgeUserDataKey = 0;

std::string GetClientTagFromSpecifics(
    const sync_pb::AutofillOfferSpecifics& specifics) {
  return base::NumberToString(specifics.id());
}

std::string GetStorageKeyFromSpecifics(
    const sync_pb::AutofillOfferSpecifics& specifics) {
  // Use client tag as the storage key.
  return GetClientTagFromSpecifics(specifics);
}

}  // namespace

// static
void AutofillWalletOfferSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillWalletOfferSyncBridgeUserDataKey,
      std::make_unique<AutofillWalletOfferSyncBridge>(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL_WALLET_OFFER,
              /*dump_stack=*/base::RepeatingClosure()),
          web_data_backend));
}

// static
syncer::ModelTypeSyncBridge* AutofillWalletOfferSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletOfferSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillWalletOfferSyncBridgeUserDataKey));
}

AutofillWalletOfferSyncBridge::AutofillWalletOfferSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(web_data_backend) {
  DCHECK(web_data_backend_);

  LoadAutofillOfferMetadata();
}

AutofillWalletOfferSyncBridge::~AutofillWalletOfferSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletOfferSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::AUTOFILL_WALLET_OFFER);
}

base::Optional<syncer::ModelError> AutofillWalletOfferSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<syncer::ModelError>
AutofillWalletOfferSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

void AutofillWalletOfferSyncBridge::GetData(StorageKeyList storage_keys,
                                            DataCallback callback) {
  NOTIMPLEMENTED();
}

void AutofillWalletOfferSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string AutofillWalletOfferSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entity_data.specifics.has_autofill_offer());
  return GetClientTagFromSpecifics(entity_data.specifics.autofill_offer());
}

std::string AutofillWalletOfferSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entity_data.specifics.has_autofill_offer());
  return GetStorageKeyFromSpecifics(entity_data.specifics.autofill_offer());
}

bool AutofillWalletOfferSyncBridge::SupportsIncrementalUpdates() const {
  return false;
}

void AutofillWalletOfferSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  NOTIMPLEMENTED();
}

AutofillTable* AutofillWalletOfferSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

void AutofillWalletOfferSyncBridge::LoadAutofillOfferMetadata() {
  if (!web_data_backend_->GetDatabase() || !GetAutofillTable()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load Autofill table."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetAutofillTable()->GetAllSyncMetadata(syncer::AUTOFILL_WALLET_OFFER,
                                              batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading autofill offer metadata from WebDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

}  // namespace autofill
