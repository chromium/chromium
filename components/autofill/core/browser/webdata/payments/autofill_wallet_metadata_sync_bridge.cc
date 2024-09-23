// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/autofill_wallet_metadata_sync_bridge.h"

#include <map>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/payments_metadata.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_util.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {

namespace {

using sync_pb::WalletMetadataSpecifics;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::MetadataChangeList;

// Address to this variable used as the user data key.
static int kAutofillWalletMetadataSyncBridgeUserDataKey = 0;

std::string GetClientTagForSpecificsId(WalletMetadataSpecifics::Type type,
                                       const std::string& specifics_id) {
  switch (type) {
    case WalletMetadataSpecifics::ADDRESS:
      // TODO(crbug.com/40273491): Even though the server-side drops
      // writes for WalletMetadataSpecifics::ADDRESS, old data wasn't cleaned
      // up yet. As such, this code is still reachable.
      return "address-" + specifics_id;
    case WalletMetadataSpecifics::CARD:
      return "card-" + specifics_id;
    case WalletMetadataSpecifics::IBAN:
      return "iban-" + specifics_id;
    case WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

// Returns the wallet metadata specifics id for the specified |metadata_id|.
std::string GetSpecificsIdForMetadataId(const std::string& metadata_id) {
  // Metadata id is in the raw format (like cards from WalletData) whereas the
  // specifics id is base64-encoded.
  return base::Base64Encode(metadata_id);
}

// Returns the wallet metadata id for the specified |specifics_id|.
std::string GetMetadataIdForSpecificsId(const std::string& specifics_id) {
  // The specifics id is base64-encoded whereas the metadata id is in the raw
  // format (like cards from WalletData).
  std::string decoded_id;
  base::Base64Decode(specifics_id, &decoded_id);
  return decoded_id;
}

// Returns the wallet metadata specifics storage key for the specified |type|
// and |metadata_id|.
std::string GetStorageKeyForWalletMetadataTypeAndId(
    WalletMetadataSpecifics::Type type,
    const std::string& metadata_id) {
  return GetStorageKeyForWalletMetadataTypeAndSpecificsId(
      type, GetSpecificsIdForMetadataId(metadata_id));
}

struct TypeAndMetadataId {
  WalletMetadataSpecifics::Type type;
  std::string metadata_id;
};

TypeAndMetadataId ParseWalletMetadataStorageKey(
    const std::string& storage_key) {
  base::Pickle pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(storage_key));
  base::PickleIterator iterator(pickle);
  int type_int;
  std::string specifics_id;
  if (!iterator.ReadInt(&type_int) || !iterator.ReadString(&specifics_id)) {
    NOTREACHED_IN_MIGRATION()
        << "Unsupported storage_key provided " << storage_key;
  }

  TypeAndMetadataId parsed;
  parsed.type = static_cast<WalletMetadataSpecifics::Type>(type_int);
  parsed.metadata_id = GetMetadataIdForSpecificsId(specifics_id);
  return parsed;
}

// Returns EntityData for wallet_metadata for |local_metadata| and |type|.
std::unique_ptr<EntityData> CreateEntityDataFromPaymentsMetadata(
    WalletMetadataSpecifics::Type type,
    const PaymentsMetadata& local_metadata) {
  auto entity_data = std::make_unique<EntityData>();
  std::string specifics_id = GetSpecificsIdForMetadataId(local_metadata.id);
  entity_data->name = GetClientTagForSpecificsId(type, specifics_id);

  WalletMetadataSpecifics* remote_metadata =
      entity_data->specifics.mutable_wallet_metadata();
  remote_metadata->set_type(type);
  remote_metadata->set_id(specifics_id);
  remote_metadata->set_use_count(local_metadata.use_count);
  remote_metadata->set_use_date(
      local_metadata.use_date.ToDeltaSinceWindowsEpoch().InMicroseconds());

  if (type == WalletMetadataSpecifics::CARD) {
    // The strings must be in valid UTF-8 to sync.
    remote_metadata->set_card_billing_address_id(
        base::Base64Encode(local_metadata.billing_address_id));
  }

  return entity_data;
}

// Returns PaymentsMetadata for |specifics|.
PaymentsMetadata CreatePaymentsMetadataFromWalletMetadataSpecifics(
    const WalletMetadataSpecifics& specifics) {
  PaymentsMetadata metadata;
  metadata.id = GetMetadataIdForSpecificsId(specifics.id());
  metadata.use_count = specifics.use_count();
  metadata.use_date = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.use_date()));

