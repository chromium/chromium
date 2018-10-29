// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/sync/model/entity_data.h"

using autofill::data_util::TruncateUTF8;
using sync_pb::AutofillWalletSpecifics;
using syncer::EntityData;

namespace autofill {
namespace {
sync_pb::WalletMaskedCreditCard::WalletCardStatus LocalToServerStatus(
    const CreditCard& card) {
  switch (card.GetServerStatus()) {
    case CreditCard::OK:
      return sync_pb::WalletMaskedCreditCard::VALID;
    case CreditCard::EXPIRED:
      return sync_pb::WalletMaskedCreditCard::EXPIRED;
  }
}

CreditCard::ServerStatus ServerToLocalStatus(
    sync_pb::WalletMaskedCreditCard::WalletCardStatus status) {
  switch (status) {
    case sync_pb::WalletMaskedCreditCard::VALID:
      return CreditCard::OK;
    case sync_pb::WalletMaskedCreditCard::EXPIRED:
      return CreditCard::EXPIRED;
  }
}

sync_pb::WalletMaskedCreditCard::WalletCardType WalletCardTypeFromCardNetwork(
    const std::string& network) {
  if (network == kAmericanExpressCard)
    return sync_pb::WalletMaskedCreditCard::AMEX;
  if (network == kDiscoverCard)
    return sync_pb::WalletMaskedCreditCard::DISCOVER;
  if (network == kJCBCard)
    return sync_pb::WalletMaskedCreditCard::JCB;
  if (network == kMasterCard)
    return sync_pb::WalletMaskedCreditCard::MASTER_CARD;
  if (network == kUnionPay)
    return sync_pb::WalletMaskedCreditCard::UNIONPAY;
  if (network == kVisaCard)
    return sync_pb::WalletMaskedCreditCard::VISA;

  // Some cards aren't supported by the client, so just return unknown.
  return sync_pb::WalletMaskedCreditCard::UNKNOWN;
}

const char* CardNetworkFromWalletCardType(
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
    case sync_pb::WalletMaskedCreditCard::UNKNOWN:
      return kGenericCard;
  }
}

sync_pb::WalletMaskedCreditCard::WalletCardClass WalletCardClassFromCardType(
    CreditCard::CardType card_type) {
  switch (card_type) {
    case CreditCard::CARD_TYPE_CREDIT:
      return sync_pb::WalletMaskedCreditCard::CREDIT;
    case CreditCard::CARD_TYPE_DEBIT:
      return sync_pb::WalletMaskedCreditCard::DEBIT;
    case CreditCard::CARD_TYPE_PREPAID:
      return sync_pb::WalletMaskedCreditCard::PREPAID;
    case CreditCard::CARD_TYPE_UNKNOWN:
      return sync_pb::WalletMaskedCreditCard::UNKNOWN_CARD_CLASS;
  }
}

CreditCard::CardType CardTypeFromWalletCardClass(
    sync_pb::WalletMaskedCreditCard::WalletCardClass card_class) {
  switch (card_class) {
    case sync_pb::WalletMaskedCreditCard::CREDIT:
      return CreditCard::CARD_TYPE_CREDIT;
    case sync_pb::WalletMaskedCreditCard::DEBIT:
      return CreditCard::CARD_TYPE_DEBIT;
    case sync_pb::WalletMaskedCreditCard::PREPAID:
      return CreditCard::CARD_TYPE_PREPAID;
    case sync_pb::WalletMaskedCreditCard::UNKNOWN_CARD_CLASS:
      return CreditCard::CARD_TYPE_UNKNOWN;
  }
}

}  // namespace

std::string GetBase64EncodedServerId(const std::string& server_id) {
  std::string encoded_id;
  base::Base64Encode(server_id, &encoded_id);
  return encoded_id;
}

std::string GetSpecificsIdForEntryServerId(const std::string& server_id) {
  // TODO(jkrcal): This specifics id for wallet_data probably should not be
  // base64 encoded - this function is only used in printing debug data; should
  // the storage key for wallet_data below be encoded? (probably yes, as this is
  // already launched).
  return GetBase64EncodedServerId(server_id);
}

std::string GetSpecificsIdForMetadataId(const std::string& metadata_id) {
  return GetBase64EncodedServerId(metadata_id);
}

std::string GetStorageKeyForSpecificsId(const std::string& specifics_id) {
  // We use the base64 encoded |specifics_id| directly as the storage key, this
  // function only hides this definition from all its call sites.
  return specifics_id;
}

