// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/strings_util.h"

#include <vector>

#include "base/notreached.h"
#include "base/stl_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

std::u16string GetShippingAddressLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile,
    const std::string& locale) {
  // Name, phone number, and country are not included in the shipping address
  // label.
  static constexpr autofill::ServerFieldType kLabelFields[] = {
      autofill::COMPANY_NAME,
      autofill::ADDRESS_HOME_STREET_ADDRESS,
      autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
      autofill::ADDRESS_HOME_CITY,
      autofill::ADDRESS_HOME_STATE,
      autofill::ADDRESS_HOME_ZIP,
      autofill::ADDRESS_HOME_SORTING_CODE,
  };

  return profile.ConstructInferredLabel(kLabelFields, base::size(kLabelFields),
                                        base::size(kLabelFields), locale);
}

std::u16string GetBillingAddressLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile,
    const std::string& locale) {
  // Name, company, phone number, and country are not included in the billing
  // address label.
  static constexpr autofill::ServerFieldType kLabelFields[] = {
      autofill::ADDRESS_HOME_STREET_ADDRESS,
      autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
      autofill::ADDRESS_HOME_CITY,
      autofill::ADDRESS_HOME_STATE,
      autofill::ADDRESS_HOME_ZIP,
      autofill::ADDRESS_HOME_SORTING_CODE,
  };

  return profile.ConstructInferredLabel(kLabelFields, base::size(kLabelFields),
                                        base::size(kLabelFields), locale);
}

std::u16string GetShippingAddressSelectorInfoMessage(
    PaymentShippingType shipping_type) {
  switch (shipping_type) {
    case payments::PaymentShippingType::DELIVERY:
      return l10n_util::GetStringUTF16(
          IDS_PAYMENTS_SELECT_DELIVERY_ADDRESS_FOR_DELIVERY_METHODS);
    case payments::PaymentShippingType::PICKUP:
      return l10n_util::GetStringUTF16(
          IDS_PAYMENTS_SELECT_PICKUP_ADDRESS_FOR_PICKUP_METHODS);
    case payments::PaymentShippingType::SHIPPING:
      return l10n_util::GetStringUTF16(
          IDS_PAYMENTS_SELECT_SHIPPING_ADDRESS_FOR_SHIPPING_METHODS);
    default:
      NOTREACHED();
      return std::u16string();
  }
}

std::u16string GetShippingAddressSectionString(
    PaymentShippingType shipping_type) {
  switch (shipping_type) {
    case PaymentShippingType::DELIVERY:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_DELIVERY_ADDRESS_LABEL);
    case PaymentShippingType::PICKUP:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_PICKUP_ADDRESS_LABEL);
    case PaymentShippingType::SHIPPING:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL);
    default:
      NOTREACHED();
      return std::u16string();
  }
}

#if defined(OS_IOS)
std::u16string GetChooseShippingAddressButtonLabel(
    PaymentShippingType shipping_type) {
  switch (shipping_type) {
    case PaymentShippingType::DELIVERY:
      return l10n_util::GetStringUTF16(
          IDS_PAYMENTS_CHOOSE_DELIVERY_ADDRESS_LABEL);
    case PaymentShippingType::PICKUP:
      return l10n_util::GetStringUTF16(
          IDS_PAYMENTS_CHOOSE_PICKUP_ADDRESS_LABEL);
    case PaymentShippingType::SHIPPING:
      return l10n_util::GetStringUTF16(
          IDS_PAYMENTS_CHOOSE_SHIPPING_ADDRESS_LABEL);
    default:
      NOTREACHED();
      return std::u16string();
  }
}

std::u16string GetAddShippingAddressButtonLabel(
    PaymentShippingType shipping_type) {
  switch (shipping_type) {
    case PaymentShippingType::DELIVERY:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_DELIVERY_ADDRESS_LABEL);
    case PaymentShippingType::PICKUP:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_PICKUP_ADDRESS_LABEL);
    case PaymentShippingType::SHIPPING:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_SHIPPING_ADDRESS_LABEL);
    default:
      NOTREACHED();
      return std::u16string();
  }
}

std::u16string GetChooseShippingOptionButtonLabel(
    PaymentShippingType shipping_type) {
  switch (shipping_type) {
    case PaymentShippingType::DELIVERY:
      return l10n_util::GetStringUTF16(
          IDS_PAYMENTS_CHOOSE_DELIVERY_OPTION_LABEL);
    case PaymentShippingType::PICKUP:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_CHOOSE_PICKUP_OPTION_LABEL);
    case PaymentShippingType::SHIPPING:
      return l10n_util::GetStringUTF16(
          IDS_PAYMENTS_CHOOSE_SHIPPING_OPTION_LABEL);
    default:
      NOTREACHED();
      return std::u16string();
  }
}
#endif  // defined(OS_IOS)

std::u16string GetShippingOptionSectionString(
    PaymentShippingType shipping_type) {
  switch (shipping_type) {
    case PaymentShippingType::DELIVERY:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_DELIVERY_OPTION_LABEL);
    case PaymentShippingType::PICKUP:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_PICKUP_OPTION_LABEL);
    case PaymentShippingType::SHIPPING:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_SHIPPING_OPTION_LABEL);
    default:
      NOTREACHED();
      return std::u16string();
  }
}

}  // namespace payments