  if (specifics.type() == WalletMetadataSpecifics::CARD) {
    base::Base64Decode(specifics.card_billing_address_id(),
                       &metadata.billing_address_id);
  }

  return metadata;
}

bool HasLocalBillingAddress(const PaymentsMetadata& metadata) {
  return metadata.billing_address_id.size() == kLocalGuidSize;
}

bool IsNewerBillingAddressEqualOrBetter(const PaymentsMetadata& older,
                                        const PaymentsMetadata& newer) {
  // If older is empty, newer is better (or equal). Otherwise, if newer is
  // empty, older is better.
  if (older.billing_address_id.empty()) {
    return true;
  } else if (newer.billing_address_id.empty()) {
    return false;
  }
  // Now we need to decide between non-empty profiles. Prefer id's pointing to
  // local profiles over ids of non-local profiles.
  if (HasLocalBillingAddress(older) != HasLocalBillingAddress(newer)) {
    return HasLocalBillingAddress(newer);
  }
  // For both older / both newer, we prefer the more recently used.
  return newer.use_date >= older.use_date;
}

PaymentsMetadata MergeMetadata(WalletMetadataSpecifics::Type type,
                               const PaymentsMetadata& local,
                               const PaymentsMetadata& remote) {
  PaymentsMetadata merged;
  DCHECK_EQ(local.id, remote.id);
  merged.id = local.id;

  if (type == WalletMetadataSpecifics::CARD) {
    if (IsNewerBillingAddressEqualOrBetter(/*older=*/local,
                                           /*newer=*/remote)) {
      merged.billing_address_id = remote.billing_address_id;
    } else {
      merged.billing_address_id = local.billing_address_id;
    }
  }

  // Special case for local models with a use_count of one. This means the local
  // model was only created, never used. The remote model should always be
  // preferred.
  // This situation can happen for new Chromium instances where there is no data
  // yet on disk, making the use_date artificially high. Once the metadata sync
  // kicks in, we should use that value.
  if (local.use_count == 1) {
    merged.use_count = remote.use_count;
    merged.use_date = remote.use_date;
  } else {
    merged.use_count = std::max(local.use_count, remote.use_count);
    merged.use_date = std::max(local.use_date, remote.use_date);
  }
  return merged;
}

// Metadata is worth updating if its value is "newer" then before; here "newer"
// is the ordering of legal state transitions that metadata can take that is
// defined below.
bool IsMetadataWorthUpdating(PaymentsMetadata existing_entry,
                             PaymentsMetadata new_entry) {
  if (existing_entry.use_count < new_entry.use_count &&
      existing_entry.use_date < new_entry.use_date) {
    return true;
  }
  // For the following type-specific fields, we don't have to distinguish the
  // type of metadata as both entries must be of the same type and therefore
  // irrelevant values are default, thus equal.
  if (existing_entry.billing_address_id != new_entry.billing_address_id &&
      IsNewerBillingAddressEqualOrBetter(/*older=*/existing_entry,
                                         /*newer=*/new_entry)) {
    return true;
  }
  return false;
}

bool IsAnyMetadataDeletable(
    const std::map<std::string, PaymentsMetadata>& metadata_map) {
  for (const auto& [storage_key, metadata] : metadata_map) {
    if (metadata.IsDeletable()) {
      return true;
    }
  }
  return false;
}

