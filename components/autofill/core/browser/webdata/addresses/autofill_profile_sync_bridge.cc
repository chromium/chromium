// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_bridge.h"

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/proto/autofill_sync.pb.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_difference_tracker.h"
#include "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/entity_data.h"

using sync_pb::AutofillProfileSpecifics;
using syncer::EntityData;
using syncer::MetadataChangeList;
using syncer::ModelError;

namespace autofill {

namespace {

// Simplify checking for optional errors and returning only when present.
#undef RETURN_IF_ERROR
#define RETURN_IF_ERROR(x)                     \
  if (std::optional<ModelError> ret_val = x) { \
    return ret_val;                            \
  }

// Address to this variable used as the user data key.
static int kAutofillProfileSyncBridgeUserDataKey = 0;

}  // namespace

// static
void AutofillProfileSyncBridge::CreateForWebDataServiceAndBackend(
    const std::string& app_locale,
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillProfileSyncBridgeUserDataKey,
      std::make_unique<AutofillProfileSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::AUTOFILL_PROFILE,
              /*dump_stack=*/base::DoNothing()),
          app_locale, web_data_backend));
}

// static
syncer::DataTypeSyncBridge* AutofillProfileSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillProfileSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillProfileSyncBridgeUserDataKey));
}

AutofillProfileSyncBridge::AutofillProfileSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    const std::string& app_locale,
    AutofillWebDataBackend* backend)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      app_locale_(app_locale),
      web_data_backend_(backend) {
  DCHECK(web_data_backend_);

  scoped_observation_.Observe(web_data_backend_.get());

  LoadMetadata();
}

AutofillProfileSyncBridge::~AutofillProfileSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<MetadataChangeList>
AutofillProfileSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetSyncMetadataStore(), syncer::AUTOFILL_PROFILE,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError> AutofillProfileSyncBridge::MergeFullSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AutofillProfileInitialSyncDifferenceTracker initial_sync_tracker(
      GetAutofillTable());

  for (const auto& change : entity_data) {
    DCHECK(change->data().specifics.has_autofill_profile());
    std::optional<AutofillProfile> remote = CreateAutofillProfileFromSpecifics(
        change->data().specifics.autofill_profile());
    if (!remote) {
      DVLOG(2)
          << "[AUTOFILL SYNC] Invalid remote specifics "
          << change->data().specifics.autofill_profile().SerializeAsString()
          << " received from the server in an initial sync.";
      continue;
    }
    RETURN_IF_ERROR(
        initial_sync_tracker.IncorporateRemoteProfile(std::move(*remote)));
  }

  RETURN_IF_ERROR(
      initial_sync_tracker.MergeSimilarEntriesForInitialSync(app_locale_));
  RETURN_IF_ERROR(
      FlushSyncTracker(std::move(metadata_change_list), &initial_sync_tracker));

  web_data_backend_->CommitChanges();
  return std::nullopt;
}

