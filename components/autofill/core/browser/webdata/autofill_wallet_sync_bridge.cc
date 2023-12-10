// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/entity_data.h"

using sync_pb::AutofillWalletSpecifics;
using syncer::EntityData;

namespace autofill {
namespace {

// Address to this variable used as the user data key.
static int kAutofillWalletSyncBridgeUserDataKey = 0;

std::string GetClientTagFromCreditCard(const CreditCard& card) {
  // Both server_id and client_tag are _not_ base64 encoded.
  return card.server_id();
}

std::string GetClientTagFromPaymentsCustomerData(
    const PaymentsCustomerData& customer_data) {
  // Both customer_id and client_tag are _not_ base64 encoded.
  return customer_data.customer_id;
}

std::string GetClientTagFromCreditCardCloudTokenData(
    const CreditCardCloudTokenData& cloud_token_data) {
  return cloud_token_data.instrument_token;
}

std::string GetClientTagFromIban(const Iban& iban) {
  return base::NumberToString(iban.instrument_id());
}

// Returns the storage key to be used for wallet data for the specified wallet
// data |client_tag|.
std::string GetStorageKeyForWalletDataClientTag(const std::string& client_tag) {
  // We use the (non-base64-encoded) |client_tag| directly as the storage key,
  // this function only hides this definition from all its call sites.
  return client_tag;
}

// Creates a EntityData object corresponding to the specified |card|.
std::unique_ptr<EntityData> CreateEntityDataFromCard(const CreditCard& card,
                                                     bool enforce_utf8) {
  auto entity_data = std::make_unique<EntityData>();
  entity_data->name =
      "Server card " + GetBase64EncodedId(GetClientTagFromCreditCard(card));

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
      GetBase64EncodedId(GetClientTagFromPaymentsCustomerData(customer_data));

  AutofillWalletSpecifics* wallet_specifics =
      entity_data->specifics.mutable_autofill_wallet();

  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     wallet_specifics);

  return entity_data;
}

// Creates a EntityData object corresponding to the specified
// |cloud_token_data|.
std::unique_ptr<EntityData> CreateEntityDataFromCreditCardCloudTokenData(
    const CreditCardCloudTokenData& cloud_token_data,
    bool enforce_utf8) {
  auto entity_data = std::make_unique<EntityData>();
  entity_data->name =
      "Server card cloud token data " +
      GetBase64EncodedId(
          GetClientTagFromCreditCardCloudTokenData(cloud_token_data));

  AutofillWalletSpecifics* wallet_specifics =
      entity_data->specifics.mutable_autofill_wallet();
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      cloud_token_data, wallet_specifics, enforce_utf8);
  return entity_data;
}

// Creates a EntityData object corresponding to the specified `iban`.
std::unique_ptr<EntityData> CreateEntityDataFromIban(const Iban& iban,
                                                     bool enforce_utf8) {
  auto entity_data = std::make_unique<EntityData>();
  entity_data->name =
      "Server IBAN " + GetBase64EncodedId(GetClientTagFromIban(iban));

  AutofillWalletSpecifics* wallet_specifics =
      entity_data->specifics.mutable_autofill_wallet();
  SetAutofillWalletSpecificsFromMaskedIban(iban, wallet_specifics,
                                           enforce_utf8);
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
              /*dump_stack=*/base::DoNothing()),
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
      GetAutofillTable(), syncer::AUTOFILL_WALLET_DATA,
      base::BindRepeating(&syncer::ModelTypeChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

absl::optional<syncer::ModelError> AutofillWalletSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // We want to notify the metadata bridge about all changes so that the
  // metadata bridge can track changes in the data bridge and react accordingly.
  SetSyncData(entity_data, /*notify_webdata_backend=*/true);

  // TODO(crbug.com/853688): Update the AutofillTable API to know about write
  // errors and report them here.
  return absl::nullopt;
}

absl::optional<syncer::ModelError>
AutofillWalletSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // This bridge does not support incremental updates, so whenever this is
  // called, the change list should be empty.
  DCHECK(entity_data.empty()) << "Received an unsupported incremental update.";
  return absl::nullopt;
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

  return syncer::GetUnhashedClientTagFromAutofillWalletSpecifics(
      entity_data.specifics.autofill_wallet());
}

std::string AutofillWalletSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_wallet());
  return GetStorageKeyForWalletDataClientTag(
      syncer::GetUnhashedClientTagFromAutofillWalletSpecifics(
          entity_data.specifics.autofill_wallet()));
}

bool AutofillWalletSyncBridge::SupportsIncrementalUpdates() const {
  // The payments server always returns the full dataset whenever there's any
  // change to the user's payments data. Therefore, we don't implement full
  // incremental-update support in this bridge, and clear all data
  // before inserting new instead.
  return false;
}

void AutofillWalletSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  // Sync is disabled, so we want to delete the payments data.

  // Do not notify the metadata bridge because we do not want to upstream the
  // deletions. The metadata bridge deletes its data independently when sync
  // gets stopped.
  SetSyncData(syncer::EntityChangeList(), /*notify_webdata_backend=*/false);
}

void AutofillWalletSyncBridge::GetAllDataForTesting(DataCallback callback) {
  GetAllDataImpl(std::move(callback), /*enforce_utf8=*/false);
}

void AutofillWalletSyncBridge::GetAllDataImpl(DataCallback callback,
                                              bool enforce_utf8) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<CreditCard>> cards;
  std::vector<std::unique_ptr<Iban>> ibans;
  std::vector<std::unique_ptr<CreditCardCloudTokenData>> cloud_token_data;
  std::unique_ptr<PaymentsCustomerData> customer_data;
  if (!GetAutofillTable()->GetServerCreditCards(cards) ||
      !GetAutofillTable()->GetServerIbans(ibans) ||
      !GetAutofillTable()->GetCreditCardCloudTokenData(cloud_token_data) ||
      !GetAutofillTable()->GetPaymentsCustomerData(customer_data)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::unique_ptr<CreditCard>& entry : cards) {
    batch->Put(
        GetStorageKeyForWalletDataClientTag(GetClientTagFromCreditCard(*entry)),
        CreateEntityDataFromCard(*entry, enforce_utf8));
  }
  for (const std::unique_ptr<CreditCardCloudTokenData>& entry :
       cloud_token_data) {
    batch->Put(
        GetStorageKeyForWalletDataClientTag(
            GetClientTagFromCreditCardCloudTokenData(*entry)),
        CreateEntityDataFromCreditCardCloudTokenData(*entry, enforce_utf8));
  }

  if (customer_data) {
    batch->Put(GetStorageKeyForWalletDataClientTag(
                   GetClientTagFromPaymentsCustomerData(*customer_data)),
               CreateEntityDataFromPaymentsCustomerData(*customer_data));
  }
  for (const std::unique_ptr<Iban>& entry : ibans) {
    batch->Put(
        GetStorageKeyForWalletDataClientTag(GetClientTagFromIban(*entry)),
        CreateEntityDataFromIban(*entry, enforce_utf8));
  }
  std::move(callback).Run(std::move(batch));
}

void AutofillWalletSyncBridge::SetSyncData(
    const syncer::EntityChangeList& entity_data,
    bool notify_webdata_backend) {
  bool wallet_data_changed = false;

  // Extract the Autofill types from the sync |entity_data|.
  std::vector<CreditCard> wallet_cards;
  std::vector<Iban> wallet_ibans;
  std::vector<PaymentsCustomerData> customer_data;
  std::vector<CreditCardCloudTokenData> cloud_token_data;
  PopulateWalletTypesFromSyncData(entity_data, wallet_cards, wallet_ibans,
                                  customer_data, cloud_token_data);

  wallet_data_changed |=
      SetWalletCards(std::move(wallet_cards), notify_webdata_backend);
  wallet_data_changed |=
      SetWalletIbans(std::move(wallet_ibans), notify_webdata_backend);
  wallet_data_changed |= SetPaymentsCustomerData(std::move(customer_data));
  wallet_data_changed |=
      SetCreditCardCloudTokenData(std::move(cloud_token_data));

  // Commit the transaction to make sure the data and the metadata with the
  // new progress marker is written down (especially on Android where we
  // cannot rely on commiting transactions on shutdown). We need to commit
  // even if the wallet data has not changed because the model type state incl.
  // the progress marker always changes.
  web_data_backend_->CommitChanges();

  if (web_data_backend_ && wallet_data_changed)
    web_data_backend_->NotifyOnAutofillChangedBySync(
        syncer::AUTOFILL_WALLET_DATA);
}