bool AddServerMetadata(PaymentsAutofillTable* table,
                       WalletMetadataSpecifics::Type type,
                       const PaymentsMetadata& metadata) {
  switch (type) {
    case WalletMetadataSpecifics::CARD:
      return table->AddServerCardMetadata(metadata);
    case WalletMetadataSpecifics::IBAN:
      return table->AddOrUpdateServerIbanMetadata(metadata);
    // ADDRESS metadata syncing is deprecated.
    case WalletMetadataSpecifics::ADDRESS:
    case WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool RemoveServerMetadata(PaymentsAutofillTable* table,
                          WalletMetadataSpecifics::Type type,
                          const std::string& id) {
  switch (type) {
    case WalletMetadataSpecifics::CARD:
      return table->RemoveServerCardMetadata(id);
    case WalletMetadataSpecifics::IBAN:
      return table->RemoveServerIbanMetadata(id);
    // ADDRESS metadata syncing is deprecated.
    case WalletMetadataSpecifics::ADDRESS:
    case WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool UpdateServerMetadata(PaymentsAutofillTable* table,
                          WalletMetadataSpecifics::Type type,
                          const PaymentsMetadata& metadata) {
  switch (type) {
    case WalletMetadataSpecifics::CARD:
      return table->UpdateServerCardMetadata(metadata);
    case WalletMetadataSpecifics::IBAN:
      return table->AddOrUpdateServerIbanMetadata(metadata);
    // ADDRESS metadata syncing is deprecated.
    case WalletMetadataSpecifics::ADDRESS:
    case WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool IsSyncedWalletCard(const CreditCard& card) {
  switch (card.record_type()) {
    case CreditCard::RecordType::kLocalCard:
      return false;
    case CreditCard::RecordType::kMaskedServerCard:
      return true;
    case CreditCard::RecordType::kFullServerCard:
      return false;
    case CreditCard::RecordType::kVirtualCard:
      return false;
  }
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
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::AUTOFILL_WALLET_METADATA,
              /*dump_stack=*/base::DoNothing()),
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
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : DataTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(web_data_backend) {
  DCHECK(web_data_backend_);
  scoped_observation_.Observe(web_data_backend_.get());

  LoadDataCacheAndMetadata();

  DeleteOldOrphanMetadata();
}

AutofillWalletMetadataSyncBridge::~AutofillWalletMetadataSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletMetadataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetSyncMetadataStore(), syncer::AUTOFILL_WALLET_METADATA,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError>
AutofillWalletMetadataSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // First upload local entities that are not mentioned in |entity_data|.
  // Because Wallet Metadata is deleted when Sync (for this data type) is turned
  // off, there should usually not be any pre-existing local data here, but it
  // can happen in some corner cases such as when PDM manages to change metadata
  // during the initial sync procedure (e.g. the remote sync data was just
  // downloaded, but first passed to the AUTOFILL_WALLET bridge, with the side
  // effect of creating wallet metadata entries immediately before this function
  // is invoked).
  UploadInitialLocalData(metadata_change_list.get(), entity_data);

  return MergeRemoteChanges(std::move(metadata_change_list),
                            std::move(entity_data));
}

std::optional<syncer::ModelError>
AutofillWalletMetadataSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return MergeRemoteChanges(std::move(metadata_change_list),
                            std::move(entity_data));
}

std::unique_ptr<syncer::DataBatch>
AutofillWalletMetadataSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  // Build a set out of the list to allow quick lookup.
  std::unordered_set<std::string> storage_keys_set(storage_keys.begin(),
                                                   storage_keys.end());
  return GetDataImpl(std::move(storage_keys_set));
}

std::unique_ptr<syncer::DataBatch>
AutofillWalletMetadataSyncBridge::GetAllDataForDebugging() {
  // Get all data by not providing any |storage_keys| filter.
  return GetDataImpl(/*storage_keys_set=*/std::nullopt);
}

std::string AutofillWalletMetadataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const WalletMetadataSpecifics& remote_metadata =
      entity_data.specifics.wallet_metadata();
  return GetClientTagForSpecificsId(remote_metadata.type(),
                                    remote_metadata.id());
}

std::string AutofillWalletMetadataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetStorageKeyForWalletMetadataTypeAndSpecificsId(
      entity_data.specifics.wallet_metadata().type(),
      entity_data.specifics.wallet_metadata().id());
}

void AutofillWalletMetadataSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  // Sync is disabled so we want to delete the data as well (i.e. the wallet
  // metadata entities).
  for (const auto& [storage_key, metadata] : cache_) {
    TypeAndMetadataId parsed_storage_key =
        ParseWalletMetadataStorageKey(storage_key);
    RemoveServerMetadata(GetAutofillTable(), parsed_storage_key.type,
                         parsed_storage_key.metadata_id);
  }
  cache_.clear();

  // We do not notify the change to the UI because the data bridge will notify
  // anyway and notifying on metadata deletion potentially before the data
  // deletion is risky.

  // Commit the transaction to make sure the sync data (deleted here) and the
  // sync metadata and the progress marker (deleted by the processor via
  // |delete_metadata_change_list|) get wiped from the DB. This is especially
  // important on Android where we cannot rely on committing transactions on
  // shutdown).
  web_data_backend_->CommitChanges();
}

