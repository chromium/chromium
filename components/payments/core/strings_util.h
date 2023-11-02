// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_STRINGS_UTIL_H_
#define COMPONENTS_PAYMENTS_CORE_STRINGS_UTIL_H_

#include <set>
#include <string>

#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/core/payment_options_provider.h"

namespace autofill {
class AutofillProfile;
}

namespace payments {

// Helper function to create a shipping address label from an autofill profile.
std::u16string GetShippingAddressLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile,
    const std::string& locale);

// Gets the informational message to be displayed in the shipping address
// selector view when there are no valid shipping options.
std::u16string GetShippingAddressSelectorInfoMessage(
    PaymentShippingType shipping_type);

// Gets the appropriate display string for the Shipping Address string for the
// given PaymentShippingType.
std::u16string GetShippingAddressSectionString(
    PaymentShippingType shipping_type);

// Gets the appropriate display string for the Shipping Option string for the
// given PaymentShippingType.
std::u16string GetShippingOptionSectionString(
    PaymentShippingType shipping_type);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_STRINGS_UTIL_H_
