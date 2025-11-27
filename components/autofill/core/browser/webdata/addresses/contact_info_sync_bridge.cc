// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/contact_info_sync_bridge.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_requirement_utils.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_sync_util.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/features.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/webdata/common/web_database.h"

namespace autofill {

namespace {

// The address of this variable is used as the user data key.
static int kContactInfoSyncBridgeUserDataKey = 0;

// Different types of account profiles exist, all of which are synced through
// CONTACT_INFO.
DenseSet<AutofillProfile::RecordType> kAccountRecordTypes = {
    AutofillProfile::RecordType::kAccount,
    AutofillProfile::RecordType::kAccountHome,
    AutofillProfile::RecordType::kAccountWork};

// H/W addresses need to meet Autofill's completeness requirements since they
// are read from a source that doesn't enforce them.
// This is not checked as part of the bridge's IsEntityDataValid(), since H/W
// addresses that fail to meet the requirements after an update need to be
// removed from local storage.
bool IsIncompleteHomeAndWorkAddress(const AutofillProfile& profile) {
  if (!profile.IsHomeAndWorkProfile() ||
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForHomeAndWork)) {
    return false;
  }
  const bool is_incomplete = !IsMinimumAddress(profile);
  base::UmaHistogramBoolean("Autofill.HomeAndWork.ProfileFiltered",
                            is_incomplete);
  return is_incomplete;
}

}  // namespace

ContactInfoSyncBridge::ContactInfoSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    AutofillWebDataBackend* backend)
    : DataTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(backend) {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable()) {
    DataTypeSyncBridge::change_processor()->ReportError(
        {FROM_HERE, syncer::ModelError::Type::
                        kContactInfoFailedToLoadAutofillWebDatabase});
    return;
  }
  scoped_observation_.Observe(web_data_backend_.get());
  LoadMetadata();
}

ContactInfoSyncBridge::~ContactInfoSyncBridge() = default;

// static
void ContactInfoSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData().SetUserData(
      &kContactInfoSyncBridgeUserDataKey,
      std::make_unique<ContactInfoSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::CONTACT_INFO,
              /*dump_stack=*/base::DoNothing()),
          web_data_backend));
}

// static
syncer::DataTypeSyncBridge* ContactInfoSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<ContactInfoSyncBridge*>(
      web_data_service->GetDBUserData().GetUserData(
          &kContactInfoSyncBridgeUserDataKey));
}

std::unique_ptr<syncer::MetadataChangeList>
ContactInfoSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetSyncMetadataStore(), syncer::CONTACT_INFO,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError> ContactInfoSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // Since the local storage is cleared when the data type is disabled in
  // `ApplyDisableSyncChanges()`, `MergeFullSyncData()` simply becomes an
  // `ApplyIncrementalSyncChanges()` call.
  if (auto error = ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                               std::move(entity_data))) {
    return error;
  }
  FlushPendingAccountProfileChanges();
  return std::nullopt;
}

std::optional<syncer::ModelError>
ContactInfoSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  auto transaction = web_data_backend_->GetDatabase()->AcquireTransaction();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_DELETE:
        if (!GetAutofillTable()->RemoveAutofillProfile(change->storage_key())) {
          return syncer::ModelError(
              FROM_HERE, syncer::ModelError::Type::
                             kContactInfoFailedToDeleteProfileForRemoteDelete);
        }
        break;
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        // Deserialize the ContactInfoSpecifics and add/update them in the DB.
        DCHECK(change->data().specifics.has_contact_info());
        AutofillProfile remote = CreateAutofillProfileFromContactInfoSpecifics(
            change->data().specifics.contact_info());
        if (IsIncompleteHomeAndWorkAddress(remote)) {
          // In case H/W was updated and doesn't meet the completeness
          // requirements anymore, remove it.
          // This change doesn't need to be synced back, since H/W is read-only.
          metadata_change_list->ClearMetadata(remote.guid());
          if (!GetAutofillTable()->RemoveAutofillProfile(remote.guid())) {
            return syncer::ModelError(
                FROM_HERE, syncer::ModelError::Type::
                               kContactInfoFailedToDeleteIncompleteHwProfile);
          }
          continue;
        }
        // Since the distinction between adds and updates is not always clear,
        // we check the existence of the profile manually and act accordingly.
        // TODO(crbug.com/40100455): Consider adding an AddOrUpdate() function
        // to AutofillTable's API.
        if (GetAutofillTable()->GetAutofillProfile(remote.guid())) {
          if (!GetAutofillTable()->UpdateAutofillProfile(remote)) {
            return syncer::ModelError(
                FROM_HERE, syncer::ModelError::Type::
                               kContactInfoFailedToUpdateProfileInTable);
          }
        } else {
          if (!GetAutofillTable()->AddAutofillProfile(remote)) {
            return syncer::ModelError(
                FROM_HERE, syncer::ModelError::Type::
                               kContactInfoFailedToAddProfileToTable);
          }
        }
        break;
      }
    }
  }

  // Commits changes through CommitChanges(...) or through the scoped
  // sql::Transaction `transaction` depending on the
  // 'SqlScopedTransactionWebDatabase' Finch experiment.
  web_data_backend_->CommitChanges();
  if (transaction) {
    transaction->Commit();
  }

  // False positives can occur here if an update doesn't change the profile.
  // Since such false positives are fine, and since AutofillTable's API
  // currently doesn't provide a way to detect such cases, we don't distinguish.
  if (!entity_changes.empty())
    web_data_backend_->NotifyOnAutofillChangedBySync(syncer::CONTACT_INFO);

  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> ContactInfoSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::ranges::sort(storage_keys);
  auto filter_by_keys = base::BindRepeating(
      [](const StorageKeyList& storage_keys, const std::string& guid) {
        return std::ranges::binary_search(storage_keys, guid);
      },
      storage_keys);
  return GetDataAndFilter(filter_by_keys);
}