void AutofillWalletMetadataSyncBridge::CreditCardChanged(
    const CreditCardChange& change) {
  // TODO(crbug.com/40765031): Clean up old metadata for local cards, this early
  // return was missing for quite a while in production.
  if (!IsSyncedWalletCard(change.data_model())) {
    return;
  }
  LocalMetadataChanged(WalletMetadataSpecifics::CARD, change);
}

void AutofillWalletMetadataSyncBridge::IbanChanged(const IbanChange& change) {
  if (change.data_model().record_type() != Iban::RecordType::kServerIban) {
    return;
  }
  LocalMetadataChanged(WalletMetadataSpecifics::IBAN, change);
}

PaymentsAutofillTable* AutofillWalletMetadataSyncBridge::GetAutofillTable() {
  return PaymentsAutofillTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

AutofillSyncMetadataTable*
AutofillWalletMetadataSyncBridge::GetSyncMetadataStore() {
  return AutofillSyncMetadataTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

void AutofillWalletMetadataSyncBridge::LoadDataCacheAndMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable() || !GetSyncMetadataStore()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }

  // Load the data cache.
  std::vector<PaymentsMetadata> cards_metadata;
  std::vector<PaymentsMetadata> ibans_metadata;
  if (!GetAutofillTable()->GetServerCardsMetadata(cards_metadata) ||
      !GetAutofillTable()->GetServerIbansMetadata(ibans_metadata)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed reading autofill data from WebDatabase."});
    return;
  }
  for (const PaymentsMetadata& card_metadata : cards_metadata) {
    cache_[GetStorageKeyForWalletMetadataTypeAndId(
        WalletMetadataSpecifics::CARD, card_metadata.id)] = card_metadata;
  }

  for (const PaymentsMetadata& iban_metadata : ibans_metadata) {
    cache_[GetStorageKeyForWalletMetadataTypeAndId(
        WalletMetadataSpecifics::IBAN, iban_metadata.id)] = iban_metadata;
  }

  // Load the metadata and send to the processor.
  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetSyncMetadataStore()->GetAllSyncMetadata(
          syncer::AUTOFILL_WALLET_METADATA, batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed reading autofill metadata from WebDatabase."});
    return;
  }

  change_processor()->ModelReadyToSync(std::move(batch));
}

void AutofillWalletMetadataSyncBridge::DeleteOldOrphanMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable()) {
    // We have a problem with the database, not an issue, we clean up next time.
    return;
  }
  if (!IsAnyMetadataDeletable(cache_)) {
    return;
  }

  // Load up (metadata) ids for which data exists; we do not delete those.
  std::unordered_set<std::string> non_orphan_ids;
  std::vector<std::unique_ptr<CreditCard>> cards;
  std::vector<std::unique_ptr<Iban>> ibans;
  if (!GetAutofillTable()->GetServerCreditCards(cards) ||
      !GetAutofillTable()->GetServerIbans(ibans)) {
    return;
  }

  non_orphan_ids.reserve(cards.size() + ibans.size());
  for (const std::unique_ptr<CreditCard>& card : cards) {
    non_orphan_ids.insert(card->server_id());
  }

  for (const std::unique_ptr<Iban>& iban : ibans) {
    non_orphan_ids.insert(base::NumberToString(iban->instrument_id()));
  }

  // Identify storage keys of old orphans (we delete them below to avoid
  // modifying |cache_| while iterating).
  std::unordered_set<std::string> old_orphan_keys;
  for (const auto& [storage_key, metadata] : cache_) {
    if (metadata.IsDeletable() && !non_orphan_ids.contains(metadata.id)) {
      old_orphan_keys.insert(storage_key);
    }
  }

  if (old_orphan_keys.empty()) {
    return;
  }

  std::unique_ptr<MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  for (const std::string& storage_key : old_orphan_keys) {
    TypeAndMetadataId parsed_storage_key =
        ParseWalletMetadataStorageKey(storage_key);
    if (RemoveServerMetadata(GetAutofillTable(), parsed_storage_key.type,
                             parsed_storage_key.metadata_id)) {
      cache_.erase(storage_key);
      change_processor()->Delete(storage_key,
                                 syncer::DeletionOrigin::Unspecified(),
                                 metadata_change_list.get());
    }
  }

  // Commit the transaction to make sure the data and the metadata is written
  // down (especially on Android where we cannot rely on committing transactions
  // on shutdown).
  web_data_backend_->CommitChanges();

  // We do not need to NotifyOnAutofillChangedBySync() because this change is
  // invisible for PersonalDataManager - it does not change metadata for any
  // existing data.
}

