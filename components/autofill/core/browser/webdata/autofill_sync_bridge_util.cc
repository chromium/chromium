// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"

#include "base/base64.h"
#include "base/pickle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/sync/protocol/entity_data.h"

using autofill::data_util::TruncateUTF8;
using sync_pb::AutofillWalletSpecifics;

namespace autofill {
namespace {

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
  if (network == kEloCard)
    return sync_pb::WalletMaskedCreditCard::ELO;

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
    case sync_pb::WalletMaskedCreditCard::ELO:
      return kEloCard;

    // These aren't supported by the client, so just declare a generic card.
    case sync_pb::WalletMaskedCreditCard::MAESTRO:
    case sync_pb::WalletMaskedCreditCard::SOLO:
    case sync_pb::WalletMaskedCreditCard::SWITCH:
    case sync_pb::WalletMaskedCreditCard::UNKNOWN:
      return kGenericCard;
  }
}

// Creates a CreditCard from the specified `card` specifics.
CreditCard CardFromSpecifics(const sync_pb::WalletMaskedCreditCard& card) {
  CreditCard result(CreditCard::MASKED_SERVER_CARD, card.id());
  result.SetNumber(base::UTF8ToUTF16(card.last_four()));
  result.SetNetworkForMaskedCard(CardNetworkFromWalletCardType(card.type()));
  result.SetRawInfo(CREDIT_CARD_NAME_FULL,
                    base::UTF8ToUTF16(card.name_on_card()));
  result.SetExpirationMonth(card.exp_month());
  result.SetExpirationYear(card.exp_year());
  result.set_billing_address_id(card.billing_address_id());

  CreditCard::Issuer issuer = CreditCard::Issuer::kIssuerUnknown;
  switch (card.card_issuer().issuer()) {
    case sync_pb::CardIssuer::ISSUER_UNKNOWN:
      issuer = CreditCard::Issuer::kIssuerUnknown;
      break;
    case sync_pb::CardIssuer::GOOGLE:
      issuer = CreditCard::Issuer::kGoogle;
      break;
    case sync_pb::CardIssuer::EXTERNAL_ISSUER:
      issuer = CreditCard::Issuer::kExternalIssuer;
      break;
  }
  result.set_card_issuer(issuer);
  result.set_issuer_id(card.card_issuer().issuer_id());

  if (!card.nickname().empty())
    result.SetNickname(base::UTF8ToUTF16(card.nickname()));
  result.set_instrument_id(card.instrument_id());

  CreditCard::VirtualCardEnrollmentState state;
  switch (card.virtual_card_enrollment_state()) {
    case sync_pb::WalletMaskedCreditCard::UNENROLLED:
      state = CreditCard::VirtualCardEnrollmentState::kUnenrolled;
      break;
    case sync_pb::WalletMaskedCreditCard::ENROLLED:
      state = CreditCard::VirtualCardEnrollmentState::kEnrolled;
      break;
    case sync_pb::WalletMaskedCreditCard::UNENROLLED_AND_NOT_ELIGIBLE:
      state = CreditCard::VirtualCardEnrollmentState::kUnenrolledAndNotEligible;
      break;
    case sync_pb::WalletMaskedCreditCard::UNENROLLED_AND_ELIGIBLE:
      state = CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible;
      break;
    case sync_pb::WalletMaskedCreditCard::UNSPECIFIED:
      state = CreditCard::VirtualCardEnrollmentState::kUnspecified;
      break;
  }
  result.set_virtual_card_enrollment_state(state);

  // We should only have a virtual card enrollment type for enrolled cards.
  if (card.virtual_card_enrollment_state() ==
      sync_pb::WalletMaskedCreditCard::ENROLLED) {
    CreditCard::VirtualCardEnrollmentType virtual_card_enrollment_type;
    switch (card.virtual_card_enrollment_type()) {
      case sync_pb::WalletMaskedCreditCard::TYPE_UNSPECIFIED:
        virtual_card_enrollment_type =
            CreditCard::VirtualCardEnrollmentType::kTypeUnspecified;
        break;
      case sync_pb::WalletMaskedCreditCard::ISSUER:
        virtual_card_enrollment_type =
            CreditCard::VirtualCardEnrollmentType::kIssuer;
        break;
      case sync_pb::WalletMaskedCreditCard::NETWORK:
        virtual_card_enrollment_type =
            CreditCard::VirtualCardEnrollmentType::kNetwork;
        break;
    }
    result.set_virtual_card_enrollment_type(virtual_card_enrollment_type);
  }

  if (!card.card_art_url().empty())
    result.set_card_art_url(GURL(card.card_art_url()));

  result.set_product_description(base::UTF8ToUTF16(card.product_description()));

  return result;
}

