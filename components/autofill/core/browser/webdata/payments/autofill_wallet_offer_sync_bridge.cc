// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/autofill_wallet_offer_sync_bridge.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"

namespace autofill {

namespace {

// Address to this variable used as the user data key.
static int kAutofillWalletOfferSyncBridgeUserDataKey = 0;

std::string GetClientTagFromSpecifics(
    const sync_pb::AutofillOfferSpecifics& specifics) {
  return syncer::GetUnhashedClientTagFromAutofillOfferSpecifics(specifics);
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
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::AUTOFILL_WALLET_OFFER,
              /*dump_stack=*/base::DoNothing()),
          web_data_backend));
}

// static
syncer::DataTypeSyncBridge* AutofillWalletOfferSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletOfferSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillWalletOfferSyncBridgeUserDataKey));
}

AutofillWalletOfferSyncBridge::AutofillWalletOfferSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : DataTypeSyncBridge(std::move(change_processor)),
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
      GetSyncMetadataStore(), syncer::AUTOFILL_WALLET_OFFER,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError>
AutofillWalletOfferSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MergeRemoteData(std::move(entity_data));
  return std::nullopt;
}

std::optional<syncer::ModelError>
AutofillWalletOfferSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // This bridge does not support incremental updates, so whenever this is
  // called, the change list should be empty.
  DCHECK(entity_data.empty()) << "Received an unsupported incremental update.";
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
AutofillWalletOfferSyncBridge::GetDataForCommit(StorageKeyList storage_keys) {
  // This data type is never synced "up" so this doesn't need to be implemented.
  NOTREACHED();
}

std::unique_ptr<syncer::DataBatch>
AutofillWalletOfferSyncBridge::GetAllDataForDebugging() {
  return GetAllDataImpl();
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

void AutofillWalletOfferSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  // Sync for this datatype is disabled so we want to delete the payments data.
  MergeRemoteData(syncer::EntityChangeList());
}

std::unique_ptr<syncer::DataBatch>
AutofillWalletOfferSyncBridge::GetAllDataImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<AutofillOfferData>> offers;
  if (!GetAutofillTable()->GetAutofillOffers(&offers)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load offer data from table."});
    return nullptr;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::unique_ptr<AutofillOfferData>& offer : offers) {
    auto entity_data = std::make_unique<syncer::EntityData>();
    sync_pb::AutofillOfferSpecifics* offer_specifics =
        entity_data->specifics.mutable_autofill_offer();
    SetAutofillOfferSpecificsFromOfferData(*offer, offer_specifics);

    entity_data->name =
        "Offer " +
        base::Base64Encode(GetClientTagFromSpecifics(*offer_specifics));

    batch->Put(GetStorageKeyFromSpecifics(*offer_specifics),
               std::move(entity_data));
  }
  return batch;
}

void AutofillWalletOfferSyncBridge::MergeRemoteData(
    const syncer::EntityChangeList& entity_data) {
  std::vector<AutofillOfferData> offer_data;
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    DCHECK(change->data().specifics.has_autofill_offer());
    const sync_pb::AutofillOfferSpecifics specifics =
        change->data().specifics.autofill_offer();
    bool offer_valid = IsOfferSpecificsValid(specifics);
    if (offer_valid) {
      offer_data.push_back(AutofillOfferDataFromOfferSpecifics(specifics));
    }
    autofill_metrics::LogSyncedOfferDataBeingValid(offer_valid);
  }

  PaymentsAutofillTable* table = GetAutofillTable();

  // Only do a write operation if there is any difference between server data
  // and local data.
  std::vector<std::unique_ptr<AutofillOfferData>> existing_offers;
  table->GetAutofillOffers(&existing_offers);

  bool offer_data_changed = AreAnyItemsDifferent(existing_offers, offer_data);
  if (offer_data_changed) {
    table->SetAutofillOffers(offer_data);
  }

  // Commit the transaction to make sure the data and the metadata with the
  // new progress marker is written down (especially on Android where we
  // cannot rely on committing transactions on shutdown). We need to commit
  // even if the wallet data has not changed because the data type state incl.
  // the progress marker always changes.
  web_data_backend_->CommitChanges();

  if (offer_data_changed) {
    web_data_backend_->NotifyOnAutofillChangedBySync(
        syncer::AUTOFILL_WALLET_OFFER);
  }
}

PaymentsAutofillTable* AutofillWalletOfferSyncBridge::GetAutofillTable() {
  return PaymentsAutofillTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

AutofillSyncMetadataTable*
AutofillWalletOfferSyncBridge::GetSyncMetadataStore() {
  return AutofillSyncMetadataTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

void AutofillWalletOfferSyncBridge::LoadAutofillOfferMetadata() {
  if (!web_data_backend_->GetDatabase() || !GetAutofillTable() ||
      !GetSyncMetadataStore()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load Autofill table."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetSyncMetadataStore()->GetAllSyncMetadata(syncer::AUTOFILL_WALLET_OFFER,
                                                  batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading autofill offer metadata from WebDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

}  // namespace autofill