std::unique_ptr<syncer::DataBatch>
AutofillWalletMetadataSyncBridge::GetDataImpl(
    std::optional<std::unordered_set<std::string>> storage_keys_set) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const auto& [storage_key, metadata] : cache_) {
    TypeAndMetadataId parsed_storage_key =
        ParseWalletMetadataStorageKey(storage_key);
    if (!storage_keys_set || storage_keys_set->contains(storage_key)) {
      batch->Put(storage_key, CreateEntityDataFromPaymentsMetadata(
                                  parsed_storage_key.type, metadata));
    }
  }

  return batch;
}

void AutofillWalletMetadataSyncBridge::UploadInitialLocalData(
    syncer::MetadataChangeList* metadata_change_list,
    const syncer::EntityChangeList& entity_data) {
  // First, make a copy of all local storage keys.
  std::set<std::string> local_keys_to_upload;
  for (const auto& [storage_key, metadata] : cache_) {
    local_keys_to_upload.insert(storage_key);
  }
  // Strip |local_keys_to_upload| of the keys of data provided by the server.
  for (const std::unique_ptr<EntityChange>& change : entity_data) {
    DCHECK_EQ(change->type(), EntityChange::ACTION_ADD)
        << "Illegal change; can only be called during initial "
           "MergeFullSyncData()";
    local_keys_to_upload.erase(change->storage_key());
  }
  // Upload the remaining storage keys
  for (const std::string& storage_key : local_keys_to_upload) {
    TypeAndMetadataId parsed_storage_key =
        ParseWalletMetadataStorageKey(storage_key);
    change_processor()->Put(storage_key,
                            CreateEntityDataFromPaymentsMetadata(
                                parsed_storage_key.type, cache_[storage_key]),
                            metadata_change_list);
  }
}

