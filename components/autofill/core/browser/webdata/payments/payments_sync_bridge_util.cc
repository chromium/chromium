// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_util.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/overloaded.h"
#include "base/pickle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/data_model/payment_instrument.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
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
  if (network == kEloCard)
    return sync_pb::WalletMaskedCreditCard::ELO;
  if (network == kJCBCard)
    return sync_pb::WalletMaskedCreditCard::JCB;
  if (network == kMasterCard)
    return sync_pb::WalletMaskedCreditCard::MASTER_CARD;
  if (network == kUnionPay)
    return sync_pb::WalletMaskedCreditCard::UNIONPAY;
  if (network == kVerveCard &&
      base::FeatureList::IsEnabled(features::kAutofillEnableVerveCardSupport)) {
    return sync_pb::WalletMaskedCreditCard::VERVE;
  }
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
    case sync_pb::WalletMaskedCreditCard::ELO:
      return kEloCard;
    case sync_pb::WalletMaskedCreditCard::JCB:
      return kJCBCard;
    case sync_pb::WalletMaskedCreditCard::MASTER_CARD:
      return kMasterCard;
    case sync_pb::WalletMaskedCreditCard::UNIONPAY:
      return kUnionPay;
    case sync_pb::WalletMaskedCreditCard::VERVE:
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableVerveCardSupport)) {
        return kVerveCard;
      }
      return kGenericCard;
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

sync_pb::CardBenefit::CategoryBenefitType
CategoryBenefitTypeFromBenefitCategory(
    const CreditCardCategoryBenefit::BenefitCategory benefit_category) {
  switch (benefit_category) {
    case CreditCardCategoryBenefit::BenefitCategory::kSubscription:
      return sync_pb::CardBenefit::SUBSCRIPTION;
    case CreditCardCategoryBenefit::BenefitCategory::kFlights:
      return sync_pb::CardBenefit::FLIGHTS;
    case CreditCardCategoryBenefit::BenefitCategory::kDining:
      return sync_pb::CardBenefit::DINING;
    case CreditCardCategoryBenefit::BenefitCategory::kEntertainment:
      return sync_pb::CardBenefit::ENTERTAINMENT;
    case CreditCardCategoryBenefit::BenefitCategory::kStreaming:
      return sync_pb::CardBenefit::STREAMING;
    case CreditCardCategoryBenefit::BenefitCategory::kGroceryStores:
      return sync_pb::CardBenefit::GROCERY_STORES;
    case CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory:
      return sync_pb::CardBenefit::CATEGORY_BENEFIT_TYPE_UNKNOWN;
  }
}

CreditCardCategoryBenefit::BenefitCategory
BenefitCategoryFromCategoryBenefitType(
    const sync_pb::CardBenefit::CategoryBenefitType category_benefit_type) {
  switch (category_benefit_type) {
    case sync_pb::CardBenefit::SUBSCRIPTION:
      return CreditCardCategoryBenefit::BenefitCategory::kSubscription;
    case sync_pb::CardBenefit::FLIGHTS:
      return CreditCardCategoryBenefit::BenefitCategory::kFlights;
    case sync_pb::CardBenefit::DINING:
      return CreditCardCategoryBenefit::BenefitCategory::kDining;
    case sync_pb::CardBenefit::ENTERTAINMENT:
      return CreditCardCategoryBenefit::BenefitCategory::kEntertainment;
    case sync_pb::CardBenefit::STREAMING:
      return CreditCardCategoryBenefit::BenefitCategory::kStreaming;
    case sync_pb::CardBenefit::GROCERY_STORES:
      return CreditCardCategoryBenefit::BenefitCategory::kGroceryStores;
    case sync_pb::CardBenefit::CATEGORY_BENEFIT_TYPE_UNKNOWN:
      return CreditCardCategoryBenefit::BenefitCategory::
          kUnknownBenefitCategory;
  }
}

