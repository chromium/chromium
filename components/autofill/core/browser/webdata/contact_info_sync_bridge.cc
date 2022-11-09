// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/contact_info_sync_bridge.h"

#include "base/check.h"
#include "base/guid.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/contact_info_sync_util.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/sync_metadata_store_change_list.h"

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
  DCHECK(web_data_backend_->GetDatabase());
  DCHECK(GetAutofillTable());
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::CONTACT_INFO,
      base::BindRepeating(&syncer::ModelTypeChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::ranges::sort(storage_keys);
  auto filter_by_keys = base::BindRepeating(
      [](const StorageKeyList& storage_keys, const std::string& guid) {
        return base::ranges::binary_search(storage_keys, guid);
      },
      storage_keys);
  if (std::unique_ptr<syncer::MutableDataBatch> batch =
          GetDataAndFilter(filter_by_keys)) {
    std::move(callback).Run(std::move(batch));
  }
}

void ContactInfoSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (std::unique_ptr<syncer::MutableDataBatch> batch = GetDataAndFilter(
          base::BindRepeating([](const std::string& guid) { return true; }))) {
    std::move(callback).Run(std::move(batch));
  }
}

std::string ContactInfoSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string ContactInfoSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_contact_info());
  const std::string& guid = entity_data.specifics.contact_info().guid();
  // For invalid `entity_data`, `GetStorageKey()` should return an empty string.
  return base::GUID::ParseLowercase(guid).is_valid() ? guid : "";
}

AutofillTable* ContactInfoSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

std::unique_ptr<syncer::MutableDataBatch>
ContactInfoSyncBridge::GetDataAndFilter(
    base::RepeatingCallback<bool(const std::string&)> filter) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  if (!GetAutofillTable()->GetAutofillProfiles(
          &profiles, AutofillProfile::Source::kAccount)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load profiles from table."});
    return nullptr;
  }
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::unique_ptr<AutofillProfile>& profile : profiles) {
    const std::string& guid = profile->guid();
    if (filter.Run(guid)) {
      batch->Put(guid,
                 CreateContactInfoEntityDataFromAutofillProfile(*profile));
    }
  }
  return batch;
}

}  // namespace autofill
