// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/contact_info_sync_bridge.h"

#include "base/notreached.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"

namespace autofill {

namespace {

// The address of this variable is used as the user data key.
static int kContactInfoSyncBridgeUserDataKey = 0;

}  // namespace

ContactInfoSyncBridge::ContactInfoSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    AutofillWebDataBackend* backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(backend) {
  DCHECK(web_data_backend_);
  scoped_observation_.Observe(web_data_backend_.get());
}

ContactInfoSyncBridge::~ContactInfoSyncBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// static
void ContactInfoSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kContactInfoSyncBridgeUserDataKey,
      std::make_unique<ContactInfoSyncBridge>(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::CONTACT_INFO,
              /*dump_stack=*/base::DoNothing()),
          web_data_backend));
}

// static
syncer::ModelTypeSyncBridge* ContactInfoSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<ContactInfoSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kContactInfoSyncBridgeUserDataKey));
}

std::unique_ptr<syncer::MetadataChangeList>
ContactInfoSyncBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return nullptr;
}

absl::optional<syncer::ModelError> ContactInfoSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

absl::optional<syncer::ModelError> ContactInfoSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

void ContactInfoSyncBridge::GetData(StorageKeyList storage_keys,
                                    DataCallback callback) {
  NOTIMPLEMENTED();
}

void ContactInfoSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string ContactInfoSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

std::string ContactInfoSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

}  // namespace autofill