std::optional<ModelError>
AutofillProfileSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AutofillProfileSyncDifferenceTracker tracker(GetAutofillTable());
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      RETURN_IF_ERROR(tracker.IncorporateRemoteDelete(change->storage_key()));
    } else {
      DCHECK(change->data().specifics.has_autofill_profile());
      std::optional<AutofillProfile> remote =
          CreateAutofillProfileFromSpecifics(
              change->data().specifics.autofill_profile());
      if (!remote) {
        DVLOG(2)
            << "[AUTOFILL SYNC] Invalid remote specifics "
            << change->data().specifics.autofill_profile().SerializeAsString()
            << " received from the server in an initial sync.";
        continue;
      }
      RETURN_IF_ERROR(tracker.IncorporateRemoteProfile(std::move(*remote)));
    }
  }

  RETURN_IF_ERROR(FlushSyncTracker(std::move(metadata_change_list), &tracker));

  web_data_backend_->CommitChanges();
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> AutofillProfileSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<AutofillProfile> entries;
  if (!GetAutofillTable()->GetAutofillProfiles(
          {AutofillProfile::RecordType::kLocalOrSyncable}, entries)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return nullptr;
  }

  std::unordered_set<std::string> keys_set(storage_keys.begin(),
                                           storage_keys.end());
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const AutofillProfile& entry : entries) {
    std::string key = GetStorageKeyFromAutofillProfile(entry);
    if (keys_set.contains(key)) {
      batch->Put(key, CreateEntityDataFromAutofillProfile(entry));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
AutofillProfileSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<AutofillProfile> entries;
  if (!GetAutofillTable()->GetAutofillProfiles(
          {AutofillProfile::RecordType::kLocalOrSyncable}, entries)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return nullptr;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const AutofillProfile& entry : entries) {
    batch->Put(GetStorageKeyFromAutofillProfile(entry),
               CreateEntityDataFromAutofillProfile(entry));
  }
  return batch;
}

void AutofillProfileSyncBridge::ActOnLocalChange(
    const AutofillProfileChange& change) {
  if (!change_processor()->IsTrackingMetadata() ||
      change.data_model().IsAccountProfile()) {
    return;
  }

  std::unique_ptr<MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  switch (change.type()) {
    case AutofillProfileChange::ADD:
    case AutofillProfileChange::UPDATE:
      change_processor()->Put(
          change.key(),
          CreateEntityDataFromAutofillProfile(change.data_model()),
          metadata_change_list.get());
      break;
    case AutofillProfileChange::REMOVE:
      change_processor()->Delete(change.key(),
                                 syncer::DeletionOrigin::Unspecified(),
                                 metadata_change_list.get());
      break;
  }

  // We do not need to commit any local changes (written by the processor via
  // the metadata change list) because the open WebDatabase transaction is
  // committed by the AutofillWebDataService when the original local write
  // operation (that triggered this notification to the bridge) finishes.
}

std::optional<syncer::ModelError> AutofillProfileSyncBridge::FlushSyncTracker(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    AutofillProfileSyncDifferenceTracker* tracker) {
  DCHECK(tracker);

  RETURN_IF_ERROR(tracker->FlushToLocal(base::BindOnce(
      &AutofillWebDataBackend::NotifyOnAutofillChangedBySync,
      base::Unretained(web_data_backend_), syncer::AUTOFILL_PROFILE)));

  std::vector<AutofillProfile> profiles_to_upload_to_sync;
  std::vector<std::string> profiles_to_delete_from_sync;
  RETURN_IF_ERROR(tracker->FlushToSync(&profiles_to_upload_to_sync,
                                       &profiles_to_delete_from_sync));
  for (const AutofillProfile& entry : profiles_to_upload_to_sync) {
    change_processor()->Put(GetStorageKeyFromAutofillProfile(entry),
                            CreateEntityDataFromAutofillProfile(entry),
                            metadata_change_list.get());
  }
  for (const std::string& storage_key : profiles_to_delete_from_sync) {
    change_processor()->Delete(storage_key,
                               syncer::DeletionOrigin::Unspecified(),
                               metadata_change_list.get());
  }

  return change_processor()->GetError();
}

void AutofillProfileSyncBridge::LoadMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable() || !GetSyncMetadataStore()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetSyncMetadataStore()->GetAllSyncMetadata(syncer::AUTOFILL_PROFILE,
                                                  batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed reading autofill metadata from WebDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

std::string AutofillProfileSyncBridge::GetClientTag(
    const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_profile());
  // Must equal to guid of the entry. This is to maintain compatibility with the
  // previous sync integration (Directory and SyncableService).
  return entity_data.specifics.autofill_profile().guid();
}

std::string AutofillProfileSyncBridge::GetStorageKey(
    const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_profile());
  return GetStorageKeyFromAutofillProfileSpecifics(
      entity_data.specifics.autofill_profile());
}

void AutofillProfileSyncBridge::AutofillProfileChanged(
    const AutofillProfileChange& change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ActOnLocalChange(change);
}

AddressAutofillTable* AutofillProfileSyncBridge::GetAutofillTable() {
  return AddressAutofillTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

AutofillSyncMetadataTable* AutofillProfileSyncBridge::GetSyncMetadataStore() {
  return AutofillSyncMetadataTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

}  // namespace autofill