std::optional<syncer::ModelError>
AutofillWalletMetadataSyncBridge::MergeRemoteChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  bool is_any_local_modified = false;

  PaymentsAutofillTable* table = GetAutofillTable();

  for (const std::unique_ptr<EntityChange>& change : entity_data) {
    TypeAndMetadataId parsed_storage_key =
        ParseWalletMetadataStorageKey(change->storage_key());
    if (parsed_storage_key.type == WalletMetadataSpecifics::ADDRESS) {
      // TODO(crbug.com/40273491): Even though the server-side drops
      // writes for WalletMetadataSpecifics::ADDRESS, old data wasn't cleaned
      // up yet. As such, this code is still reachable.
      continue;
    }
    switch (change->type()) {
      case EntityChange::ACTION_ADD:
      case EntityChange::ACTION_UPDATE: {
        const WalletMetadataSpecifics& specifics =
            change->data().specifics.wallet_metadata();
        PaymentsMetadata remote =
            CreatePaymentsMetadataFromWalletMetadataSpecifics(specifics);
        auto it = cache_.find(change->storage_key());
        std::optional<PaymentsMetadata> local = std::nullopt;
        if (it != cache_.end()) {
          local = it->second;
        }

        if (!local) {
          cache_[change->storage_key()] = remote;
          is_any_local_modified |= AddServerMetadata(
              GetAutofillTable(), parsed_storage_key.type, remote);
          continue;
        }

        // Resolve the conflict between the local and the newly received remote.
        PaymentsMetadata merged =
            MergeMetadata(parsed_storage_key.type, *local, remote);
        if (merged != *local) {
          cache_[change->storage_key()] = merged;
          is_any_local_modified |=
              UpdateServerMetadata(table, parsed_storage_key.type, merged);
        }
        if (merged != remote) {
          change_processor()->Put(change->storage_key(),
                                  CreateEntityDataFromPaymentsMetadata(
                                      parsed_storage_key.type, merged),
                                  metadata_change_list.get());
        }
        break;
      }
      case EntityChange::ACTION_DELETE: {
        // We intentionally ignore remote deletions in order to avoid
        // delete-create ping pongs.
        // This is safe because this client will delete the wallet_metadata
        // entity locally as soon as the wallet_data entity gets deleted.
        // Corner cases are handled by DeleteOldOrphanMetadata().
        break;
      }
    }
  }

  // Commit the transaction to make sure the data and the metadata with the
  // new progress marker is written down (especially on Android where we
  // cannot rely on committing transactions on shutdown). We need to commit
  // even if !|is_any_local_modified| because the data type state or local
  // metadata may have changed.
  web_data_backend_->CommitChanges();

  if (is_any_local_modified) {
    web_data_backend_->NotifyOnAutofillChangedBySync(
        syncer::AUTOFILL_WALLET_METADATA);
  }

  return change_processor()->GetError();
}

template <typename DataType, typename KeyType>
void AutofillWalletMetadataSyncBridge::LocalMetadataChanged(
    WalletMetadataSpecifics::Type type,
    AutofillDataModelChange<DataType, KeyType> change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40927747): Conversion logic is necessary once credit cards
  // have migrated to use instrument IDs, then the branching can be removed.
  std::string metadata_id;
  if constexpr (std::same_as<DataType, Iban>) {
    metadata_id = base::NumberToString(absl::get<int64_t>(change.key()));
  } else {
    metadata_id = change.key();
  }

  std::string storage_key =
      GetStorageKeyForWalletMetadataTypeAndId(type, metadata_id);
  std::unique_ptr<MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  switch (change.type()) {
    case AutofillDataModelChange<DataType, KeyType>::REMOVE:
      if (RemoveServerMetadata(GetAutofillTable(), type, metadata_id)) {
        cache_.erase(storage_key);
        // Send up deletion only if we had this entry in the DB. It is not there
        // if it was previously deleted by a remote deletion.
        change_processor()->Delete(storage_key,
                                   syncer::DeletionOrigin::Unspecified(),
                                   metadata_change_list.get());
      }
      return;
    case AutofillDataModelChange<DataType, KeyType>::ADD:
    case AutofillDataModelChange<DataType, KeyType>::UPDATE:
      PaymentsMetadata new_entry = change.data_model().GetMetadata();
      auto it = cache_.find(storage_key);
      std::optional<PaymentsMetadata> existing_entry = std::nullopt;
      if (it != cache_.end()) {
        existing_entry = it->second;
      }

      if (existing_entry &&
          !IsMetadataWorthUpdating(*existing_entry, new_entry)) {
        // Skip changes that are outdated, etc. (changes that would result in
        // inferior metadata compared to what we have now).
        return;
      }

      cache_[storage_key] = new_entry;
      if (existing_entry) {
        UpdateServerMetadata(GetAutofillTable(), type, new_entry);
      } else {
        AddServerMetadata(GetAutofillTable(), type, new_entry);
      }

      change_processor()->Put(
          storage_key, CreateEntityDataFromPaymentsMetadata(type, new_entry),
          metadata_change_list.get());
      return;
  }

  // We do not need to commit any local changes (written by the processor via
  // the metadata change list) because the open WebDatabase transaction is
  // committed by the AutofillWebDataService when the original local write
  // operation (that triggered this notification to the bridge) finishes.
}

}  // namespace autofill