std::string GetStorageKeyForEntryServerId(const std::string& server_id) {
  // TODO(jkrcal): This probably needs to stay base64 encoded while specifics id
  // should not. Fix.
  return GetStorageKeyForSpecificsId(GetSpecificsIdForEntryServerId(server_id));
}

std::string GetStorageKeyForMetadataId(const std::string& metadata_id) {
  return GetStorageKeyForSpecificsId(GetSpecificsIdForMetadataId(metadata_id));
}

std::string GetClientTagForSpecificsId(
    AutofillWalletSpecifics::WalletInfoType type,
    const std::string& wallet_data_specifics_id) {
  switch (type) {
    case AutofillWalletSpecifics::POSTAL_ADDRESS:
      return "address-" + wallet_data_specifics_id;
    case AutofillWalletSpecifics::MASKED_CREDIT_CARD:
      return "card-" + wallet_data_specifics_id;
    case sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA:
      return "customer-" + wallet_data_specifics_id;
    case AutofillWalletSpecifics::UNKNOWN:
      NOTREACHED();
      return "";
  }
}

void SetAutofillWalletSpecificsFromServerProfile(
    const AutofillProfile& address,
    AutofillWalletSpecifics* wallet_specifics) {
  wallet_specifics->set_type(AutofillWalletSpecifics::POSTAL_ADDRESS);

  sync_pb::WalletPostalAddress* wallet_address =
      wallet_specifics->mutable_address();

  wallet_address->set_id(address.server_id());
  wallet_address->set_language_code(TruncateUTF8(address.language_code()));

  if (address.HasRawInfo(NAME_FULL)) {
    wallet_address->set_recipient_name(
        TruncateUTF8(base::UTF16ToUTF8(address.GetRawInfo(NAME_FULL))));
  }
  if (address.HasRawInfo(COMPANY_NAME)) {
    wallet_address->set_company_name(
        TruncateUTF8(base::UTF16ToUTF8(address.GetRawInfo(COMPANY_NAME))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_STREET_ADDRESS)) {
    wallet_address->add_street_address(TruncateUTF8(
        base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_STATE)) {
    wallet_address->set_address_1(TruncateUTF8(
        base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_STATE))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_CITY)) {
    wallet_address->set_address_2(
        TruncateUTF8(base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_CITY))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY)) {
    wallet_address->set_address_3(TruncateUTF8(base::UTF16ToUTF8(
        address.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_ZIP)) {
    wallet_address->set_postal_code(
        TruncateUTF8(base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_ZIP))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_COUNTRY)) {
    wallet_address->set_country_code(TruncateUTF8(
        base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_COUNTRY))));
  }
  if (address.HasRawInfo(PHONE_HOME_WHOLE_NUMBER)) {
    wallet_address->set_phone_number(TruncateUTF8(
        base::UTF16ToUTF8(address.GetRawInfo(PHONE_HOME_WHOLE_NUMBER))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_SORTING_CODE)) {
    wallet_address->set_sorting_code(TruncateUTF8(
        base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_SORTING_CODE))));
  }
}

std::unique_ptr<EntityData> CreateEntityDataFromAutofillServerProfile(
    const AutofillProfile& address) {
  auto entity_data = std::make_unique<EntityData>();

  std::string specifics_id =
      GetSpecificsIdForEntryServerId(address.server_id());
  entity_data->non_unique_name = GetClientTagForSpecificsId(
      AutofillWalletSpecifics::POSTAL_ADDRESS, specifics_id);

  AutofillWalletSpecifics* wallet_specifics =
      entity_data->specifics.mutable_autofill_wallet();

  SetAutofillWalletSpecificsFromServerProfile(address, wallet_specifics);

  return entity_data;
}

