// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/proto/autofill_sync.pb.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_difference_tracker.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"

using base::Optional;
using base::UTF16ToUTF8;
using sync_pb::AutofillProfileSpecifics;
using syncer::EntityData;
using syncer::MetadataChangeList;
using syncer::ModelError;

namespace autofill {

namespace {

// Simplify checking for optional errors and returning only when present.
#define RETURN_IF_ERROR(x)                \
  if (Optional<ModelError> ret_val = x) { \
    return ret_val;                       \
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
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL_PROFILE,
              /*dump_stack=*/base::RepeatingClosure()),
          app_locale, web_data_backend));
}

// static
syncer::ModelTypeSyncBridge* AutofillProfileSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillProfileSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillProfileSyncBridgeUserDataKey));
}

AutofillProfileSyncBridge::AutofillProfileSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    const std::string& app_locale,
    AutofillWebDataBackend* backend)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)),
      app_locale_(app_locale),
      web_data_backend_(backend) {
  DCHECK(web_data_backend_);

  scoped_observer_.Add(web_data_backend_);

  LoadMetadata();
}

AutofillProfileSyncBridge::~AutofillProfileSyncBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

std::unique_ptr<MetadataChangeList>
AutofillProfileSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::AUTOFILL_PROFILE);
}

Optional<syncer::ModelError> AutofillProfileSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  AutofillProfileInitialSyncDifferenceTracker initial_sync_tracker(
      GetAutofillTable());

  for (const auto& change : entity_data) {
    DCHECK(change->data().specifics.has_autofill_profile());
    std::unique_ptr<AutofillProfile> remote =
        CreateAutofillProfileFromSpecifics(
            change->data().specifics.autofill_profile());
    if (!remote) {
      DVLOG(2)
          << "[AUTOFILL SYNC] Invalid remote specifics "
          << change->data().specifics.autofill_profile().SerializeAsString()
          << " received from the server in an initial sync.";
      continue;
    }
    RETURN_IF_ERROR(
        initial_sync_tracker.IncorporateRemoteProfile(std::move(remote)));
  }

  RETURN_IF_ERROR(
      initial_sync_tracker.MergeSimilarEntriesForInitialSync(app_locale_));
  RETURN_IF_ERROR(
      FlushSyncTracker(std::move(metadata_change_list), &initial_sync_tracker));

  web_data_backend_->CommitChanges();
  web_data_backend_->NotifyThatSyncHasStarted(syncer::AUTOFILL_PROFILE);
  return base::nullopt;
}

Optional<ModelError> AutofillProfileSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  AutofillProfileSyncDifferenceTracker tracker(GetAutofillTable());
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      RETURN_IF_ERROR(tracker.IncorporateRemoteDelete(change->storage_key()));
    } else {
      DCHECK(change->data().specifics.has_autofill_profile());
      std::unique_ptr<AutofillProfile> remote =
          CreateAutofillProfileFromSpecifics(
              change->data().specifics.autofill_profile());
      if (!remote) {
        DVLOG(2)
            << "[AUTOFILL SYNC] Invalid remote specifics "
            << change->data().specifics.autofill_profile().SerializeAsString()
            << " received from the server in an initial sync.";
        continue;
      }
      RETURN_IF_ERROR(tracker.IncorporateRemoteProfile(std::move(remote)));
    }
  }

  RETURN_IF_ERROR(FlushSyncTracker(std::move(metadata_change_list), &tracker));

  web_data_backend_->CommitChanges();
  return base::nullopt;
}

void AutofillProfileSyncBridge::GetData(StorageKeyList storage_keys,
                                        DataCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<std::unique_ptr<AutofillProfile>> entries;
  if (!GetAutofillTable()->GetAutofillProfiles(&entries)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return;
  }

  std::unordered_set<std::string> keys_set(storage_keys.begin(),
                                           storage_keys.end());
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::unique_ptr<AutofillProfile>& entry : entries) {
    std::string key = GetStorageKeyFromAutofillProfile(*entry);
    if (base::Contains(keys_set, key)) {
      batch->Put(key, CreateEntityDataFromAutofillProfile(*entry));
    }
  }
  std::move(callback).Run(std::move(batch));
}

void AutofillProfileSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<std::unique_ptr<AutofillProfile>> entries;
  if (!GetAutofillTable()->GetAutofillProfiles(&entries)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::unique_ptr<AutofillProfile>& entry : entries) {
    batch->Put(GetStorageKeyFromAutofillProfile(*entry),
               CreateEntityDataFromAutofillProfile(*entry));
  }
  std::move(callback).Run(std::move(batch));
}

void AutofillProfileSyncBridge::ActOnLocalChange(
    const AutofillProfileChange& change) {
  DCHECK(change.data_model());
  if (!change_processor()->IsTrackingMetadata() ||
      change.data_model()->record_type() != AutofillProfile::LOCAL_PROFILE) {
    return;
  }

  auto metadata_change_list =
      std::make_unique<syncer::SyncMetadataStoreChangeList>(
          GetAutofillTable(), syncer::AUTOFILL_PROFILE);

  switch (change.type()) {
    case AutofillProfileChange::ADD:
    case AutofillProfileChange::UPDATE:
      change_processor()->Put(
          change.key(),
          CreateEntityDataFromAutofillProfile(*change.data_model()),
          metadata_change_list.get());
      break;
    case AutofillProfileChange::REMOVE:
      change_processor()->Delete(change.key(), metadata_change_list.get());
      break;
    case AutofillProfileChange::EXPIRE:
      // EXPIRE changes are not being issued for profiles.
      NOTREACHED();
      break;
  }

  // We do not need to commit any local changes (written by the processor via
  // the metadata change list) because the open WebDatabase transaction is
  // committed by the AutofillWebDataService when the original local write
  // operation (that triggered this notification to the bridge) finishes.

  if (Optional<ModelError> error = metadata_change_list->TakeError()) {
    change_processor()->ReportError(*error);
  }
}

base::Optional<syncer::ModelError> AutofillProfileSyncBridge::FlushSyncTracker(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    AutofillProfileSyncDifferenceTracker* tracker) {
  DCHECK(tracker);

  RETURN_IF_ERROR(tracker->FlushToLocal(
      base::BindOnce(&AutofillWebDataBackend::NotifyOfMultipleAutofillChanges,
                     base::Unretained(web_data_backend_))));

  std::vector<std::unique_ptr<AutofillProfile>> profiles_to_upload_to_sync;
  RETURN_IF_ERROR(tracker->FlushToSync(&profiles_to_upload_to_sync));
  for (const std::unique_ptr<AutofillProfile>& entry :
       profiles_to_upload_to_sync) {
    change_processor()->Put(GetStorageKeyFromAutofillProfile(*entry),
                            CreateEntityDataFromAutofillProfile(*entry),
                            metadata_change_list.get());
  }

  return static_cast<syncer::SyncMetadataStoreChangeList*>(
             metadata_change_list.get())
      ->TakeError();
}

void AutofillProfileSyncBridge::LoadMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetAutofillTable()->GetAllSyncMetadata(syncer::AUTOFILL_PROFILE,
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ActOnLocalChange(change);
}

AutofillTable* AutofillProfileSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

}  // namespace autofill
