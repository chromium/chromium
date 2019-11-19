// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"

#include <utility>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"

using sync_pb::AutofillWalletSpecifics;
using syncer::EntityData;

namespace autofill {
namespace {

// Address to this variable used as the user data key.
static int kAutofillWalletSyncBridgeUserDataKey = 0;

std::string GetSpecificsIdFromAutofillWalletSpecifics(
    const AutofillWalletSpecifics& specifics) {
  switch (specifics.type()) {
    case AutofillWalletSpecifics::MASKED_CREDIT_CARD:
      return specifics.masked_card().id();
    case AutofillWalletSpecifics::POSTAL_ADDRESS:
      return specifics.address().id();
    case AutofillWalletSpecifics::CUSTOMER_DATA:
      return specifics.customer_data().id();
    case AutofillWalletSpecifics::CREDIT_CARD_CLOUD_TOKEN_DATA:
      return specifics.cloud_token_data().masked_card_id();
    case AutofillWalletSpecifics::UNKNOWN:
      NOTREACHED();
      return std::string();
  }
  return std::string();
}

std::string GetSpecificsIdFromAutofillProfile(const AutofillProfile& profile) {
  // Both server_id and specifics_id are _not_ base64 encoded.
  return profile.server_id();
}

std::string GetSpecificsIdFromCreditCard(const CreditCard& card) {
  // Both server_id and specifics_id are _not_ base64 encoded.
  return card.server_id();
}

std::string GetSpecificsIdFromPaymentsCustomerData(
    const PaymentsCustomerData& customer_data) {
  // Both customer_id and specifics_id are _not_ base64 encoded.
  return customer_data.customer_id;
}

// Returns the client tag for wallet data specifics id.
std::string GetClientTagForWalletDataSpecificsId(
    const std::string& specifics_id) {
  // Unlike for the wallet_metadata model type, the wallet_data expects
  // specifics id directly as client tags.
  return specifics_id;
}

// Returns the storage key to be used for wallet data for the specified wallet
// data |specifics_id|.
std::string GetStorageKeyForWalletDataSpecificsId(
    const std::string& specifics_id) {
  // We use the (non-base64-encoded) |specifics_id| directly as the storage key,
  // this function only hides this definition from all its call sites.
  return specifics_id;
}

// Creates a EntityData object corresponding to the specified |address|.
std::unique_ptr<EntityData> CreateEntityDataFromAutofillServerProfile(
    const AutofillProfile& address,
    bool enforce_utf8) {
  auto entity_data = std::make_unique<EntityData>();
  entity_data->name =
      "Server profile " +
      GetBase64EncodedId(GetSpecificsIdFromAutofillProfile(address));

  AutofillWalletSpecifics* wallet_specifics =
      entity_data->specifics.mutable_autofill_wallet();
  SetAutofillWalletSpecificsFromServerProfile(address, wallet_specifics,
                                              enforce_utf8);

  return entity_data;
}

// Creates a EntityData object corresponding to the specified |card|.
std::unique_ptr<EntityData> CreateEntityDataFromCard(const CreditCard& card,
                                                     bool enforce_utf8) {
  auto entity_data = std::make_unique<EntityData>();
  entity_data->name =
      "Server card " + GetBase64EncodedId(GetSpecificsIdFromCreditCard(card));

  AutofillWalletSpecifics* wallet_specifics =
      entity_data->specifics.mutable_autofill_wallet();
  SetAutofillWalletSpecificsFromServerCard(card, wallet_specifics,
                                           enforce_utf8);

  return entity_data;
}

// Creates a EntityData object corresponding to the specified |customer_data|.
std::unique_ptr<EntityData> CreateEntityDataFromPaymentsCustomerData(
    const PaymentsCustomerData& customer_data) {
  auto entity_data = std::make_unique<EntityData>();
  entity_data->name =
      "Payments customer data " +
      GetBase64EncodedId(GetSpecificsIdFromPaymentsCustomerData(customer_data));

  AutofillWalletSpecifics* wallet_specifics =
      entity_data->specifics.mutable_autofill_wallet();

  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     wallet_specifics);

  return entity_data;
}

}  // namespace