// Creates a CreditCardBenefit from the specified `benefit_specifics` and
// `instrument_id`.
std::optional<CreditCardBenefit> CreditCardBenefitFromSpecifics(
    const sync_pb::CardBenefit& benefit_specifics,
    const int64_t instrument_id) {
  if (!benefit_specifics.has_benefit_id() ||
      !benefit_specifics.has_benefit_description()) {
    return std::nullopt;
  }

  CreditCardBenefitBase::BenefitId benefit_id =
      CreditCardBenefitBase::BenefitId(benefit_specifics.benefit_id());
  CreditCardBenefitBase::LinkedCardInstrumentId linked_card_instrument_id =
      CreditCardBenefitBase::LinkedCardInstrumentId(instrument_id);
  std::u16string benefit_description =
      base::UTF8ToUTF16(benefit_specifics.benefit_description());
  // Set `start_time` to min if no value is given by specifics.
  base::Time start_time =
      benefit_specifics.has_start_time_unix_epoch_milliseconds()
          ? base::Time::FromMillisecondsSinceUnixEpoch(
                benefit_specifics.start_time_unix_epoch_milliseconds())
          : base::Time::Min();
  // Set `expiry_time` to max if no value is given by specifics.
  base::Time expiry_time =
      benefit_specifics.has_end_time_unix_epoch_milliseconds()
          ? base::Time::FromMillisecondsSinceUnixEpoch(
                benefit_specifics.end_time_unix_epoch_milliseconds())
          : base::Time::Max();

  if (benefit_specifics.has_flat_rate_benefit()) {
    return CreditCardFlatRateBenefit(benefit_id, linked_card_instrument_id,
                                     benefit_description, start_time,
                                     expiry_time);
  }

  if (benefit_specifics.has_category_benefit() &&
      BenefitCategoryFromCategoryBenefitType(
          benefit_specifics.category_benefit().category_benefit_type()) !=
          CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory) {
    return CreditCardCategoryBenefit(
        benefit_id, linked_card_instrument_id,
        BenefitCategoryFromCategoryBenefitType(
            benefit_specifics.category_benefit().category_benefit_type()),
        benefit_description, start_time, expiry_time);
  }

  if (benefit_specifics.has_merchant_benefit() &&
      benefit_specifics.merchant_benefit().merchant_domain_size() > 0) {
    base::flat_set<url::Origin> merchant_domains;
    for (const std::string& url :
         benefit_specifics.merchant_benefit().merchant_domain()) {
      merchant_domains.insert(url::Origin::Create(GURL(url)));
    }
    return CreditCardMerchantBenefit(benefit_id, linked_card_instrument_id,
                                     benefit_description, merchant_domains,
                                     start_time, expiry_time);
  }

  return std::nullopt;
}

// Creates a vector of CreditCardBenefit from the specifies 'card_specifics`.
std::vector<CreditCardBenefit> CreditCardBenefitsFromCardSpecifics(
    const sync_pb::WalletMaskedCreditCard& card_specifics) {
  std::vector<CreditCardBenefit> benefits_from_specifics;

  // Only return card benefits if the related feature is enabled and
  // `product_terms_url` is not empty.
  // Benefit should not be returned without a `product_terms_url` as we
  // should not show users data from the issuers without giving them
  // access to the terms and conditions.
  if (!card_specifics.has_product_terms_url() ||
      !base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableCardBenefitsSync)) {
    return benefits_from_specifics;
  }

  // Get `CreditCardBenefit` from card_specifics.
  for (const sync_pb::CardBenefit& benefit_specifics :
       card_specifics.card_benefit()) {
    if (std::optional<CreditCardBenefit> benefit =
            CreditCardBenefitFromSpecifics(benefit_specifics,
                                           card_specifics.instrument_id())) {
      benefits_from_specifics.push_back(benefit.value());
    }
  }

  return benefits_from_specifics;
}

// Creates a CreditCard from the specified `card` specifics.
CreditCard CardFromSpecifics(const sync_pb::WalletMaskedCreditCard& card) {
  CreditCard result(CreditCard::RecordType::kMaskedServerCard, card.id());
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

  if (!card.nickname().empty()) {
    result.SetNickname(base::UTF8ToUTF16(card.nickname()));
  }
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

  if (!card.card_art_url().empty()) {
    result.set_card_art_url(GURL(card.card_art_url()));
  }

  result.set_product_description(base::UTF8ToUTF16(card.product_description()));

  if (card.has_product_terms_url() &&
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableCardBenefitsSync)) {
    result.set_product_terms_url(GURL(card.product_terms_url()));
  }

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

// Creates an IBAN from the specified `payment_instrument` specifics.
Iban IbanFromSpecifics(const sync_pb::PaymentInstrument& payment_instrument) {
  Iban result{Iban::InstrumentId(payment_instrument.instrument_id())};
  result.set_prefix(base::UTF8ToUTF16(payment_instrument.iban().prefix()));
  result.set_suffix(base::UTF8ToUTF16(payment_instrument.iban().suffix()));
  result.set_nickname(base::UTF8ToUTF16(payment_instrument.nickname()));
  return result;
}

