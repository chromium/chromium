// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_syncable_service.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/sync.pb.h"

namespace autofill {

namespace {

void* AutofillWalletSyncableServiceUserDataKey() {
  // Use the address of a static so that COMDAT folding won't ever fold
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

const char* CardNetworkFromAutofillWalletCardType(
    sync_pb::WalletMaskedCreditCard::WalletCardType type) {
  switch (type) {
    case sync_pb::WalletMaskedCreditCard::AMEX:
      return kAmericanExpressCard;
    case sync_pb::WalletMaskedCreditCard::DISCOVER:
      return kDiscoverCard;
    case sync_pb::WalletMaskedCreditCard::JCB:
      return kJCBCard;
    case sync_pb::WalletMaskedCreditCard::MASTER_CARD:
      return kMasterCard;
    case sync_pb::WalletMaskedCreditCard::UNIONPAY:
      return kUnionPay;
    case sync_pb::WalletMaskedCreditCard::VISA:
      return kVisaCard;

    // These aren't supported by the client, so just declare a generic card.
    case sync_pb::WalletMaskedCreditCard::MAESTRO:
    case sync_pb::WalletMaskedCreditCard::SOLO:
    case sync_pb::WalletMaskedCreditCard::SWITCH:
    default:
      return kGenericCard;
  }
}

CreditCard::CardType CardTypeFromAutofillWalletCardClass(
    sync_pb::WalletMaskedCreditCard::WalletCardClass card_class) {
  switch (card_class) {
    case sync_pb::WalletMaskedCreditCard::CREDIT:
      return CreditCard::CARD_TYPE_CREDIT;
    case sync_pb::WalletMaskedCreditCard::DEBIT:
      return CreditCard::CARD_TYPE_DEBIT;
    case sync_pb::WalletMaskedCreditCard::PREPAID:
      return CreditCard::CARD_TYPE_PREPAID;
    default:
      return CreditCard::CARD_TYPE_UNKNOWN;
  }
}

CreditCard::ServerStatus ServerToLocalWalletCardStatus(
    sync_pb::WalletMaskedCreditCard::WalletCardStatus status) {
  switch (status) {
    case sync_pb::WalletMaskedCreditCard::VALID:
      return CreditCard::OK;
    case sync_pb::WalletMaskedCreditCard::EXPIRED:
    default:
      DCHECK_EQ(sync_pb::WalletMaskedCreditCard::EXPIRED, status);
      return CreditCard::EXPIRED;
  }
}

CreditCard CardFromWalletCardSpecifics(
    const sync_pb::WalletMaskedCreditCard& card) {
  CreditCard result(CreditCard::MASKED_SERVER_CARD, card.id());
  result.SetNumber(base::UTF8ToUTF16(card.last_four()));
  result.SetServerStatus(ServerToLocalWalletCardStatus(card.status()));
  result.SetNetworkForMaskedCard(
      CardNetworkFromAutofillWalletCardType(card.type()));
  result.set_card_type(CardTypeFromAutofillWalletCardClass(card.card_class()));
  result.SetRawInfo(CREDIT_CARD_NAME_FULL,
                    base::UTF8ToUTF16(card.name_on_card()));
  result.SetExpirationMonth(card.exp_month());
  result.SetExpirationYear(card.exp_year());
  result.set_billing_address_id(card.billing_address_id());
  result.set_bank_name(card.bank_name());
  return result;
}

AutofillProfile ProfileFromWalletCardSpecifics(
    const sync_pb::WalletPostalAddress& address) {
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, std::string());

  // AutofillProfile stores multi-line addresses with newline separators.
  std::vector<base::StringPiece> street_address(
      address.street_address().begin(), address.street_address().end());
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     base::UTF8ToUTF16(base::JoinString(street_address, "\n")));

  profile.SetRawInfo(COMPANY_NAME, base::UTF8ToUTF16(address.company_name()));
  profile.SetRawInfo(ADDRESS_HOME_STATE,
                     base::UTF8ToUTF16(address.address_1()));
  profile.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16(address.address_2()));
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                     base::UTF8ToUTF16(address.address_3()));
  // AutofillProfile doesn't support address_4 ("sub dependent locality").
  profile.SetRawInfo(ADDRESS_HOME_ZIP,
                     base::UTF8ToUTF16(address.postal_code()));
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE,
                     base::UTF8ToUTF16(address.sorting_code()));
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY,
                     base::UTF8ToUTF16(address.country_code()));
  profile.set_language_code(address.language_code());

  // SetInfo instead of SetRawInfo so the constituent pieces will be parsed
  // for these data types.
  profile.SetInfo(NAME_FULL, base::UTF8ToUTF16(address.recipient_name()),
                  profile.language_code());
  profile.SetInfo(PHONE_HOME_WHOLE_NUMBER,
                  base::UTF8ToUTF16(address.phone_number()),
                  profile.language_code());

  profile.GenerateServerProfileIdentifier();

  return profile;
}

