// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"

#include <unordered_map>
#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/optional.h"
#include "components/autofill/core/browser/autofill_metadata.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"

namespace autofill {

namespace {

using sync_pb::WalletMetadataSpecifics;
using syncer::EntityData;
using syncer::MetadataChangeList;

// Address to this variable used as the user data key.
static int kAutofillWalletMetadataSyncBridgeUserDataKey = 0;

std::string GetClientTagForSpecificsId(WalletMetadataSpecifics::Type type,
                                       const std::string& specifics_id) {
  switch (type) {
    case WalletMetadataSpecifics::ADDRESS:
      return "address-" + specifics_id;
    case WalletMetadataSpecifics::CARD:
      return "card-" + specifics_id;
    case WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED();
      return "";
  }
}

// Returns EntityData for wallet_metadata for |local_metadata| and |type|.
std::unique_ptr<EntityData> CreateEntityDataFromAutofillMetadata(
    const AutofillMetadata& local_metadata,
    WalletMetadataSpecifics::Type type) {
  auto entity_data = std::make_unique<EntityData>();
  std::string specifics_id = GetSpecificsIdForMetadataId(local_metadata.id);
  entity_data->non_unique_name = GetClientTagForSpecificsId(type, specifics_id);

  WalletMetadataSpecifics* remote_metadata =
      entity_data->specifics.mutable_wallet_metadata();
  remote_metadata->set_type(type);
  remote_metadata->set_id(specifics_id);
  remote_metadata->set_use_count(local_metadata.use_count);
  remote_metadata->set_use_date(
      local_metadata.use_date.ToDeltaSinceWindowsEpoch().InMicroseconds());

  switch (type) {
    case WalletMetadataSpecifics::ADDRESS: {
      remote_metadata->set_address_has_converted(local_metadata.has_converted);
      break;
    }
    case WalletMetadataSpecifics::CARD: {
      // The strings must be in valid UTF-8 to sync.
      std::string billing_address_id;
      base::Base64Encode(local_metadata.billing_address_id,
                         &billing_address_id);
      remote_metadata->set_card_billing_address_id(billing_address_id);
      break;
    }
    case WalletMetadataSpecifics::UNKNOWN: {
      NOTREACHED();
      break;
    }
  }

  return entity_data;
}

// Returns EntityData for wallet_metadata for |local_profile|.
std::unique_ptr<EntityData> CreateMetadataEntityDataFromAutofillServerProfile(
    const AutofillProfile& local_profile) {
  return CreateEntityDataFromAutofillMetadata(local_profile.GetMetadata(),
                                              WalletMetadataSpecifics::ADDRESS);
}

// Returns EntityData for wallet_metadata for |local_card|.
std::unique_ptr<EntityData> CreateMetadataEntityDataFromCard(
    const CreditCard& local_card) {
  return CreateEntityDataFromAutofillMetadata(local_card.GetMetadata(),
                                              WalletMetadataSpecifics::CARD);
}

}  // namespace

// static
void AutofillWalletMetadataSyncBridge::CreateForWebDataServiceAndBackend(
    const std::string& app_locale,
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillWalletMetadataSyncBridgeUserDataKey,
      std::make_unique<AutofillWalletMetadataSyncBridge>(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL_WALLET_METADATA,
              /*dump_stack=*/base::RepeatingClosure()),
          web_data_backend));
}

// static
AutofillWalletMetadataSyncBridge*
AutofillWalletMetadataSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletMetadataSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillWalletMetadataSyncBridgeUserDataKey));
}

AutofillWalletMetadataSyncBridge::AutofillWalletMetadataSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(web_data_backend),
      scoped_observer_(this),
      track_wallet_data_(false),
      weak_ptr_factory_(this) {
  DCHECK(web_data_backend_);
  scoped_observer_.Add(web_data_backend_);

  LoadDataCacheAndMetadata();
}

AutofillWalletMetadataSyncBridge::~AutofillWalletMetadataSyncBridge() {}

void AutofillWalletMetadataSyncBridge::OnWalletDataTrackingStateChanged(
    bool is_tracking) {
  track_wallet_data_ = is_tracking;
}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletMetadataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::AUTOFILL_WALLET_METADATA);
}

base::Optional<syncer::ModelError>
AutofillWalletMetadataSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<syncer::ModelError>
AutofillWalletMetadataSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

void AutofillWalletMetadataSyncBridge::GetData(StorageKeyList storage_keys,
                                               DataCallback callback) {
  // Build a set out of the list to allow quick lookup.
  std::unordered_set<std::string> storage_keys_set(storage_keys.begin(),
                                                   storage_keys.end());
  GetDataImpl(std::move(storage_keys_set), std::move(callback));
}

void AutofillWalletMetadataSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  // Get all data by not providing any |storage_keys| filter.
  GetDataImpl(/*storage_keys=*/base::nullopt, std::move(callback));
}

std::string AutofillWalletMetadataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  const WalletMetadataSpecifics& remote_metadata =
      entity_data.specifics.wallet_metadata();
  return GetClientTagForSpecificsId(remote_metadata.type(),
                                    remote_metadata.id());
}