BankAccount::AccountType ConvertSyncAccountTypeToBankAccountType(
    sync_pb::BankAccountDetails::AccountType account_type) {
  switch (account_type) {
    case sync_pb::BankAccountDetails_AccountType_CHECKING:
      return BankAccount::AccountType::kChecking;
    case sync_pb::BankAccountDetails_AccountType_SAVINGS:
      return BankAccount::AccountType::kSavings;
    case sync_pb::BankAccountDetails_AccountType_CURRENT:
      return BankAccount::AccountType::kCurrent;
    case sync_pb::BankAccountDetails_AccountType_SALARY:
      return BankAccount::AccountType::kSalary;
    case sync_pb::BankAccountDetails_AccountType_TRANSACTING_ACCOUNT:
      return BankAccount::AccountType::kTransactingAccount;
    case sync_pb::BankAccountDetails_AccountType_ACCOUNT_TYPE_UNSPECIFIED:
      return BankAccount::AccountType::kUnknown;
  }
}

sync_pb::BankAccountDetails::AccountType
ConvertBankAccountTypeToSyncBankAccountType(
    BankAccount::AccountType account_type) {
  switch (account_type) {
    case BankAccount::AccountType::kChecking:
      return sync_pb::BankAccountDetails_AccountType_CHECKING;
    case BankAccount::AccountType::kSavings:
      return sync_pb::BankAccountDetails_AccountType_SAVINGS;
    case BankAccount::AccountType::kCurrent:
      return sync_pb::BankAccountDetails_AccountType_CURRENT;
    case BankAccount::AccountType::kSalary:
      return sync_pb::BankAccountDetails_AccountType_SALARY;
    case BankAccount::AccountType::kTransactingAccount:
      return sync_pb::BankAccountDetails_AccountType_TRANSACTING_ACCOUNT;
    case BankAccount::AccountType::kUnknown:
      return sync_pb::BankAccountDetails_AccountType_ACCOUNT_TYPE_UNSPECIFIED;
  }
}

sync_pb::PaymentInstrument::SupportedRail
ConvertPaymentInstrumentPaymentRailToSyncPaymentRail(
    PaymentInstrument::PaymentRail payment_rail) {
  switch (payment_rail) {
    case PaymentInstrument::PaymentRail::kPix:
      return sync_pb::PaymentInstrument_SupportedRail_PIX;
    case PaymentInstrument::PaymentRail::kPaymentHyperlink:
      return sync_pb::PaymentInstrument_SupportedRail_PAYMENT_HYPERLINK;
    case PaymentInstrument::PaymentRail::kUnknown:
      return sync_pb::PaymentInstrument_SupportedRail_SUPPORTED_RAIL_UNKNOWN;
  }
}

}  // namespace

std::string GetStorageKeyForWalletMetadataTypeAndSpecificsId(
    sync_pb::WalletMetadataSpecifics::Type type,
    const std::string& specifics_id) {
  base::Pickle pickle;
  pickle.WriteInt(static_cast<int>(type));
  // We use the (base64-encoded) |specifics_id| here.
  pickle.WriteString(specifics_id);
  return std::string(pickle.data_as_char(), pickle.size());
}

