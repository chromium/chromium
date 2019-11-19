// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_

#include <set>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/mojom/payment_request_data.mojom.h"
#include "url/gurl.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace payments {

struct BasicCardResponse;
class PaymentMethodData;

namespace data_util {

// Helper function to get an instance of PaymentAddressPtr from an autofill
// profile.
mojom::PaymentAddressPtr GetPaymentAddressFromAutofillProfile(
    const autofill::AutofillProfile& profile,
    const std::string& app_locale);

// Helper function to get an instance of web::BasicCardResponse from an autofill
// credit card.
std::unique_ptr<BasicCardResponse> GetBasicCardResponseFromAutofillCreditCard(
    const autofill::CreditCard& card,
    const base::string16& cvc,
    const autofill::AutofillProfile& billing_profile,
    const std::string& app_locale);

// Parse all the supported payment methods from the merchant including 1) the
// supported card networks from supportedMethods and  "basic-card"'s
// supportedNetworks and 2) the url-based payment method identifiers.
// |out_supported_networks| is filled with a list of networks
// in the order that they were specified by the merchant.
// |out_basic_card_supported_networks| is a subset of |out_supported_networks|
// that includes all networks that were specified as part of "basic-card". This
// is used to know whether to return the card network name (e.g., "visa") or
// "basic-card" in the PaymentResponse. |method_data.supported_networks| is
// expected to only contain basic-card card network names (the list is at
// https://www.w3.org/Payments/card-network-ids).
// |out_url_payment_method_identifiers| is filled with a list of all the
// payment method identifiers specified by the merchant that are URL-based.
void ParseSupportedMethods(
    const std::vector<PaymentMethodData>& method_data,
    std::vector<std::string>* out_supported_networks,
    std::set<std::string>* out_basic_card_supported_networks,
    std::vector<GURL>* out_url_payment_method_identifiers,
    std::set<std::string>* out_payment_method_identifiers);

// Parses the supported card types (e.g., credit, debit, prepaid) from
// supportedTypes. |out_supported_card_types_set| is expected to be empty. It
// will always contain autofill::CreditCard::CARD_TYPE_UNKNOWN after the call.
// Also, it gets filled with all of the card types if supportedTypes is empty.
void ParseSupportedCardTypes(
    const std::vector<PaymentMethodData>& method_data,
    std::set<autofill::CreditCard::CardType>* out_supported_card_types_set);

// Formats |card_number| for display. For example, "4111111111111111" is
// formatted into "4111 1111 1111 1111". This method does not format masked card
// numbers, which start with a letter.
base::string16 FormatCardNumberForDisplay(const base::string16& card_number);

}  // namespace data_util
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_