// static
void AutofillWalletSyncBridge::CreateForWebDataServiceAndBackend(
    const std::string& app_locale,
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillWalletSyncBridgeUserDataKey,
      std::make_unique<AutofillWalletSyncBridge>(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL_WALLET_DATA,
              /*dump_stack=*/base::RepeatingClosure()),
          web_data_backend));
}

// static
syncer::ModelTypeSyncBridge* AutofillWalletSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillWalletSyncBridgeUserDataKey));
}

AutofillWalletSyncBridge::AutofillWalletSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(web_data_backend) {
  DCHECK(web_data_backend_);

  LoadMetadata();
}

AutofillWalletSyncBridge::~AutofillWalletSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::AUTOFILL_WALLET_DATA);
}

base::Optional<syncer::ModelError> AutofillWalletSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // All metadata changes have been already written, return early for an error.
  base::Optional<syncer::ModelError> error =
      static_cast<syncer::SyncMetadataStoreChangeList*>(
          metadata_change_list.get())
          ->TakeError();
  if (error) {
    return error;
  }

  // We want to notify the metadata bridge about all changes so that the
  // metadata bridge can track changes in the data bridge and react accordingly.
  SetSyncData(entity_data, /*notify_metadata_bridge=*/true);

  // TODO(crbug.com/853688): Update the AutofillTable API to know about write
  // errors and report them here.
  return base::nullopt;
}

base::Optional<syncer::ModelError> AutofillWalletSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // This bridge does not support incremental updates, so whenever this is
  // called, the change list should be empty.
  DCHECK(entity_data.empty()) << "Received an unsupported incremental update.";
  return base::nullopt;
}

void AutofillWalletSyncBridge::GetData(StorageKeyList storage_keys,
                                       DataCallback callback) {
  // This data type is never synced "up" so we don't need to implement this.
  NOTIMPLEMENTED();
}

void AutofillWalletSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  GetAllDataImpl(std::move(callback), /*enforce_utf8=*/true);
}

std::string AutofillWalletSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_wallet());

  return GetClientTagForWalletDataSpecificsId(
      GetSpecificsIdFromAutofillWalletSpecifics(
          entity_data.specifics.autofill_wallet()));
}

std::string AutofillWalletSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_wallet());
  return GetStorageKeyForWalletDataSpecificsId(
      GetSpecificsIdFromAutofillWalletSpecifics(
          entity_data.specifics.autofill_wallet()));
}

bool AutofillWalletSyncBridge::SupportsIncrementalUpdates() const {
  // The payments server always returns the full dataset whenever there's any
  // change to the user's payments data. Therefore, we don't implement full
  // incremental-update support in this bridge, and clear all data
  // before inserting new instead.
  return false;
}

void AutofillWalletSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  // If a metadata change list gets passed in, that means sync is actually
  // disabled, so we want to delete the payments data.
  if (delete_metadata_change_list) {
    // Do not notify the metadata bridge because we do not want to upstream the
    // deletions. The metadata bridge deletes its data independently when sync
    // gets stopped.
    SetSyncData(syncer::EntityChangeList(), /*notify_metadata_bridge=*/false);
  }
}

void AutofillWalletSyncBridge::GetAllDataForTesting(DataCallback callback) {
  GetAllDataImpl(std::move(callback), /*enforce_utf8=*/false);
}

void AutofillWalletSyncBridge::GetAllDataImpl(DataCallback callback,
                                              bool enforce_utf8) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<std::unique_ptr<CreditCard>> cards;
  std::unique_ptr<PaymentsCustomerData> customer_data;
  if (!GetAutofillTable()->GetServerProfiles(&profiles) ||
      !GetAutofillTable()->GetServerCreditCards(&cards) ||
      !GetAutofillTable()->GetPaymentsCustomerData(&customer_data)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::unique_ptr<AutofillProfile>& entry : profiles) {
    batch->Put(GetStorageKeyForWalletDataSpecificsId(
                   GetSpecificsIdFromAutofillProfile(*entry)),
               CreateEntityDataFromAutofillServerProfile(*entry, enforce_utf8));
  }
  for (const std::unique_ptr<CreditCard>& entry : cards) {
    batch->Put(GetStorageKeyForWalletDataSpecificsId(
                   GetSpecificsIdFromCreditCard(*entry)),
               CreateEntityDataFromCard(*entry, enforce_utf8));
  }

  if (customer_data) {
    batch->Put(GetStorageKeyForWalletDataSpecificsId(
                   GetSpecificsIdFromPaymentsCustomerData(*customer_data)),
               CreateEntityDataFromPaymentsCustomerData(*customer_data));
  }
  std::move(callback).Run(std::move(batch));
}