void SetAutofillWalletSpecificsFromServerCard(
    const CreditCard& card,
    AutofillWalletSpecifics* wallet_specifics,
    bool enforce_utf8) {
  wallet_specifics->set_type(AutofillWalletSpecifics::MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* wallet_card =
      wallet_specifics->mutable_masked_card();

  if (enforce_utf8) {
    wallet_card->set_id(base::Base64Encode(card.server_id()));

    // The billing address id might refer to a local profile guid which doesn't
    // need to be encoded.
    if (base::IsStringUTF8(card.billing_address_id())) {
      wallet_card->set_billing_address_id(card.billing_address_id());
    } else {
      wallet_card->set_billing_address_id(
          base::Base64Encode(card.billing_address_id()));
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

  if (!card.card_art_url().is_empty()) {
    wallet_card->set_card_art_url(card.card_art_url().spec());
  }

  wallet_card->set_product_description(
      base::UTF16ToUTF8(card.product_description()));

  if (!card.product_terms_url().is_empty()) {
    wallet_card->set_product_terms_url(card.product_terms_url().spec());
  }
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
        base::Base64Encode(cloud_token_data.masked_card_id));
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

void SetAutofillWalletSpecificsFromMaskedIban(
    const Iban& iban,
    sync_pb::AutofillWalletSpecifics* wallet_specifics,
    bool enforce_utf8) {
  wallet_specifics->set_type(AutofillWalletSpecifics::PAYMENT_INSTRUMENT);
  sync_pb::PaymentInstrument* wallet_payment_instrument =
      wallet_specifics->mutable_payment_instrument();
  wallet_payment_instrument->set_instrument_id(iban.instrument_id());
  wallet_payment_instrument->set_nickname(base::UTF16ToUTF8(iban.nickname()));
  sync_pb::WalletMaskedIban* masked_iban =
      wallet_payment_instrument->mutable_iban();
  masked_iban->set_prefix(base::UTF16ToUTF8(iban.prefix()));
  masked_iban->set_suffix(base::UTF16ToUTF8(iban.suffix()));
}

void SetAutofillWalletSpecificsFromCardBenefit(
    const CreditCardBenefit& benefit,
    bool enforce_utf8,
    sync_pb::AutofillWalletSpecifics& wallet_specifics) {
  sync_pb::CardBenefit* wallet_benefit =
      wallet_specifics.mutable_masked_card()->add_card_benefit();
  const CreditCardBenefitBase& benefit_base = absl::visit(
      [](const auto& a) -> const CreditCardBenefitBase& { return a; }, benefit);
  if (enforce_utf8) {
    wallet_benefit->set_benefit_id(
        base::Base64Encode(benefit_base.benefit_id().value()));
  } else {
    wallet_benefit->set_benefit_id(benefit_base.benefit_id().value());
  }
  wallet_benefit->set_benefit_description(
      base::UTF16ToUTF8(benefit_base.benefit_description()));
  if (!benefit_base.start_time().is_min()) {
    wallet_benefit->set_start_time_unix_epoch_milliseconds(
        benefit_base.start_time().InMillisecondsSinceUnixEpoch());
  }
  if (!benefit_base.expiry_time().is_max()) {
    wallet_benefit->set_end_time_unix_epoch_milliseconds(
        benefit_base.expiry_time().InMillisecondsSinceUnixEpoch());
  }
  absl::visit(
      base::Overloaded{
          [&wallet_benefit](const CreditCardFlatRateBenefit&) {
            wallet_benefit->mutable_flat_rate_benefit();
          },
          [&wallet_benefit](const CreditCardCategoryBenefit& category_benefit) {
            wallet_benefit->mutable_category_benefit()
                ->set_category_benefit_type(
                    CategoryBenefitTypeFromBenefitCategory(
                        category_benefit.benefit_category()));
          },
          [&wallet_benefit](const CreditCardMerchantBenefit& merchant_benefit) {
            sync_pb::CardBenefit_MerchantBenefit* wallet_merchant_benefit =
                wallet_benefit->mutable_merchant_benefit();
            for (const url::Origin& merchant_origin :
                 merchant_benefit.merchant_domains()) {
              wallet_merchant_benefit->add_merchant_domain(
                  merchant_origin.Serialize());
            }
          },
      },
      benefit);
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

sync_pb::AutofillWalletCredentialSpecifics
AutofillWalletCredentialSpecificsFromStructData(const ServerCvc& server_cvc) {
  sync_pb::AutofillWalletCredentialSpecifics wallet_credential_specifics;
  CHECK(!server_cvc.cvc.empty());
  wallet_credential_specifics.set_instrument_id(
      base::NumberToString(server_cvc.instrument_id));
  wallet_credential_specifics.set_cvc(base::UTF16ToUTF8(server_cvc.cvc));
  wallet_credential_specifics.set_last_updated_time_unix_epoch_millis(
      (server_cvc.last_updated_timestamp - base::Time::UnixEpoch())
          .InMilliseconds());
  return wallet_credential_specifics;
}

ServerCvc AutofillWalletCvcStructDataFromWalletCredentialSpecifics(
    const sync_pb::AutofillWalletCredentialSpecifics&
        wallet_credential_specifics) {
  CHECK(IsAutofillWalletCredentialDataSpecificsValid(
      wallet_credential_specifics));
  int64_t instrument_id;
  base::StringToInt64(wallet_credential_specifics.instrument_id(),
                      &instrument_id);

  return ServerCvc{
      .instrument_id = instrument_id,
      .cvc = base::UTF8ToUTF16(wallet_credential_specifics.cvc()),
      .last_updated_timestamp =
          base::Time::UnixEpoch() +
          base::Milliseconds(wallet_credential_specifics
                                 .last_updated_time_unix_epoch_millis())};
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

BankAccount BankAccountFromWalletSpecifics(
    const sync_pb::PaymentInstrument& payment_instrument) {
  return BankAccount(
      payment_instrument.instrument_id(),
      base::UTF8ToUTF16(payment_instrument.nickname()),
      GURL(payment_instrument.display_icon_url()),
      base::UTF8ToUTF16(payment_instrument.bank_account().bank_name()),
      base::UTF8ToUTF16(
          payment_instrument.bank_account().account_number_suffix()),
      ConvertSyncAccountTypeToBankAccountType(
          payment_instrument.bank_account().account_type()));
}

void SetAutofillWalletSpecificsFromBankAccount(
    const BankAccount& bank_account,
    sync_pb::AutofillWalletSpecifics* wallet_specifics) {
  wallet_specifics->set_type(AutofillWalletSpecifics::PAYMENT_INSTRUMENT);
  sync_pb::PaymentInstrument* wallet_payment_instrument =
      wallet_specifics->mutable_payment_instrument();
  wallet_payment_instrument->set_instrument_id(
      bank_account.payment_instrument().instrument_id());
  wallet_payment_instrument->set_nickname(
      base::UTF16ToUTF8(bank_account.payment_instrument().nickname()));
  wallet_payment_instrument->set_display_icon_url(
      bank_account.payment_instrument().display_icon_url().spec());
  for (PaymentInstrument::PaymentRail supported_payment_rail :
       bank_account.payment_instrument().supported_rails()) {
    wallet_payment_instrument->add_supported_rails(
        ConvertPaymentInstrumentPaymentRailToSyncPaymentRail(
            supported_payment_rail));
  }
  sync_pb::BankAccountDetails* bank_account_details =
      wallet_payment_instrument->mutable_bank_account();
  bank_account_details->set_bank_name(
      base::UTF16ToUTF8(bank_account.bank_name()));
  bank_account_details->set_account_number_suffix(
      base::UTF16ToUTF8(bank_account.account_number_suffix()));
  bank_account_details->set_account_type(
      ConvertBankAccountTypeToSyncBankAccountType(bank_account.account_type()));
}

void SetAutofillWalletSpecificsFromPaymentInstrument(
    const sync_pb::PaymentInstrument& payment_instrument,
    sync_pb::AutofillWalletSpecifics& wallet_specifics) {
  wallet_specifics.set_type(AutofillWalletSpecifics::PAYMENT_INSTRUMENT);
  *wallet_specifics.mutable_payment_instrument() = payment_instrument;
}

void CopyRelevantWalletMetadataAndCvc(
    const PaymentsAutofillTable& table,
    std::vector<CreditCard>* cards_from_server) {
  std::vector<std::unique_ptr<CreditCard>> cards_from_local_storage;
  table.GetServerCreditCards(cards_from_local_storage);

  // Since the number of cards is fairly small, the brute-force search is good
  // enough.
  for (const auto& saved_card : cards_from_local_storage) {
    for (CreditCard& server_card : *cards_from_server) {
      if (saved_card->server_id() == server_card.server_id()) {
        // The wallet data doesn't have the use stats. Use the ones present on
        // disk to not overwrite them with bad data.
        server_card.set_use_count(saved_card->use_count());
        server_card.set_use_date(saved_card->use_date());

        // Wallet data from the server doesn't have the CVC data as it's
        // decoupled. Use the data present in the local storage, to prevent
        // CVC data deletion.
        server_card.set_cvc(saved_card->cvc());

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
    std::vector<CreditCard>& wallet_cards,
    std::vector<Iban>& wallet_ibans,
    std::vector<PaymentsCustomerData>& customer_data,
    std::vector<CreditCardCloudTokenData>& cloud_token_data,
    std::vector<BankAccount>& bank_accounts,
    std::vector<CreditCardBenefit>& benefits,
    std::vector<sync_pb::PaymentInstrument>& payment_instruments) {
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    DCHECK(change->data().specifics.has_autofill_wallet());

    const sync_pb::AutofillWalletSpecifics& autofill_specifics =
        change->data().specifics.autofill_wallet();

    switch (autofill_specifics.type()) {
      case sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD: {
        wallet_cards.push_back(
            CardFromSpecifics(autofill_specifics.masked_card()));

        std::vector<CreditCardBenefit> benefits_from_specifics =
            CreditCardBenefitsFromCardSpecifics(
                autofill_specifics.masked_card());
        benefits.insert(benefits.end(), benefits_from_specifics.begin(),
                        benefits_from_specifics.end());
        break;
      }
      case sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS:
        // POSTAL_ADDRESS is deprecated.
        break;
      case sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA:
        customer_data.push_back(
            CustomerDataFromSpecifics(autofill_specifics.customer_data()));
        break;
      case sync_pb::AutofillWalletSpecifics::CREDIT_CARD_CLOUD_TOKEN_DATA:
        cloud_token_data.push_back(
            CloudTokenDataFromSpecifics(autofill_specifics.cloud_token_data()));
        break;
      case sync_pb::AutofillWalletSpecifics::PAYMENT_INSTRUMENT:
        // Only payment instruments of type bank account are supported. This
        // support is also only available on Android. For other platforms,
        // we'd ignore this type.
        if (AreMaskedBankAccountSupported() &&
            autofill_specifics.payment_instrument().instrument_details_case() ==
                sync_pb::PaymentInstrument::InstrumentDetailsCase::
                    kBankAccount) {
          bank_accounts.push_back(BankAccountFromWalletSpecifics(
              autofill_specifics.payment_instrument()));
        } else if (autofill_specifics.payment_instrument()
                       .instrument_details_case() ==
                   sync_pb::PaymentInstrument::InstrumentDetailsCase::kIban) {
          wallet_ibans.push_back(
              IbanFromSpecifics(autofill_specifics.payment_instrument()));
        } else if (autofill_specifics.payment_instrument()
                           .instrument_details_case() ==
                       sync_pb::PaymentInstrument::InstrumentDetailsCase::
                           kEwalletDetails &&
                   IsEwalletAccountSupported()) {
          payment_instruments.push_back(
              autofill_specifics.payment_instrument());
        }
        break;
      // This entry is deprecated and not supported anymore.
      case sync_pb::AutofillWalletSpecifics::MASKED_IBAN:
      case sync_pb::AutofillWalletSpecifics::UNKNOWN:
        // Just ignore new entry types that the client doesn't know about.
        break;
    }
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

template bool AreAnyItemsDifferent<>(const std::vector<BankAccount>&,
                                     const std::vector<BankAccount>&);

template <class Item>
bool AreAnyItemsDifferent(const std::vector<Item>& old_data,
                          const std::vector<Item>& new_data) {
  if (old_data.size() != new_data.size()) {
    return true;
  }

  return base::MakeFlatSet<Item>(old_data) != base::MakeFlatSet<Item>(new_data);
}

template bool AreAnyItemsDifferent<>(const std::vector<CreditCardBenefit>&,
                                     const std::vector<CreditCardBenefit>&);

template bool AreAnyItemsDifferent<>(const std::vector<std::string>&,
                                     const std::vector<std::string>&);

bool AreAnyItemsDifferent(
    const std::vector<sync_pb::PaymentInstrument>& old_instruments,
    const std::vector<sync_pb::PaymentInstrument>& new_instruments) {
  if (old_instruments.size() != new_instruments.size()) {
    return true;
  }

  std::vector<std::string> old_instrument_strings, new_instrument_strings;
  for (const auto& instrument : old_instruments) {
    old_instrument_strings.push_back(instrument.SerializeAsString());
  }
  for (const auto& instrument : new_instruments) {
    new_instrument_strings.push_back(instrument.SerializeAsString());
  }

  return AreAnyItemsDifferent(old_instrument_strings, new_instrument_strings);
}

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

bool IsAutofillWalletCredentialDataSpecificsValid(
    const sync_pb::AutofillWalletCredentialSpecifics&
        wallet_credential_specifics) {
  int64_t temp_instrument_id;
  return !wallet_credential_specifics.instrument_id().empty() &&
         base::StringToInt64(wallet_credential_specifics.instrument_id(),
                             &temp_instrument_id) &&
         !wallet_credential_specifics.cvc().empty() &&
         wallet_credential_specifics
             .has_last_updated_time_unix_epoch_millis() &&
         wallet_credential_specifics.last_updated_time_unix_epoch_millis() != 0;
}

bool AreMaskedBankAccountSupported() {
#if BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableSyncingOfPixBankAccounts);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool IsEwalletAccountSupported() {
#if BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(features::kAutofillSyncEwalletAccounts);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool IsGenericPaymentInstrumentSupported() {
  // Currently only eWallet account is using generic payment instrument proto
  // for read/write.
  return IsEwalletAccountSupported();
}

}  // namespace autofill
