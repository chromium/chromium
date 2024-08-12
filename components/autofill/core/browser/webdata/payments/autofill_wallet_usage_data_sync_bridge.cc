// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/autofill_wallet_usage_data_sync_bridge.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/metrics/payments/wallet_usage_data_metrics.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/sync_metadata_store_change_list.h"

namespace autofill {

namespace {

// Address to this variable used as the user data key.
const int kAutofillWalletUsageDataSyncBridgeUserDataKey = 0;

}  // namespace

// static
void AutofillWalletUsageDataSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillWalletUsageDataSyncBridgeUserDataKey,
      std::make_unique<AutofillWalletUsageDataSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::AUTOFILL_WALLET_USAGE,
              /*dump_stack=*/base::RepeatingClosure()),
          web_data_backend));
}

// static
AutofillWalletUsageDataSyncBridge*
AutofillWalletUsageDataSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletUsageDataSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillWalletUsageDataSyncBridgeUserDataKey));
}

AutofillWalletUsageDataSyncBridge::AutofillWalletUsageDataSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : DataTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(web_data_backend) {
  DCHECK(web_data_backend_);
  DCHECK(GetAutofillTable());

  LoadMetadata();
}

AutofillWalletUsageDataSyncBridge::~AutofillWalletUsageDataSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletUsageDataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetSyncMetadataStore(), syncer::AUTOFILL_WALLET_USAGE,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError>
AutofillWalletUsageDataSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // There is no local data to write, so use ApplyIncrementalSyncChanges.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<syncer::ModelError>
AutofillWalletUsageDataSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PaymentsAutofillTable* table = GetAutofillTable();

  // Only Virtual Card Usage Data is currently supported.
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_DELETE:
        if (table &&
            !table->RemoveVirtualCardUsageData(change->storage_key())) {
          return syncer::ModelError(
              FROM_HERE,
              "Failed to delete virtual card usage data from table.");
        }
        break;
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        // TODO(crbug.com/40255173): AddOrUpdate VirtualCardUsageData method for
        // Autofill Table
        DCHECK(IsEntityDataValid(change->data()));
        bool valid_data = IsVirtualCardUsageDataSpecificsValid(
            change->data()
                .specifics.autofill_wallet_usage()
                .virtual_card_usage_data());
        autofill_metrics::LogSyncedVirtualCardUsageDataBeingValid(valid_data);
        if (!valid_data) {
          continue;
        }
        VirtualCardUsageData remote = VirtualCardUsageDataFromUsageSpecifics(
            change->data().specifics.autofill_wallet_usage());
        if (table && !table->AddOrUpdateVirtualCardUsageData(remote)) {
          return syncer::ModelError(
              FROM_HERE,
              "Failed to add or update virtual card usage data in table.");
        }
      }
    }
  }

  // Commit the transaction to make sure the data and the metadata with the
  // new progress marker is written down.
  web_data_backend_->CommitChanges();

  // False positives can occur here if an update doesn't change the profile.
  // Since such false positives are fine, and since PaymentsAutofillTable's API
  // currently doesn't provide a way to detect such cases, we don't distinguish.
  if (!entity_data.empty()) {
    web_data_backend_->NotifyOnAutofillChangedBySync(
        syncer::AUTOFILL_WALLET_USAGE);
  }

  return change_processor()->GetError();
}

std::unique_ptr<syncer::DataBatch>
AutofillWalletUsageDataSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ranges::sort(storage_keys);
  auto filter_by_keys = base::BindRepeating(
      [](const StorageKeyList& storage_keys, const std::string& usage_data_id) {
        return base::ranges::binary_search(storage_keys, usage_data_id);
      },
      storage_keys);
  return GetDataAndFilter(filter_by_keys);
}

std::unique_ptr<syncer::DataBatch>
AutofillWalletUsageDataSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetDataAndFilter(base::BindRepeating(
      [](const std::string& usage_data_id) { return true; }));
}

std::string AutofillWalletUsageDataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_wallet_usage());
  return entity_data.specifics.autofill_wallet_usage().guid();
}

std::string AutofillWalletUsageDataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_wallet_usage());

  // Use client tag as the storage key.
  return GetClientTag(entity_data);
}

void AutofillWalletUsageDataSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  PaymentsAutofillTable* table = GetAutofillTable();
  if (table && !table->RemoveAllVirtualCardUsageData()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to delete usage data from table."});
  }
  web_data_backend_->CommitChanges();
  web_data_backend_->NotifyOnAutofillChangedBySync(
      syncer::AUTOFILL_WALLET_USAGE);
}

bool AutofillWalletUsageDataSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.has_autofill_wallet_usage() &&
         entity_data.specifics.autofill_wallet_usage()
             .has_virtual_card_usage_data();
}

PaymentsAutofillTable* AutofillWalletUsageDataSyncBridge::GetAutofillTable() {
  return PaymentsAutofillTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

AutofillSyncMetadataTable*
AutofillWalletUsageDataSyncBridge::GetSyncMetadataStore() {
  return AutofillSyncMetadataTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

void AutofillWalletUsageDataSyncBridge::LoadMetadata() {
  if (!web_data_backend_->GetDatabase() || !GetAutofillTable() ||
      !GetSyncMetadataStore()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load Autofill table."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetSyncMetadataStore()->GetAllSyncMetadata(syncer::AUTOFILL_WALLET_USAGE,
                                                  batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading Autofill Wallet usage metadata from WebDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

std::unique_ptr<syncer::MutableDataBatch>
AutofillWalletUsageDataSyncBridge::GetDataAndFilter(
    base::RepeatingCallback<bool(const std::string&)> filter) {
  std::vector<VirtualCardUsageData> virtual_card_usage_data_list;
  if (!GetAutofillTable()->GetAllVirtualCardUsageData(
          virtual_card_usage_data_list)) {
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed to load Autofill Wallet usage data data from table."});
    return nullptr;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const VirtualCardUsageData& virtual_card_usage_data :
       virtual_card_usage_data_list) {
    if (filter.Run(*virtual_card_usage_data.usage_data_id())) {
      AutofillWalletUsageData usage_data =
          AutofillWalletUsageData::ForVirtualCard(virtual_card_usage_data);
      auto entity_data = std::make_unique<syncer::EntityData>();
      sync_pb::AutofillWalletUsageSpecifics* usage_specifics =
          entity_data->specifics.mutable_autofill_wallet_usage();
      SetAutofillWalletUsageSpecificsFromAutofillWalletUsageData(
          usage_data, usage_specifics);

      std::string storage_key = GetStorageKey(*entity_data);
      entity_data->name = storage_key;
      batch->Put(storage_key, std::move(entity_data));
    }
  }
  return batch;
}

}  // namespace autofill