PaymentsCustomerData CustomerDataFromSyncSpecifics(
    const sync_pb::PaymentsCustomerData& customer_data) {
  return PaymentsCustomerData{/*customer_id=*/customer_data.id()};
}

}  // namespace

// static
template <class Item>
AutofillWalletSyncableService::Diff AutofillWalletSyncableService::ComputeDiff(
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

  // Walk over both of them and count added/removed elements.
  Diff result;
  auto old_it = old_ptrs.begin();
  auto new_it = new_ptrs.begin();
  while (old_it != old_ptrs.end()) {
    if (new_it == new_ptrs.end()) {
      result.items_removed += std::distance(old_it, old_ptrs.end());
      break;
    }
    int cmp = (*old_it)->Compare(**new_it);
    if (cmp < 0) {
      ++result.items_removed;
      ++old_it;
    } else if (cmp == 0) {
      ++old_it;
      ++new_it;
    } else {
      ++result.items_added;
      ++new_it;
    }
  }
  result.items_added += std::distance(new_it, new_ptrs.end());

  DCHECK_EQ(old_data.size() + result.items_added - result.items_removed,
            new_data.size());

  return result;
}

AutofillWalletSyncableService::AutofillWalletSyncableService(
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale)
    : webdata_backend_(webdata_backend) {}

AutofillWalletSyncableService::~AutofillWalletSyncableService() {}

syncer::SyncMergeResult AutofillWalletSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_processor_ = std::move(sync_processor);
  syncer::SyncMergeResult result =
      SetSyncData(initial_sync_data, /*is_initial_data=*/true);
  if (webdata_backend_)
    webdata_backend_->NotifyThatSyncHasStarted(type);
  return result;
}

void AutofillWalletSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::AUTOFILL_WALLET_DATA);
  sync_processor_.reset();
}

syncer::SyncDataList AutofillWalletSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This data type is never synced "up" so we don't need to implement this.
  syncer::SyncDataList current_data;
  return current_data;
}

syncer::SyncError AutofillWalletSyncableService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Don't bother handling incremental updates. Wallet data changes very rarely
  // and has few items. Instead, just get all the current data and save it.
  SetSyncData(sync_processor_->GetAllSyncData(syncer::AUTOFILL_WALLET_DATA),
              /*is_initial_data=*/false);
  return syncer::SyncError();
}

// static
void AutofillWalletSyncableService::CreateForWebDataServiceAndBackend(
    AutofillWebDataService* web_data_service,
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale) {
  web_data_service->GetDBUserData()->SetUserData(
      AutofillWalletSyncableServiceUserDataKey(),
      base::WrapUnique(
          new AutofillWalletSyncableService(webdata_backend, app_locale)));
}

// static
AutofillWalletSyncableService*
AutofillWalletSyncableService::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletSyncableService*>(
      web_data_service->GetDBUserData()->GetUserData(
          AutofillWalletSyncableServiceUserDataKey()));
}

void AutofillWalletSyncableService::InjectStartSyncFlare(
    const syncer::SyncableService::StartSyncFlare& flare) {
  flare_ = flare;
}

// static
void AutofillWalletSyncableService::PopulateWalletTypesFromSyncData(
    const syncer::SyncDataList& data_list,
    std::vector<CreditCard>* wallet_cards,
    std::vector<AutofillProfile>* wallet_addresses,
    std::vector<PaymentsCustomerData>* customer_data) {
  std::map<std::string, std::string> ids;

  for (const syncer::SyncData& data : data_list) {
    DCHECK_EQ(syncer::AUTOFILL_WALLET_DATA, data.GetDataType());
    const sync_pb::AutofillWalletSpecifics& autofill_specifics =
        data.GetSpecifics().autofill_wallet();
    switch (autofill_specifics.type()) {
      case sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD:
        wallet_cards->push_back(
            CardFromWalletCardSpecifics(autofill_specifics.masked_card()));
        break;
      case sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS:
        wallet_addresses->push_back(
            ProfileFromWalletCardSpecifics(autofill_specifics.address()));

        // Map the sync billing address id to the profile's id.
        ids[autofill_specifics.address().id()] =
            wallet_addresses->back().server_id();
        break;
      case sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA:
        customer_data->push_back(
            CustomerDataFromSyncSpecifics(autofill_specifics.customer_data()));
        break;
      case sync_pb::AutofillWalletSpecifics::UNKNOWN:
        // Just ignore new entry types that the client doesn't know about.
        break;
    }
  }

  // Set the billing address of the wallet cards to the id of the appropriate
  // profile.
  for (CreditCard& card : *wallet_cards) {
    auto it = ids.find(card.billing_address_id());
    if (it != ids.end())
      card.set_billing_address_id(it->second);
  }
}