AutofillProfile ProfileFromSpecifics(
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

void SetAutofillWalletSpecificsFromServerCard(
    const CreditCard& card,
    AutofillWalletSpecifics* wallet_specifics) {
  wallet_specifics->set_type(AutofillWalletSpecifics::MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* wallet_card =
      wallet_specifics->mutable_masked_card();
  wallet_card->set_id(card.server_id());
  wallet_card->set_status(LocalToServerStatus(card));
  if (card.HasRawInfo(CREDIT_CARD_NAME_FULL)) {
    wallet_card->set_name_on_card(TruncateUTF8(
        base::UTF16ToUTF8(card.GetRawInfo(CREDIT_CARD_NAME_FULL))));
  }
  wallet_card->set_type(WalletCardTypeFromCardNetwork(card.network()));
  wallet_card->set_last_four(base::UTF16ToUTF8(card.LastFourDigits()));
  wallet_card->set_exp_month(card.expiration_month());
  wallet_card->set_exp_year(card.expiration_year());
  wallet_card->set_billing_address_id(card.billing_address_id());
  wallet_card->set_card_class(WalletCardClassFromCardType(card.card_type()));
  wallet_card->set_bank_name(card.bank_name());
}

std::unique_ptr<EntityData> CreateEntityDataFromCard(const CreditCard& card) {
  std::string specifics_id = GetSpecificsIdForEntryServerId(card.server_id());

  auto entity_data = std::make_unique<EntityData>();
  entity_data->non_unique_name = GetClientTagForSpecificsId(
      AutofillWalletSpecifics::MASKED_CREDIT_CARD, specifics_id);

  AutofillWalletSpecifics* wallet_specifics =
      entity_data->specifics.mutable_autofill_wallet();

  SetAutofillWalletSpecificsFromServerCard(card, wallet_specifics);

  return entity_data;
}

CreditCard CardFromSpecifics(const sync_pb::WalletMaskedCreditCard& card) {
  CreditCard result(CreditCard::MASKED_SERVER_CARD, card.id());
  result.SetNumber(base::UTF8ToUTF16(card.last_four()));
  result.SetServerStatus(ServerToLocalStatus(card.status()));
  result.SetNetworkForMaskedCard(CardNetworkFromWalletCardType(card.type()));
  result.set_card_type(CardTypeFromWalletCardClass(card.card_class()));
  result.SetRawInfo(CREDIT_CARD_NAME_FULL,
                    base::UTF8ToUTF16(card.name_on_card()));
  result.SetExpirationMonth(card.exp_month());
  result.SetExpirationYear(card.exp_year());
  result.set_billing_address_id(card.billing_address_id());
  result.set_bank_name(card.bank_name());
  return result;
}

std::unique_ptr<EntityData> CreateEntityDataFromPaymentsCustomerData(
    const PaymentsCustomerData& customer_data) {
  // We use customer_id as a storage key here.
  auto entity_data = std::make_unique<EntityData>();
  entity_data->non_unique_name = GetClientTagForSpecificsId(
      AutofillWalletSpecifics::CUSTOMER_DATA, customer_data.customer_id);

  AutofillWalletSpecifics* wallet_specifics =
      entity_data->specifics.mutable_autofill_wallet();

  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     wallet_specifics);

  return entity_data;
}

void SetAutofillWalletSpecificsFromPaymentsCustomerData(
    const PaymentsCustomerData& customer_data,
    AutofillWalletSpecifics* wallet_specifics) {
  wallet_specifics->set_type(AutofillWalletSpecifics::CUSTOMER_DATA);

  sync_pb::PaymentsCustomerData* mutable_customer_data =
      wallet_specifics->mutable_customer_data();
  mutable_customer_data->set_id(customer_data.customer_id);
}

PaymentsCustomerData CustomerDataFromSpecifics(
    const sync_pb::PaymentsCustomerData& customer_data) {
  return PaymentsCustomerData{/*customer_id=*/customer_data.id()};
}

void CopyRelevantWalletMetadataFromDisk(
    const AutofillTable& table,
    std::vector<CreditCard>* cards_from_server) {
  std::vector<std::unique_ptr<CreditCard>> cards_on_disk;
  table.GetServerCreditCards(&cards_on_disk);

  // Since the number of cards is fairly small, the brute-force search is good
  // enough.
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

void PopulateWalletTypesFromSyncData(
    const syncer::EntityChangeList& entity_data,
    std::vector<CreditCard>* wallet_cards,
    std::vector<AutofillProfile>* wallet_addresses,
    std::vector<PaymentsCustomerData>* customer_data) {
  std::map<std::string, std::string> ids;

  for (const syncer::EntityChange& change : entity_data) {
    DCHECK(change.data().specifics.has_autofill_wallet());

    const sync_pb::AutofillWalletSpecifics& autofill_specifics =
        change.data().specifics.autofill_wallet();

    switch (autofill_specifics.type()) {
      case sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD:
        wallet_cards->push_back(
            CardFromSpecifics(autofill_specifics.masked_card()));
        break;
      case sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS:
        // Unlike other pointers, |wallet_addresses| can be nullptr. This means
        // that addresses should not get populated (and billing address ids not
        // get translated to local profile ids).
        if (wallet_addresses) {
          wallet_addresses->push_back(
              ProfileFromSpecifics(autofill_specifics.address()));

          // Map the sync billing address id to the profile's id.
          ids[autofill_specifics.address().id()] =
              wallet_addresses->back().server_id();
        }
        break;
      case sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA:
        customer_data->push_back(
            CustomerDataFromSpecifics(autofill_specifics.customer_data()));
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

}  // namespace autofill
