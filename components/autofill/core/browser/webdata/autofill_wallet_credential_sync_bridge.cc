// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_credential_sync_bridge.h"

#include <utility>

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"

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
}

AutofillWalletCredentialSyncBridge::~AutofillWalletCredentialSyncBridge() =
    default;

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletCredentialSyncBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return nullptr;
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
  NOTIMPLEMENTED();
  return "";
}

std::string AutofillWalletCredentialSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

void AutofillWalletCredentialSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  NOTIMPLEMENTED();
}

}  // namespace autofill