void AutofillWalletSyncBridge::SetSyncData(
    const syncer::EntityChangeList& entity_data,
    bool notify_metadata_bridge) {
  bool wallet_data_changed = false;

  // Extract the Autofill types from the sync |entity_data|.
  std::vector<CreditCard> wallet_cards;
  std::vector<AutofillProfile> wallet_addresses;
  std::vector<PaymentsCustomerData> customer_data;
  PopulateWalletTypesFromSyncData(entity_data, &wallet_cards, &wallet_addresses,
                                  &customer_data);

  wallet_data_changed |= SetPaymentsCustomerData(std::move(customer_data));
  wallet_data_changed |=
      SetWalletCards(std::move(wallet_cards), notify_metadata_bridge);
  wallet_data_changed |=
      SetWalletAddresses(std::move(wallet_addresses), notify_metadata_bridge);

  // Commit the transaction to make sure the data and the metadata with the
  // new progress marker is written down (especially on Android where we
  // cannot rely on commiting transactions on shutdown). We need to commit
  // even if the wallet data has not changed because the model type state incl.
  // the progress marker always changes.
  web_data_backend_->CommitChanges();

  if (web_data_backend_ && wallet_data_changed)
    web_data_backend_->NotifyOfMultipleAutofillChanges();
}

bool AutofillWalletSyncBridge::SetWalletCards(
    std::vector<CreditCard> wallet_cards,
    bool notify_metadata_bridge) {
  // Users can set billing address of the server credit card locally, but that
  // information does not propagate to either Chrome Sync or Google Payments
  // server. To preserve user's preferred billing address and most recent use
  // stats, copy them from disk into |wallet_cards|.
  AutofillTable* table = GetAutofillTable();
  CopyRelevantWalletMetadataFromDisk(*table, &wallet_cards);

  // In the common case, the database won't have changed. Committing an update
  // to the database will require at least one DB page write and will schedule
  // a fsync. To avoid this I/O, it should be more efficient to do a read and
  // only do the writes if something changed.
  std::vector<std::unique_ptr<CreditCard>> existing_cards;
  table->GetServerCreditCards(&existing_cards);
  AutofillWalletDiff<CreditCard> diff =
      ComputeAutofillWalletDiff(existing_cards, wallet_cards);

  if (!diff.IsEmpty()) {
    table->SetServerCardsData(wallet_cards);
    if (notify_metadata_bridge) {
      for (const CreditCardChange& change : diff.changes) {
        web_data_backend_->NotifyOfCreditCardChanged(change);
      }
    }
    return true;
  }
  return false;
}

bool AutofillWalletSyncBridge::SetWalletAddresses(
    std::vector<AutofillProfile> wallet_addresses,
    bool notify_metadata_bridge) {
  // We do not have to CopyRelevantWalletMetadataFromDisk() because we will
  // never overwrite the same entity with different data (server_id is generated
  // based on content so addresses have the same server_id iff they have the
  // same content). For that reason it is impossible to issue a DELETE and ADD
  // for the same entity just because some of its fields got changed. As a
  // result, we do not need to care to have up-to-date use stats for cards
  // because we never notify on an existing one.

  // In the common case, the database won't have changed. Committing an update
  // to the database will require at least one DB page write and will schedule
  // a fsync. To avoid this I/O, it should be more efficient to do a read and
  // only do the writes if something changed.
  AutofillTable* table = GetAutofillTable();
  std::vector<std::unique_ptr<AutofillProfile>> existing_addresses;
  table->GetServerProfiles(&existing_addresses);
  AutofillWalletDiff<AutofillProfile> diff =
      ComputeAutofillWalletDiff(existing_addresses, wallet_addresses);

  if (!diff.IsEmpty()) {
    table->SetServerAddressesData(wallet_addresses);
    if (notify_metadata_bridge) {
      for (const AutofillProfileChange& change : diff.changes) {
        web_data_backend_->NotifyOfAutofillProfileChanged(change);
      }
    }
    return true;
  }
  return false;
}