std::string AutofillWalletMetadataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return GetStorageKeyForSpecificsId(
      entity_data.specifics.wallet_metadata().id());
}

void AutofillWalletMetadataSyncBridge::AutofillProfileChanged(
    const AutofillProfileChange& change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const AutofillProfile* changed = change.data_model();
  if (!changed || changed->record_type() != AutofillProfile::SERVER_PROFILE) {
    return;
  }

  // The only legal change on a server profile is that its use count or use date
  // or has-converted status gets updated. Other changes (adding, deleting) are
  // only done by the AutofillWalletSyncBridge and result only in the
  // AutofillMultipleChanged() notification.
  DCHECK(change.type() == AutofillProfileChange::UPDATE);
  SyncUpUpdatedEntity(
      CreateMetadataEntityDataFromAutofillServerProfile(*changed));
}

void AutofillWalletMetadataSyncBridge::CreditCardChanged(
    const CreditCardChange& change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const CreditCard* changed = change.data_model();
  if (!changed || changed->record_type() == CreditCard::LOCAL_CARD) {
    return;
  }

  // The only legal change on a server card is that its use count or use date or
  // billing address id gets updated. Other changes (adding, deleting) are only
  // done by the AutofillWalletSyncBridge and result only in the
  // AutofillMultipleChanged() notification.
  DCHECK(change.type() == CreditCardChange::UPDATE);
  SyncUpUpdatedEntity(CreateMetadataEntityDataFromCard(*changed));
}

void AutofillWalletMetadataSyncBridge::AutofillMultipleChanged() {
  NOTIMPLEMENTED();
}

void AutofillWalletMetadataSyncBridge::SyncUpUpdatedEntity(
    std::unique_ptr<EntityData> entity_after_change) {
  std::string storage_key = GetStorageKey(*entity_after_change);
  auto it = cache_.find(storage_key);

  // This *changed* entity should already be in the cache, ignore otherwise.
  if (it == cache_.end())
    return;

  const WalletMetadataSpecifics& specifics_before = it->second;
  const WalletMetadataSpecifics& specifics_after =
      entity_after_change->specifics.wallet_metadata();

  if (specifics_before.use_count() < specifics_after.use_count() &&
      specifics_before.use_date() < specifics_after.use_date()) {
    std::unique_ptr<MetadataChangeList> metadata_change_list =
        CreateMetadataChangeList();
    cache_[storage_key] = specifics_after;
    change_processor()->Put(storage_key, std::move(entity_after_change),
                            metadata_change_list.get());
  }
}

AutofillTable* AutofillWalletMetadataSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

void AutofillWalletMetadataSyncBridge::LoadDataCacheAndMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }

  // Load the data cache.
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<std::unique_ptr<CreditCard>> cards;
  if (!GetAutofillTable()->GetServerProfiles(&profiles) ||
      !GetAutofillTable()->GetServerCreditCards(&cards)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed reading autofill data from WebDatabase."});
    return;
  }
  for (const std::unique_ptr<AutofillProfile>& entry : profiles) {
    cache_[GetStorageKeyForMetadataId(entry->GetMetadata().id)] =
        CreateMetadataEntityDataFromAutofillServerProfile(*entry)
            ->specifics.wallet_metadata();
  }
  for (const std::unique_ptr<CreditCard>& entry : cards) {
    cache_[GetStorageKeyForMetadataId(entry->GetMetadata().id)] =
        CreateMetadataEntityDataFromCard(*entry)->specifics.wallet_metadata();
  }

  // Load the metadata and send to the processor.
  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetAutofillTable()->GetAllSyncMetadata(syncer::AUTOFILL_WALLET_METADATA,
                                              batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed reading autofill metadata from WebDatabase."});
    return;
  }

  change_processor()->ModelReadyToSync(std::move(batch));
}

void AutofillWalletMetadataSyncBridge::GetDataImpl(
    base::Optional<std::unordered_set<std::string>> storage_keys_set,
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<std::unique_ptr<CreditCard>> cards;
  if (!GetAutofillTable()->GetServerProfiles(&profiles) ||
      !GetAutofillTable()->GetServerCreditCards(&cards)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::unique_ptr<AutofillProfile>& entry : profiles) {
    std::string key = GetStorageKeyForMetadataId(entry->GetMetadata().id);
    if (!storage_keys_set || base::ContainsKey(*storage_keys_set, key)) {
      batch->Put(key,
                 CreateMetadataEntityDataFromAutofillServerProfile(*entry));
    }
  }
  for (const std::unique_ptr<CreditCard>& entry : cards) {
    std::string key = GetStorageKeyForMetadataId(entry->GetMetadata().id);
    if (!storage_keys_set || base::ContainsKey(*storage_keys_set, key)) {
      batch->Put(key, CreateMetadataEntityDataFromCard(*entry));
    }
  }

  std::move(callback).Run(std::move(batch));
}

}  // namespace autofill