// Creates a PaymentCustomerData object corresponding to the sync datatype
// |customer_data|.
PaymentsCustomerData CustomerDataFromSpecifics(
    const sync_pb::PaymentsCustomerData& customer_data) {
  return PaymentsCustomerData{/*customer_id=*/customer_data.id()};
}

// Creates a CreditCardCloudTokenData object corresponding to the sync datatype
// |cloud_token_data|.
CreditCardCloudTokenData CloudTokenDataFromSpecifics(
    const sync_pb::WalletCreditCardCloudTokenData& cloud_token_data) {
  CreditCardCloudTokenData result;
  result.masked_card_id = cloud_token_data.masked_card_id();
  result.suffix = base::UTF8ToUTF16(cloud_token_data.suffix());
  result.exp_month = cloud_token_data.exp_month();
  result.exp_year = cloud_token_data.exp_year();
  result.card_art_url = cloud_token_data.art_fife_url();
  result.instrument_token = cloud_token_data.instrument_token();
  return result;
}

}  // namespace

std::string GetBase64EncodedId(const std::string& id) {
  std::string encoded_id;
  base::Base64Encode(id, &encoded_id);
  return encoded_id;
}

std::string GetBase64DecodedId(const std::string& id) {
  std::string decoded_id;
  base::Base64Decode(id, &decoded_id);
  return decoded_id;
}

std::string GetStorageKeyForWalletMetadataTypeAndSpecificsId(
    sync_pb::WalletMetadataSpecifics::Type type,
    const std::string& specifics_id) {
  base::Pickle pickle;
  pickle.WriteInt(static_cast<int>(type));
  // We use the (base64-encoded) |specifics_id| here.
  pickle.WriteString(specifics_id);
  return std::string(pickle.data_as_char(), pickle.size());
}

