// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_offer_sync_bridge.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
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
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL_WALLET_OFFER,
              /*dump_stack=*/base::DoNothing()),
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
      GetAutofillTable(), syncer::AUTOFILL_WALLET_OFFER,
      base::BindRepeating(&syncer::ModelTypeChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

absl::optional<syncer::ModelError>
AutofillWalletOfferSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MergeRemoteData(std::move(entity_data));
  return absl::nullopt;
}

absl::optional<syncer::ModelError>
AutofillWalletOfferSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // This bridge does not support incremental updates, so whenever this is
  // called, the change list should be empty.
  DCHECK(entity_data.empty()) << "Received an unsupported incremental update.";
  return absl::nullopt;
}

void AutofillWalletOfferSyncBridge::GetData(StorageKeyList storage_keys,
                                            DataCallback callback) {}

void AutofillWalletOfferSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  GetAllDataImpl(std::move(callback));
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

void AutofillWalletOfferSyncBridge::GetAllDataImpl(DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<AutofillOfferData>> offers;
  if (!GetAutofillTable()->GetAutofillOffers(&offers)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load offer data from table."});
    return;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::unique_ptr<AutofillOfferData>& offer : offers) {
    auto entity_data = std::make_unique<syncer::EntityData>();
    sync_pb::AutofillOfferSpecifics* offer_specifics =
        entity_data->specifics.mutable_autofill_offer();
    SetAutofillOfferSpecificsFromOfferData(*offer, offer_specifics);

    entity_data->name =
        "Offer " +
        GetBase64EncodedId(GetClientTagFromSpecifics(*offer_specifics));

    batch->Put(GetStorageKeyFromSpecifics(*offer_specifics),
               std::move(entity_data));
  }
  std::move(callback).Run(std::move(batch));
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

  AutofillTable* table = GetAutofillTable();

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
  // even if the wallet data has not changed because the model type state incl.
  // the progress marker always changes.
  web_data_backend_->CommitChanges();

  if (offer_data_changed) {
    web_data_backend_->NotifyOfMultipleAutofillChanges();
  }
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