// static
void AutofillWalletSyncableService::CopyRelevantMetadataFromDisk(
    const AutofillTable& table,
    std::vector<CreditCard>* cards_from_server) {
  std::vector<std::unique_ptr<CreditCard>> cards_on_disk;
  table.GetServerCreditCards(&cards_on_disk);

  // The reasons behind brute-force search are explained in SetDataIfChanged.
  for (const auto& saved_card : cards_on_disk) {
    for (CreditCard& server_card : *cards_from_server) {
      if (saved_card->server_id() == server_card.server_id()) {
        // The wallet data doesn't have the use stats. Use the ones present on
        // disk to not overwrite them with bad data.
        server_card.set_use_count(saved_card->use_count());
        server_card.set_use_date(saved_card->use_date());

        // Keep the billing address id of the saved cards only if it points to
        // a local address.
        if (saved_card->billing_address_id().length() == kLocalGuidSize) {
          server_card.set_billing_address_id(saved_card->billing_address_id());
          break;
        }
      }
    }
  }
}

syncer::SyncMergeResult AutofillWalletSyncableService::SetSyncData(
    const syncer::SyncDataList& data_list,
    bool is_initial_data) {
  std::vector<CreditCard> wallet_cards;
  std::vector<AutofillProfile> wallet_addresses;
  std::vector<PaymentsCustomerData> customer_data;
  PopulateWalletTypesFromSyncData(data_list, &wallet_cards, &wallet_addresses,
                                  &customer_data);

  // Users can set billing address of the server credit card locally, but that
  // information does not propagate to either Chrome Sync or Google Payments
  // server. To preserve user's preferred billing address and most recent use
  // stats, copy them from disk into |wallet_cards|.
  AutofillTable* table =
      AutofillTable::FromWebDatabase(webdata_backend_->GetDatabase());
  CopyRelevantMetadataFromDisk(*table, &wallet_cards);

  // In the common case, the database won't have changed. Committing an update
  // to the database will require at least one DB page write and will schedule
  // a fsync. To avoid this I/O, it should be more efficient to do a read and
  // only do the writes if something changed.
  std::vector<std::unique_ptr<CreditCard>> existing_cards;
  table->GetServerCreditCards(&existing_cards);
  Diff cards_diff = ComputeDiff(existing_cards, wallet_cards);
  if (!cards_diff.IsEmpty())
    table->SetServerCreditCards(wallet_cards);

  std::vector<std::unique_ptr<AutofillProfile>> existing_addresses;
  table->GetServerProfiles(&existing_addresses);
  Diff addresses_diff = ComputeDiff(existing_addresses, wallet_addresses);
  if (!addresses_diff.IsEmpty())
    table->SetServerProfiles(wallet_addresses);

  syncer::SyncMergeResult merge_result(syncer::AUTOFILL_WALLET_DATA);
  merge_result.set_num_items_before_association(
      static_cast<int>(existing_cards.size() + existing_addresses.size()));
  merge_result.set_num_items_after_association(
      static_cast<int>(wallet_cards.size() + wallet_addresses.size()));

  if (customer_data.empty()) {
    // Clears the data only.
    table->SetPaymentsCustomerData(nullptr);
  } else {
    // In case there were multiple entries (and there shouldn't!), we take the
    // first entry in the vector.
    DCHECK_EQ(1u, customer_data.size());
    table->SetPaymentsCustomerData(&customer_data.front());
  }

  if (!is_initial_data) {
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletCardsAdded",
                             cards_diff.items_added);
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletCardsRemoved",
                             cards_diff.items_removed);
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletCardsAddedOrRemoved",
                             cards_diff.items_added + cards_diff.items_removed);

    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletAddressesAdded",
                             addresses_diff.items_added);
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletAddressesRemoved",
                             addresses_diff.items_removed);
    UMA_HISTOGRAM_COUNTS_100(
        "Autofill.WalletAddressesAddedOrRemoved",
        addresses_diff.items_added + addresses_diff.items_removed);
  }

  if (webdata_backend_ && (!cards_diff.IsEmpty() || !addresses_diff.IsEmpty()))
    webdata_backend_->NotifyOfMultipleAutofillChanges();

  return merge_result;
}

}  // namespace autofill