void SetAutofillWalletSpecificsFromServerProfile(
    const AutofillProfile& address,
    AutofillWalletSpecifics* wallet_specifics,
    bool enforce_utf8) {
  wallet_specifics->set_type(AutofillWalletSpecifics::POSTAL_ADDRESS);

  sync_pb::WalletPostalAddress* wallet_address =
      wallet_specifics->mutable_address();

  if (enforce_utf8) {
    wallet_address->set_id(GetBase64EncodedId(address.server_id()));
  } else {
    wallet_address->set_id(address.server_id());
  }

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

void SetAutofillWalletSpecificsFromServerCard(
    const CreditCard& card,
    AutofillWalletSpecifics* wallet_specifics,
    bool enforce_utf8) {
  wallet_specifics->set_type(AutofillWalletSpecifics::MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* wallet_card =
      wallet_specifics->mutable_masked_card();

  if (enforce_utf8) {
    wallet_card->set_id(GetBase64EncodedId(card.server_id()));
    // The billing address id might refer to a local profile guid which doesn't
    // need to be encoded.
    if (base::IsStringUTF8(card.billing_address_id())) {
      wallet_card->set_billing_address_id(card.billing_address_id());
    } else {
      wallet_card->set_billing_address_id(
          GetBase64EncodedId(card.billing_address_id()));
    }
  } else {
    wallet_card->set_id(card.server_id());
    wallet_card->set_billing_address_id(card.billing_address_id());
  }

  if (card.HasRawInfo(CREDIT_CARD_NAME_FULL)) {
    wallet_card->set_name_on_card(TruncateUTF8(
        base::UTF16ToUTF8(card.GetRawInfo(CREDIT_CARD_NAME_FULL))));
  }
  wallet_card->set_type(WalletCardTypeFromCardNetwork(card.network()));
  wallet_card->set_last_four(base::UTF16ToUTF8(card.LastFourDigits()));
  wallet_card->set_exp_month(card.expiration_month());
  wallet_card->set_exp_year(card.expiration_year());
  if (!card.nickname().empty())
    wallet_card->set_nickname(base::UTF16ToUTF8(card.nickname()));

  sync_pb::CardIssuer::Issuer issuer = sync_pb::CardIssuer::ISSUER_UNKNOWN;
  switch (card.card_issuer()) {
    case CreditCard::Issuer::kIssuerUnknown:
      issuer = sync_pb::CardIssuer::ISSUER_UNKNOWN;
      break;
    case CreditCard::Issuer::kGoogle:
      issuer = sync_pb::CardIssuer::GOOGLE;
      break;
    case CreditCard::Issuer::kExternalIssuer:
      issuer = sync_pb::CardIssuer::EXTERNAL_ISSUER;
      break;
  }
  wallet_card->mutable_card_issuer()->set_issuer(issuer);
  wallet_card->mutable_card_issuer()->set_issuer_id(card.issuer_id());

  wallet_card->set_instrument_id(card.instrument_id());

  sync_pb::WalletMaskedCreditCard::VirtualCardEnrollmentState state;
  switch (card.virtual_card_enrollment_state()) {
    case CreditCard::VirtualCardEnrollmentState::kUnenrolled:
      state = sync_pb::WalletMaskedCreditCard::UNENROLLED;
      break;
    case CreditCard::VirtualCardEnrollmentState::kEnrolled:
      state = sync_pb::WalletMaskedCreditCard::ENROLLED;
      break;
    case CreditCard::VirtualCardEnrollmentState::kUnenrolledAndNotEligible:
      state = sync_pb::WalletMaskedCreditCard::UNENROLLED_AND_NOT_ELIGIBLE;
      break;
    case CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible:
      state = sync_pb::WalletMaskedCreditCard::UNENROLLED_AND_ELIGIBLE;
      break;
    case CreditCard::VirtualCardEnrollmentState::kUnspecified:
      state = sync_pb::WalletMaskedCreditCard::UNSPECIFIED;
      break;
  }
  wallet_card->set_virtual_card_enrollment_state(state);

  // We should only have a virtual card enrollment type for enrolled cards.
  if (card.virtual_card_enrollment_state() ==
      CreditCard::VirtualCardEnrollmentState::kEnrolled) {
    sync_pb::WalletMaskedCreditCard::VirtualCardEnrollmentType
        virtual_card_enrollment_type;
    switch (card.virtual_card_enrollment_type()) {
      case CreditCard::VirtualCardEnrollmentType::kTypeUnspecified:
        virtual_card_enrollment_type =
            sync_pb::WalletMaskedCreditCard::TYPE_UNSPECIFIED;
        break;
      case CreditCard::VirtualCardEnrollmentType::kIssuer:
        virtual_card_enrollment_type = sync_pb::WalletMaskedCreditCard::ISSUER;
        break;
      case CreditCard::VirtualCardEnrollmentType::kNetwork:
        virtual_card_enrollment_type = sync_pb::WalletMaskedCreditCard::NETWORK;
        break;
    }
    wallet_card->set_virtual_card_enrollment_type(virtual_card_enrollment_type);
  }

  if (!card.card_art_url().is_empty())
    wallet_card->set_card_art_url(card.card_art_url().spec());

  wallet_card->set_product_description(
      base::UTF16ToUTF8(card.product_description()));
}

void SetAutofillWalletSpecificsFromPaymentsCustomerData(
    const PaymentsCustomerData& customer_data,
    AutofillWalletSpecifics* wallet_specifics) {
  wallet_specifics->set_type(AutofillWalletSpecifics::CUSTOMER_DATA);

  sync_pb::PaymentsCustomerData* mutable_customer_data =
      wallet_specifics->mutable_customer_data();
  mutable_customer_data->set_id(customer_data.customer_id);
}

void SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
    const CreditCardCloudTokenData& cloud_token_data,
    sync_pb::AutofillWalletSpecifics* wallet_specifics,
    bool enforce_utf8) {
  wallet_specifics->set_type(
      AutofillWalletSpecifics::CREDIT_CARD_CLOUD_TOKEN_DATA);

  sync_pb::WalletCreditCardCloudTokenData* mutable_cloud_token_data =
      wallet_specifics->mutable_cloud_token_data();

  if (enforce_utf8) {
    mutable_cloud_token_data->set_masked_card_id(
        GetBase64EncodedId(cloud_token_data.masked_card_id));
  } else {
    mutable_cloud_token_data->set_masked_card_id(
        cloud_token_data.masked_card_id);
  }

  mutable_cloud_token_data->set_suffix(
      base::UTF16ToUTF8(cloud_token_data.suffix));
  mutable_cloud_token_data->set_exp_month(cloud_token_data.exp_month);
  mutable_cloud_token_data->set_exp_year(cloud_token_data.exp_year);
  mutable_cloud_token_data->set_art_fife_url(cloud_token_data.card_art_url);
  mutable_cloud_token_data->set_instrument_token(
      cloud_token_data.instrument_token);
}

void SetAutofillWalletUsageSpecificsFromAutofillWalletUsageData(
    const AutofillWalletUsageData& wallet_usage_data,
    sync_pb::AutofillWalletUsageSpecifics* wallet_usage_specifics) {
  if (wallet_usage_data.usage_data_type() ==
      AutofillWalletUsageData::UsageDataType::kVirtualCard) {
    // Ensure the Virtual Card Usage Data fields are set before transferring to
    // `wallet_usage_specifics`.
    DCHECK(
        IsVirtualCardUsageDataSet(wallet_usage_data.virtual_card_usage_data()));

    wallet_usage_specifics->set_guid(
        *wallet_usage_data.virtual_card_usage_data().usage_data_id());

    wallet_usage_specifics->mutable_virtual_card_usage_data()
        ->set_instrument_id(
            *wallet_usage_data.virtual_card_usage_data().instrument_id());

    wallet_usage_specifics->mutable_virtual_card_usage_data()
        ->set_virtual_card_last_four(
            base::UTF16ToUTF8(*wallet_usage_data.virtual_card_usage_data()
                                   .virtual_card_last_four()));

    wallet_usage_specifics->mutable_virtual_card_usage_data()->set_merchant_url(
        wallet_usage_data.virtual_card_usage_data()
            .merchant_origin()
            .Serialize());
  }
}

void SetAutofillOfferSpecificsFromOfferData(
    const AutofillOfferData& offer_data,
    sync_pb::AutofillOfferSpecifics* offer_specifics) {
  // General offer data:
  offer_specifics->set_id(offer_data.GetOfferId());
  offer_specifics->set_offer_details_url(
      offer_data.GetOfferDetailsUrl().spec());
  for (const GURL& merchant_origin : offer_data.GetMerchantOrigins()) {
    offer_specifics->add_merchant_domain(merchant_origin.spec());
  }
  offer_specifics->set_offer_expiry_date(
      (offer_data.GetExpiry() - base::Time::UnixEpoch()).InSeconds());
  offer_specifics->mutable_display_strings()->set_value_prop_text(
      offer_data.GetDisplayStrings().value_prop_text);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  offer_specifics->mutable_display_strings()->set_see_details_text_mobile(
      offer_data.GetDisplayStrings().see_details_text);
  offer_specifics->mutable_display_strings()
      ->set_usage_instructions_text_mobile(
          offer_data.GetDisplayStrings().usage_instructions_text);
#else
  offer_specifics->mutable_display_strings()->set_see_details_text_desktop(
      offer_data.GetDisplayStrings().see_details_text);
  offer_specifics->mutable_display_strings()
      ->set_usage_instructions_text_desktop(
          offer_data.GetDisplayStrings().usage_instructions_text);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Because card_linked_offer_data and promo_code_offer_data are a oneof,
  // setting one will clear the other. We should figure out which one we care
  // about.
  if (offer_data.GetPromoCode().empty()) {
    // Card-linked offer fields (promo code is empty):
    for (int64_t instrument_id : offer_data.GetEligibleInstrumentIds()) {
      offer_specifics->mutable_card_linked_offer_data()->add_instrument_id(
          instrument_id);
    }
    if (offer_data.GetOfferRewardAmount().find("%") != std::string::npos) {
      offer_specifics->mutable_percentage_reward()->set_percentage(
          offer_data.GetOfferRewardAmount());
    } else {
      offer_specifics->mutable_fixed_amount_reward()->set_amount(
          offer_data.GetOfferRewardAmount());
    }
  } else {
    // Promo code offer fields:
    offer_specifics->mutable_promo_code_offer_data()->set_promo_code(
        offer_data.GetPromoCode());
  }
}

AutofillOfferData AutofillOfferDataFromOfferSpecifics(
    const sync_pb::AutofillOfferSpecifics& offer_specifics) {
  DCHECK(IsOfferSpecificsValid(offer_specifics));

  // General offer data:
  int64_t offer_id = offer_specifics.id();
  base::Time expiry = base::Time::UnixEpoch() +
                      base::Seconds(offer_specifics.offer_expiry_date());
  GURL offer_details_url = GURL(offer_specifics.offer_details_url());
  std::vector<GURL> merchant_origins;
  for (const std::string& domain : offer_specifics.merchant_domain()) {
    const GURL gurl_domain = GURL(domain);
    if (gurl_domain.is_valid())
      merchant_origins.emplace_back(gurl_domain.DeprecatedGetOriginAsURL());
  }
  DisplayStrings display_strings;
  display_strings.value_prop_text =
      offer_specifics.display_strings().value_prop_text();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  display_strings.see_details_text =
      offer_specifics.display_strings().see_details_text_mobile();
  display_strings.usage_instructions_text =
      offer_specifics.display_strings().usage_instructions_text_mobile();
#else
  display_strings.see_details_text =
      offer_specifics.display_strings().see_details_text_desktop();
  display_strings.usage_instructions_text =
      offer_specifics.display_strings().usage_instructions_text_desktop();
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  if (offer_specifics.promo_code_offer_data().promo_code().empty()) {
    // Card-linked offer fields:
    std::string offer_reward_amount =
        offer_specifics.has_percentage_reward()
            ? offer_specifics.percentage_reward().percentage()
            : offer_specifics.fixed_amount_reward().amount();
    std::vector<int64_t> eligible_instrument_id;
    for (int64_t instrument_id :
         offer_specifics.card_linked_offer_data().instrument_id()) {
      eligible_instrument_id.push_back(instrument_id);
    }

    AutofillOfferData offer_data = AutofillOfferData::GPayCardLinkedOffer(
        offer_id, expiry, merchant_origins, offer_details_url, display_strings,
        eligible_instrument_id, offer_reward_amount);
    return offer_data;
  } else {
    AutofillOfferData offer_data = AutofillOfferData::GPayPromoCodeOffer(
        offer_id, expiry, merchant_origins, offer_details_url, display_strings,
        offer_specifics.promo_code_offer_data().promo_code());
    return offer_data;
  }
}

VirtualCardUsageData VirtualCardUsageDataFromUsageSpecifics(
    const sync_pb::AutofillWalletUsageSpecifics& usage_specifics) {
  const sync_pb::AutofillWalletUsageSpecifics::VirtualCardUsageData
      virtual_card_usage_data_specifics =
          usage_specifics.virtual_card_usage_data();
  DCHECK(usage_specifics.has_guid() && IsVirtualCardUsageDataSpecificsValid(
                                           virtual_card_usage_data_specifics));

  return VirtualCardUsageData(
      VirtualCardUsageData::UsageDataId(usage_specifics.guid()),
      VirtualCardUsageData::InstrumentId(
          virtual_card_usage_data_specifics.instrument_id()),
      VirtualCardUsageData::VirtualCardLastFour(base::UTF8ToUTF16(
          virtual_card_usage_data_specifics.virtual_card_last_four())),
      url::Origin::Create(
          GURL(virtual_card_usage_data_specifics.merchant_url())));
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

  // Call the finalization routine to parse the structure of the name and the
  // address into their components.
  profile.FinalizeAfterImport();
  return profile;
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
    std::vector<PaymentsCustomerData>* customer_data,
    std::vector<CreditCardCloudTokenData>* cloud_token_data) {
  std::map<std::string, std::string> ids;

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    DCHECK(change->data().specifics.has_autofill_wallet());

    const sync_pb::AutofillWalletSpecifics& autofill_specifics =
        change->data().specifics.autofill_wallet();

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
      case sync_pb::AutofillWalletSpecifics::CREDIT_CARD_CLOUD_TOKEN_DATA:
        cloud_token_data->push_back(
            CloudTokenDataFromSpecifics(autofill_specifics.cloud_token_data()));
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

template <class Item>
bool AreAnyItemsDifferent(const std::vector<std::unique_ptr<Item>>& old_data,
                          const std::vector<Item>& new_data) {
  if (old_data.size() != new_data.size())
    return true;

  std::vector<const Item*> old_ptrs;
  old_ptrs.reserve(old_data.size());
  for (const std::unique_ptr<Item>& old_item : old_data)
    old_ptrs.push_back(old_item.get());
  std::vector<const Item*> new_ptrs;
  new_ptrs.reserve(new_data.size());
  for (const Item& new_item : new_data)
    new_ptrs.push_back(&new_item);

  // Sort our vectors.
  auto compare_less = [](const Item* lhs, const Item* rhs) {
    return lhs->Compare(*rhs) < 0;
  };
  std::sort(old_ptrs.begin(), old_ptrs.end(), compare_less);
  std::sort(new_ptrs.begin(), new_ptrs.end(), compare_less);

  auto compare_equal = [](const Item* lhs, const Item* rhs) {
    return lhs->Compare(*rhs) == 0;
  };
  return !base::ranges::equal(old_ptrs, new_ptrs, compare_equal);
}

template bool AreAnyItemsDifferent<>(
    const std::vector<std::unique_ptr<AutofillOfferData>>&,
    const std::vector<AutofillOfferData>&);

template bool AreAnyItemsDifferent<>(
    const std::vector<std::unique_ptr<CreditCardCloudTokenData>>&,
    const std::vector<CreditCardCloudTokenData>&);

bool IsOfferSpecificsValid(const sync_pb::AutofillOfferSpecifics specifics) {
  // A valid offer has a non-empty id.
  if (!specifics.has_id())
    return false;

  // A valid offer must have at least one valid merchant domain URL.
  if (specifics.merchant_domain().size() == 0) {
    return false;
  }
  bool has_valid_domain = false;
  for (const std::string& domain : specifics.merchant_domain()) {
    if (GURL(domain).is_valid()) {
      has_valid_domain = true;
      break;
    }
  }
  if (!has_valid_domain) {
    return false;
  }

  // Card-linked offers must have at least one linked card instrument ID, and
  // fixed_amount_reward or percentage_reward. Promo code offers must have a
  // promo code.
  bool has_instrument_id =
      specifics.has_card_linked_offer_data() &&
      specifics.card_linked_offer_data().instrument_id().size() != 0;
  bool has_fixed_or_percentage_reward =
      (specifics.has_fixed_amount_reward() &&
       specifics.fixed_amount_reward().has_amount()) ||
      (specifics.has_percentage_reward() &&
       specifics.percentage_reward().has_percentage() &&
       specifics.percentage_reward().percentage().find('%') !=
           std::string::npos);
  bool has_promo_code = specifics.has_promo_code_offer_data() &&
                        specifics.promo_code_offer_data().promo_code() != "";

  return (has_instrument_id && has_fixed_or_percentage_reward) ||
         has_promo_code;
}

bool IsVirtualCardUsageDataSpecificsValid(
    const sync_pb::AutofillWalletUsageSpecifics::VirtualCardUsageData&
        specifics) {
  // Ensure fields are present and in correct format.
  return specifics.has_instrument_id() &&
         specifics.has_virtual_card_last_four() &&
         specifics.virtual_card_last_four().length() == 4 &&
         specifics.has_merchant_url() &&
         !url::Origin::Create(GURL(specifics.merchant_url())).opaque();
}

bool IsVirtualCardUsageDataSet(
    const VirtualCardUsageData& virtual_card_usage_data) {
  return *virtual_card_usage_data.instrument_id() != 0 &&
         !virtual_card_usage_data.usage_data_id()->empty() &&
         !virtual_card_usage_data.virtual_card_last_four()->empty();
}

}  // namespace autofill