std::unique_ptr<syncer::DataBatch>
ContactInfoSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetDataAndFilter(
      base::BindRepeating([](const std::string& guid) { return true; }));
}

bool ContactInfoSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK(entity_data.specifics.has_contact_info());
  return AreContactInfoSpecificsValid(entity_data.specifics.contact_info());
}

std::string ContactInfoSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string ContactInfoSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK(IsEntityDataValid(entity_data));
  return entity_data.specifics.contact_info().guid();
}

void ContactInfoSyncBridge::AutofillProfileChanged(
    const AutofillProfileChange& change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Determine if the profile change should be uploaded to CONTACT_INFO.
  switch (change.data_model().record_type()) {
    case AutofillProfile::RecordType::kAccount:
      break;
    case AutofillProfile::RecordType::kAccountHome:
    case AutofillProfile::RecordType::kAccountWork:
      // Home and work record types are read-only on the client side. Changes
      // are only persisted locally, but not uploaded.
      return;
    case AutofillProfile::RecordType::kAccountNameEmail:
      // Name and email record type should not be synced.
      return;
    case AutofillProfile::RecordType::kLocalOrSyncable:
      // kLocalOrSyncable addresses are synced through AUTOFILL_PROFILE.
      return;
  }
  if (!change_processor()->IsTrackingMetadata()) {
    pending_account_profile_changes_.push(change);
    return;
  }

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  switch (change.type()) {
    case AutofillProfileChange::ADD:
    case AutofillProfileChange::UPDATE:
      change_processor()->Put(
          change.key(),
          CreateContactInfoEntityDataFromAutofillProfile(
              change.data_model(),
              GetPossiblyTrimmedContactInfoSpecificsDataFromProcessor(
                  change.key())),
          metadata_change_list.get());
      break;
    case AutofillProfileChange::REMOVE:
      change_processor()->Delete(change.key(),
                                 syncer::DeletionOrigin::Unspecified(),
                                 metadata_change_list.get());
      break;
    case AutofillProfileChange::HIDE_IN_AUTOFILL:
      auto entity_data = CreateContactInfoEntityDataFromAutofillProfile(
              change.data_model(),
              GetPossiblyTrimmedContactInfoSpecificsDataFromProcessor(
                  change.key()));
      entity_data->specifics.mutable_contact_info()->set_invisible_in_autofill(
        true);
      change_processor()->Put(
          change.key(), std::move(entity_data), metadata_change_list.get());
      break;
  }

  // Local changes (written by the processor via the metadata change list) don't
  // need to be committed, because the open WebDatabase transaction is committed
  // by the AutofillWebDataService when the original local write operation (that
  // triggered this notification to the bridge) finishes.
}

void ContactInfoSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  auto transaction = web_data_backend_->GetDatabase()->AcquireTransaction();

  if (!GetAutofillTable()->RemoveAllAutofillProfiles(kAccountRecordTypes)) {
    change_processor()->ReportError(
        {FROM_HERE, syncer::ModelError::Type::
                        kContactInfoFailedToDeleteProfilesOnDisableSync});
  }

  // Commits changes through CommitChanges(...) or through the scoped
  // sql::Transaction `transaction` depending on the
  // 'SqlScopedTransactionWebDatabase' Finch experiment.
  web_data_backend_->CommitChanges();
  if (transaction) {
    transaction->Commit();
  }

  // False positives can occur here if there were no profiles to begin with.
  web_data_backend_->NotifyOnAutofillChangedBySync(syncer::CONTACT_INFO);
}

