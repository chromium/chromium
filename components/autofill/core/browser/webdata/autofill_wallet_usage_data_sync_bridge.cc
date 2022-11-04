// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_usage_data_sync_bridge.h"

#include <utility>

#include "base/strings/string_util.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/sync_metadata_store_change_list.h"

namespace autofill {

namespace {

// Address to this variable used as the user data key.
const int kAutofillWalletUsageDataSyncBridgeUserDataKey = 0;

// Prefix used as the client tag for Virtual Card Usage Data.
constexpr char kVirtualCardUsageDataClientTagPrefix[] = "VirtualCardUsageData";

}  // namespace

// static
void AutofillWalletUsageDataSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillWalletUsageDataSyncBridgeUserDataKey,
      std::make_unique<AutofillWalletUsageDataSyncBridge>(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
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
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(web_data_backend) {
  DCHECK(web_data_backend_);

  LoadMetadata();
}

AutofillWalletUsageDataSyncBridge::~AutofillWalletUsageDataSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletUsageDataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::AUTOFILL_WALLET_USAGE,
      base::BindRepeating(&syncer::ModelTypeChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

absl::optional<syncer::ModelError>
AutofillWalletUsageDataSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

absl::optional<syncer::ModelError>
AutofillWalletUsageDataSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

void AutofillWalletUsageDataSyncBridge::GetData(StorageKeyList storage_keys,
                                                DataCallback callback) {
  NOTIMPLEMENTED();
}

void AutofillWalletUsageDataSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string AutofillWalletUsageDataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_wallet_usage());
  const sync_pb::AutofillWalletUsageSpecifics::VirtualCardUsageData&
      virtual_card_usage_data = entity_data.specifics.autofill_wallet_usage()
                                    .virtual_card_usage_data();

  return base::JoinString(
      {kVirtualCardUsageDataClientTagPrefix,
       base::NumberToString(virtual_card_usage_data.instrument_id()),
       virtual_card_usage_data.merchant_url(),
       virtual_card_usage_data.merchant_app_package()},
      "|");
}

std::string AutofillWalletUsageDataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_wallet_usage());

  // Use client tag as the storage key.
  return GetClientTag(entity_data);
}

void AutofillWalletUsageDataSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  NOTIMPLEMENTED();
}

AutofillTable* AutofillWalletUsageDataSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

void AutofillWalletUsageDataSyncBridge::LoadMetadata() {
  if (!web_data_backend_->GetDatabase() || !GetAutofillTable()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load Autofill table."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetAutofillTable()->GetAllSyncMetadata(syncer::AUTOFILL_WALLET_USAGE,
                                              batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading Autofill Wallet usage metadata from WebDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

}  // namespace autofill