bool AutofillWalletSyncBridge::SetWalletCards(
    std::vector<CreditCard> wallet_cards,
    bool notify_webdata_backend) {
  // Users can set billing address of the server credit card locally, but that
  // information does not propagate to either Chrome Sync or Google Payments
  // server. To preserve user's preferred billing address and most recent use
  // stats, copy them from local storage into `wallet_cards`.
  // Wallet CVC data is decoupled from the Wallet card data, so if
  // CVC data is present on the locally saved server card, copy that onto
  // `wallet_cards` to prevent deletion of CVC data.
  AutofillTable* table = GetAutofillTable();
  CopyRelevantWalletMetadataAndCvc(*table, &wallet_cards);

  std::vector<std::unique_ptr<CreditCard>> existing_cards;
  if (!table->GetServerCreditCards(existing_cards)) {
    return false;
  }

  table->SetServerCardsData(wallet_cards);
  bool found_diff = false;
  for (const std::unique_ptr<CreditCard>& existing_card : existing_cards) {
    bool has_orphan_card =
        base::ranges::none_of(wallet_cards, [&](const CreditCard& card) {
          return card.Compare(*existing_card) == 0;
        });
    if (has_orphan_card) {
      found_diff = true;
      if (notify_webdata_backend) {
        web_data_backend_->NotifyOfCreditCardChanged(
            CreditCardChange(CreditCardChange::REMOVE,
                             existing_card->server_id(), *existing_card));
      }
    }
  }
  for (const CreditCard& wallet_card : wallet_cards) {
    bool has_new_card = base::ranges::none_of(
        existing_cards, [&](const std::unique_ptr<CreditCard>& card) {
          return card->Compare(wallet_card) == 0;
        });
    if (has_new_card) {
      found_diff = true;
      if (notify_webdata_backend) {
        web_data_backend_->NotifyOfCreditCardChanged(CreditCardChange(
            CreditCardChange::ADD, wallet_card.server_id(), wallet_card));
      }
    }
  }
  if (found_diff) {
    // Check if there is any update on cards' virtual card metadata. If so log
    // it.
    LogVirtualCardMetadataChanges(existing_cards, wallet_cards);
  }
  return found_diff;
}

bool AutofillWalletSyncBridge::SetWalletIbans(std::vector<Iban> wallet_ibans,
                                              bool notify_webdata_backend) {
  AutofillTable* table = GetAutofillTable();

  std::vector<std::unique_ptr<Iban>> existing_ibans;
  if (!table->GetServerIbans(existing_ibans)) {
    return false;
  }

  GetAutofillTable()->SetServerIbansData(wallet_ibans);
  bool found_diff = false;
    for (const std::unique_ptr<Iban>& existing_iban : existing_ibans) {
      bool has_orphan_iban = base::ranges::none_of(
          wallet_ibans,
          [&](const Iban& iban) { return iban.Compare(*existing_iban) == 0; });
      if (has_orphan_iban) {
        found_diff = true;
        if (notify_webdata_backend) {
          web_data_backend_->NotifyOfIbanChanged(
              IbanChange(IbanChange::REMOVE, existing_iban->instrument_id(),
                         *existing_iban));
        }
      }
    }
    for (const Iban& wallet_iban : wallet_ibans) {
      bool has_new_iban = base::ranges::none_of(
          existing_ibans, [&](const std::unique_ptr<Iban>& iban) {
            return iban->Compare(wallet_iban) == 0;
          });
      if (has_new_iban) {
        found_diff = true;
        if (notify_webdata_backend) {
          web_data_backend_->NotifyOfIbanChanged(IbanChange(
              IbanChange::ADD, wallet_iban.instrument_id(), wallet_iban));
        }
      }
    }
  return found_diff;
}

bool AutofillWalletSyncBridge::SetPaymentsCustomerData(
    std::vector<PaymentsCustomerData> customer_data) {
  AutofillTable* table = GetAutofillTable();
  std::unique_ptr<PaymentsCustomerData> existing_entry;
  table->GetPaymentsCustomerData(existing_entry);

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

bool AutofillWalletSyncBridge::SetCreditCardCloudTokenData(
    const std::vector<CreditCardCloudTokenData>& cloud_token_data) {
  AutofillTable* table = GetAutofillTable();
  std::vector<std::unique_ptr<CreditCardCloudTokenData>> existing_data;
  table->GetCreditCardCloudTokenData(existing_data);

  if (AreAnyItemsDifferent(existing_data, cloud_token_data)) {
    table->SetCreditCardCloudTokenData(cloud_token_data);
    return true;
  }
  return false;
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

void AutofillWalletSyncBridge::LogVirtualCardMetadataChanges(
    const std::vector<std::unique_ptr<CreditCard>>& old_data,
    const std::vector<CreditCard>& new_data) {
  for (const CreditCard& new_card : new_data) {
    // Try to find the old card with same server id.
    auto old_data_iterator = base::ranges::find(old_data, new_card.server_id(),
                                                &CreditCard::server_id);

    // No existing card with the same ID found.
    if (old_data_iterator == old_data.end()) {
      // log the newly-synced card.
      AutofillMetrics::LogVirtualCardMetadataSynced(/*existing_card=*/false);
      continue;
    }

    // If the virtual card metadata has changed from the old card to the new
    // cards, log the updated sync.
    if ((*old_data_iterator)->virtual_card_enrollment_state() !=
            new_card.virtual_card_enrollment_state() ||
        (*old_data_iterator)->card_art_url() != new_card.card_art_url()) {
      AutofillMetrics::LogVirtualCardMetadataSynced(/*existing_card=*/true);
    }
  }
}

}  // namespace autofill