sync_pb::EntitySpecifics
ContactInfoSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  sync_pb::ContactInfoSpecifics trimmed_contact_info_specifics =
      TrimContactInfoSpecificsDataForCaching(entity_specifics.contact_info());

  // If all fields are cleared from the contact info specifics, return a fresh
  // EntitySpecifics to avoid caching a few residual bytes.
  if (trimmed_contact_info_specifics.ByteSizeLong() == 0u) {
    return sync_pb::EntitySpecifics();
  }

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  *trimmed_entity_specifics.mutable_contact_info() =
      std::move(trimmed_contact_info_specifics);

  return trimmed_entity_specifics;
}

const sync_pb::ContactInfoSpecifics&
ContactInfoSyncBridge::GetPossiblyTrimmedContactInfoSpecificsDataFromProcessor(
    const std::string& storage_key) {
  return change_processor()
      ->GetPossiblyTrimmedRemoteSpecifics(storage_key)
      .contact_info();
}

// TODO(crbug.com/40253286): Consider moving this logic to processor.
bool ContactInfoSyncBridge::SyncMetadataCacheContainsSupportedFields(
    const syncer::EntityMetadataMap& metadata_map) const {
  for (const auto& metadata_entry : metadata_map) {
    // Serialize the cached specifics and parse them back to a proto. Any fields
    // that were cached as unknown and are known in the current browser version
    // should be parsed correctly.
    std::string serialized_specifics;
    metadata_entry.second->possibly_trimmed_base_specifics().SerializeToString(
        &serialized_specifics);
    sync_pb::EntitySpecifics parsed_specifics;
    parsed_specifics.ParseFromString(serialized_specifics);

    // If `parsed_specifics` contain any supported fields, they would be cleared
    // by the trimming function.
    if (parsed_specifics.ByteSizeLong() !=
        TrimAllSupportedFieldsFromRemoteSpecifics(parsed_specifics)
            .ByteSizeLong()) {
      return true;
    }
  }

  return false;
}

AddressAutofillTable* ContactInfoSyncBridge::GetAutofillTable() {
  return AddressAutofillTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

AutofillSyncMetadataTable* ContactInfoSyncBridge::GetSyncMetadataStore() {
  return AutofillSyncMetadataTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

std::unique_ptr<syncer::MutableDataBatch>
ContactInfoSyncBridge::GetDataAndFilter(
    base::RepeatingCallback<bool(const std::string&)> filter) {
  std::vector<AutofillProfile> profiles;
  if (!GetAutofillTable()->GetAutofillProfiles(kAccountRecordTypes, profiles)) {
    change_processor()->ReportError(
        {FROM_HERE,
         syncer::ModelError::Type::kContactInfoFailedToLoadProfilesFromTable});
    return nullptr;
  }
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const AutofillProfile& profile : profiles) {
    const std::string& guid = profile.guid();
    if (filter.Run(guid)) {
      batch->Put(
          guid,
          CreateContactInfoEntityDataFromAutofillProfile(
              profile,
              GetPossiblyTrimmedContactInfoSpecificsDataFromProcessor(guid)));
    }
  }
  return batch;
}

void ContactInfoSyncBridge::LoadMetadata() {
  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetSyncMetadataStore()->GetAllSyncMetadata(syncer::CONTACT_INFO,
                                                  batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE, syncer::ModelError::Type::
                        kContactInfoFailedToReadMetadataFromWebDatabase});
    return;
  } else if (SyncMetadataCacheContainsSupportedFields(
                 batch->GetAllMetadata())) {
    // Caching entity specifics is meant to preserve fields not supported in a
    // given browser version during commits to the server. If the cache
    // contains supported fields, this means that the browser was updated and
    // we should force the initial sync flow to propagate the cached data into
    // the local model.
    GetSyncMetadataStore()->DeleteAllSyncMetadata(
        syncer::DataType::CONTACT_INFO);

    batch = std::make_unique<syncer::MetadataBatch>();
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

void ContactInfoSyncBridge::FlushPendingAccountProfileChanges() {
  CHECK(change_processor()->IsTrackingMetadata());
  while (!pending_account_profile_changes_.empty()) {
    const AutofillProfileChange& change =
        pending_account_profile_changes_.front();
    CHECK(change.data_model().IsAccountProfile());
    AutofillProfileChanged(change);
    pending_account_profile_changes_.pop();
  }
}

}  // namespace autofill
