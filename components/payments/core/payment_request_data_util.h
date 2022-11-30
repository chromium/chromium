// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "components/payments/mojom/payment_request_data.mojom.h"
#include "url/gurl.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace payments {

class PaymentMethodData;

namespace data_util {

// Helper function to get an instance of PaymentAddressPtr from an autofill
// profile.
mojom::PaymentAddressPtr GetPaymentAddressFromAutofillProfile(
    const autofill::AutofillProfile& profile,
    const std::string& app_locale);

// Parse the supported URL payment methods from the merchant.
// |out_url_payment_method_identifiers| is filled with a list of all the
// payment method identifiers specified by the merchant that are URL-based.
void ParseSupportedMethods(
    const std::vector<PaymentMethodData>& method_data,
    std::vector<GURL>* out_url_payment_method_identifiers,
    std::set<std::string>* out_payment_method_identifiers);

// Returns the subset of |stringified_method_data| map where the keys are in the
// |supported_payment_method_names| set. Used for ensuring that a payment app
// will not be queried about payment method names that it does not support.
//
// FilterStringifiedMethodData({"a": {"b"}: "c": {"d"}}, {"a"}) -> {"a": {"b"}}
//
// Both the return value and the first parameter to the function have the
// following format:
// Key: Payment method identifier, such as "example-test" or
//      "https://example.test".
// Value: The set of all payment method specific parameters for the given
//        payment method identifier, each one serialized into a JSON string,
//        e.g., '{"key": "value"}'.
std::unique_ptr<std::map<std::string, std::set<std::string>>>
FilterStringifiedMethodData(
    const std::map<std::string, std::set<std::string>>& stringified_method_data,
    const std::set<std::string>& supported_payment_method_names);

}  // namespace data_util
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_
