// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_request_data_util.h"

#include <memory>

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/payments/core/basic_card_response.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payment_method_data.h"
#include "components/payments/core/payments_validators.h"
#include "components/payments/core/url_util.h"
#include "net/base/url_util.h"
#include "url/url_constants.h"

namespace payments {
namespace data_util {

mojom::PaymentAddressPtr GetPaymentAddressFromAutofillProfile(
    const autofill::AutofillProfile& profile,
    const std::string& app_locale) {
  mojom::PaymentAddressPtr payment_address = mojom::PaymentAddress::New();

  if (profile.IsEmpty(app_locale))
    return payment_address;

  payment_address->country =
      base::UTF16ToUTF8(profile.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  DCHECK(PaymentsValidators::IsValidCountryCodeFormat(payment_address->country,
                                                      nullptr));

  payment_address->address_line =
      base::SplitString(base::UTF16ToUTF8(profile.GetInfo(
                            autofill::ADDRESS_HOME_STREET_ADDRESS, app_locale)),
                        "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  payment_address->region = base::UTF16ToUTF8(
      profile.GetInfo(autofill::ADDRESS_HOME_STATE, app_locale));
  payment_address->city = base::UTF16ToUTF8(
      profile.GetInfo(autofill::ADDRESS_HOME_CITY, app_locale));
  payment_address->dependent_locality = base::UTF16ToUTF8(
      profile.GetInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY, app_locale));
  payment_address->postal_code = base::UTF16ToUTF8(
      profile.GetInfo(autofill::ADDRESS_HOME_ZIP, app_locale));
  payment_address->sorting_code = base::UTF16ToUTF8(
      profile.GetInfo(autofill::ADDRESS_HOME_SORTING_CODE, app_locale));
  payment_address->organization =
      base::UTF16ToUTF8(profile.GetInfo(autofill::COMPANY_NAME, app_locale));
  payment_address->recipient =
      base::UTF16ToUTF8(profile.GetInfo(autofill::NAME_FULL, app_locale));

  // TODO(crbug.com/705945): Format phone number according to spec.
  payment_address->phone =
      base::UTF16ToUTF8(profile.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER));

  return payment_address;
}

std::unique_ptr<BasicCardResponse> GetBasicCardResponseFromAutofillCreditCard(
    const autofill::CreditCard& card,
    const base::string16& cvc,
    const autofill::AutofillProfile& billing_profile,
    const std::string& app_locale) {
  std::unique_ptr<BasicCardResponse> response =
      std::make_unique<BasicCardResponse>();
  response->cardholder_name = card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL);
  response->card_number = card.GetRawInfo(autofill::CREDIT_CARD_NUMBER);
  response->expiry_month = card.GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH);
  response->expiry_year =
      card.GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);
  response->card_security_code = cvc;

  response->billing_address =
      GetPaymentAddressFromAutofillProfile(billing_profile, app_locale);

  return response;
}

void ParseSupportedMethods(
    const std::vector<PaymentMethodData>& method_data,
    std::vector<std::string>* out_supported_networks,
    std::set<std::string>* out_basic_card_specified_networks,
    std::vector<GURL>* out_url_payment_method_identifiers,
    std::set<std::string>* out_payment_method_identifiers) {
  DCHECK(out_supported_networks->empty());
  DCHECK(out_basic_card_specified_networks->empty());
  DCHECK(out_url_payment_method_identifiers->empty());
  DCHECK(out_payment_method_identifiers->empty());

  const std::set<std::string> kBasicCardNetworks{
      "amex",       "diners", "discover", "jcb",
      "mastercard", "mir",    "unionpay", "visa"};

  std::set<GURL> url_payment_method_identifiers;

  for (const PaymentMethodData& method_data_entry : method_data) {
    if (method_data_entry.supported_method.empty())
      return;

    out_payment_method_identifiers->insert(method_data_entry.supported_method);

    if (method_data_entry.supported_method == methods::kBasicCard) {
      if (method_data_entry.supported_networks.empty()) {
        // Empty |supported_networks| means all networks are supported.
        out_supported_networks->insert(out_supported_networks->end(),
                                       kBasicCardNetworks.begin(),
                                       kBasicCardNetworks.end());
        out_basic_card_specified_networks->insert(kBasicCardNetworks.begin(),
                                                  kBasicCardNetworks.end());
      } else {
        // The merchant has specified a few basic card supported networks. Use
        // the mapping to transform to known basic-card types.
        for (const std::string& supported_network :
             method_data_entry.supported_networks) {
          if (kBasicCardNetworks.find(supported_network) !=
                  kBasicCardNetworks.end() &&
              out_basic_card_specified_networks->find(supported_network) ==
                  out_basic_card_specified_networks->end()) {
            out_supported_networks->push_back(supported_network);
            out_basic_card_specified_networks->insert(supported_network);
          }
        }
      }
    } else {
      // Here |method_data_entry.supported_method| could be a deprecated
      // supported network (e.g., "visa"), some invalid string or a URL-based
      // payment method identifier. Capture this last category if it is valid.
      // Avoid duplicates.
      GURL url(method_data_entry.supported_method);
      if (UrlUtil::IsValidUrlBasedPaymentMethodIdentifier(url)) {
        const auto result = url_payment_method_identifiers.insert(url);
        if (result.second)
          out_url_payment_method_identifiers->push_back(url);
      }
    }
  }
}

void ParseSupportedCardTypes(
    const std::vector<PaymentMethodData>& method_data,
    std::set<autofill::CreditCard::CardType>* out_supported_card_types_set) {
  DCHECK(out_supported_card_types_set->empty());

  for (const PaymentMethodData& method_data_entry : method_data) {
    // Ignore |supported_types| if |supported_method| is not "basic-card".
    if (method_data_entry.supported_method != methods::kBasicCard)
      continue;

    for (const autofill::CreditCard::CardType& card_type :
         method_data_entry.supported_types) {
      out_supported_card_types_set->insert(card_type);
    }
  }

  // Omitting the card types means all 3 card types are supported.
  if (out_supported_card_types_set->empty()) {
    out_supported_card_types_set->insert(
        autofill::CreditCard::CARD_TYPE_CREDIT);
    out_supported_card_types_set->insert(autofill::CreditCard::CARD_TYPE_DEBIT);
    out_supported_card_types_set->insert(
        autofill::CreditCard::CARD_TYPE_PREPAID);
  }

  // Let the user decide whether an unknown card type should be used.
  out_supported_card_types_set->insert(autofill::CreditCard::CARD_TYPE_UNKNOWN);
}

base::string16 FormatCardNumberForDisplay(const base::string16& card_number) {
  base::string16 number = autofill::CreditCard::StripSeparators(card_number);
  if (number.empty() || !base::IsAsciiDigit(number[0]))
    return card_number;

  std::vector<size_t> positions = {4U, 9U, 14U};
  if (autofill::CreditCard::GetCardNetwork(number) ==
      autofill::kAmericanExpressCard) {
    positions = {4U, 11U};
  }

  static const base::char16 kSeparator = base::ASCIIToUTF16(" ")[0];
  for (size_t i : positions) {
    if (number.size() > i)
      number.insert(i, 1U, kSeparator);
  }

  return number;
}

}  // namespace data_util
}  // namespace payments