bool AutofillWalletSyncBridge::SetPaymentsCustomerData(
    std::vector<PaymentsCustomerData> customer_data) {
  AutofillTable* table = GetAutofillTable();
  std::unique_ptr<PaymentsCustomerData> existing_entry;
  table->GetPaymentsCustomerData(&existing_entry);

  // In case there were multiple entries (and there shouldn't!), we take the
  // pointer to the first entry in the vector.
  PaymentsCustomerData* new_entry =
      customer_data.empty() ? nullptr : customer_data.data();

#if DCHECK_IS_ON()
  if (customer_data.size() > 1) {
    DLOG(WARNING) << "Sync wallet_data update has " << customer_data.size()
                  << " payments-customer-data entries; expected 0 or 1.";
  }
#endif  // DCHECK_IS_ON()

  if (!new_entry && existing_entry) {
    // Clear the existing entry in the DB.
    GetAutofillTable()->SetPaymentsCustomerData(nullptr);
    return true;
  } else if (new_entry && (!existing_entry || *new_entry != *existing_entry)) {
    // Write the new entry in the DB as it differs from the existing one.
    GetAutofillTable()->SetPaymentsCustomerData(new_entry);
    return true;
  }
  return false;
}

template <class Item>
AutofillWalletSyncBridge::AutofillWalletDiff<Item>
AutofillWalletSyncBridge::ComputeAutofillWalletDiff(
    const std::vector<std::unique_ptr<Item>>& old_data,
    const std::vector<Item>& new_data) {
  // Build vectors of pointers, so that we can mutate (sort) them.
  std::vector<const Item*> old_ptrs;
  old_ptrs.reserve(old_data.size());
  for (const std::unique_ptr<Item>& old_item : old_data)
    old_ptrs.push_back(old_item.get());
  std::vector<const Item*> new_ptrs;
  new_ptrs.reserve(new_data.size());
  for (const Item& new_item : new_data)
    new_ptrs.push_back(&new_item);

  // Sort our vectors.
  auto compare = [](const Item* lhs, const Item* rhs) {
    return lhs->Compare(*rhs) < 0;
  };
  std::sort(old_ptrs.begin(), old_ptrs.end(), compare);
  std::sort(new_ptrs.begin(), new_ptrs.end(), compare);

  AutofillWalletDiff<Item> result;
  // We collect ADD changes separately to ensure proper order.
  std::vector<AutofillDataModelChange<Item>> add_changes;

  // Walk over both of them and count added/removed elements.
  auto old_it = old_ptrs.begin();
  auto new_it = new_ptrs.begin();
  while (old_it != old_ptrs.end() || new_it != new_ptrs.end()) {
    int cmp;
    if (old_it != old_ptrs.end() && new_it != new_ptrs.end()) {
      cmp = (*old_it)->Compare(**new_it);
    } else if (new_it == new_ptrs.end()) {
      cmp = -1;  // At the end of new items, *old_it needs to get removed.
    } else {
      cmp = 1;  // At the end of old items, *new_it needs to get added.
    }

    if (cmp < 0) {
      ++result.items_removed;
      result.changes.emplace_back(AutofillDataModelChange<Item>::REMOVE,
                                  (*old_it)->server_id(), *old_it);
      ++old_it;
    } else if (cmp == 0) {
      ++old_it;
      ++new_it;
    } else {
      ++result.items_added;
      add_changes.emplace_back(AutofillDataModelChange<Item>::ADD,
                               (*new_it)->server_id(), *new_it);
      ++new_it;
    }
  }

  // Append ADD changes to make sure they all come after all REMOVE changes.
  // Since we CopyRelevantWalletMetadataFromDisk(), the ADD contains all current
  // metadata if we happen to REMOVE and ADD the same entity.
  result.changes.insert(result.changes.end(), add_changes.begin(),
                        add_changes.end());

  DCHECK_EQ(old_data.size() + result.items_added - result.items_removed,
            new_data.size());

  return result;
}

AutofillTable* AutofillWalletSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

void AutofillWalletSyncBridge::LoadMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetAutofillTable()->GetAllSyncMetadata(syncer::AUTOFILL_WALLET_DATA,
                                              batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed reading autofill metadata from WebDatabase."});
    return;
  }

  change_processor()->ModelReadyToSync(std::move(batch));
}

}  // namespace autofill
