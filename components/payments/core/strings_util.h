// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_STRINGS_UTIL_H_
#define COMPONENTS_PAYMENTS_CORE_STRINGS_UTIL_H_

#include <set>
#include <string>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/core/payment_options_provider.h"

namespace autofill {
class AutofillProfile;
}

namespace payments {

// Helper function to create a shipping address label from an autofill profile.
base::string16 GetShippingAddressLabelFormAutofillProfile(
    const autofill::AutofillProfile& profile,
    const std::string& locale);

// Helper function to create a billing address label from an autofill profile.
base::string16 GetBillingAddressLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile,
    const std::string& locale);

// Gets the informational message to be displayed in the shipping address
// selector view when there are no valid shipping options.
base::string16 GetShippingAddressSelectorInfoMessage(
    PaymentShippingType shipping_type);

// Gets the appropriate display string for the Shipping Address string for the
// given PaymentShippingType.
base::string16 GetShippingAddressSectionString(
    PaymentShippingType shipping_type);

#if defined(OS_IOS)
// Gets the appropriate display string for the Choose Shipping Address string
// for the given PaymentShippingType.
base::string16 GetChooseShippingAddressButtonLabel(
    PaymentShippingType shipping_type);

// Gets the appropriate display string for the Add Shipping Address string
// for the given PaymentShippingType.
base::string16 GetAddShippingAddressButtonLabel(
    PaymentShippingType shipping_type);

// Gets the appropriate display string for the Choose Shipping Option string for
// the given PaymentShippingType.
base::string16 GetChooseShippingOptionButtonLabel(
    PaymentShippingType shipping_type);
#endif  // defined(OS_IOS)

// Gets the appropriate display string for the Shipping Option string for the
// given PaymentShippingType.
base::string16 GetShippingOptionSectionString(
    PaymentShippingType shipping_type);

// Returns the label "Accepted cards" that is customized based on the
// accepted card |types|. For example, "Accepted debit cards". If |types| is
// empty or contains all possible values, then returns the generic "Accepted
// cards" string.
base::string16 GetAcceptedCardTypesText(
    const std::set<autofill::CreditCard::CardType>& types);

// Returns the label "Cards are accepted" that is customized based on the
// accepted card |types|. For example, "Debit cards are accepted". If |types| is
// empty or contains all possible values, then returns an empty string.
base::string16 GetCardTypesAreAcceptedText(
    const std::set<autofill::CreditCard::CardType>& types);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_STRINGS_UTIL_H_
