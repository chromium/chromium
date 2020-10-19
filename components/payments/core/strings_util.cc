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

base::string16 GetShippingAddressLabelFromAutofillProfile(
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

base::string16 GetBillingAddressLabelFromAutofillProfile(
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

base::string16 GetShippingAddressSelectorInfoMessage(
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
      return base::string16();
  }
}

base::string16 GetShippingAddressSectionString(
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
      return base::string16();
  }
}

#if defined(OS_IOS)
base::string16 GetChooseShippingAddressButtonLabel(
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
      return base::string16();
  }
}

base::string16 GetAddShippingAddressButtonLabel(
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
      return base::string16();
  }
}

base::string16 GetChooseShippingOptionButtonLabel(
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
      return base::string16();
  }
}
#endif  // defined(OS_IOS)

base::string16 GetShippingOptionSectionString(
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
      return base::string16();
  }
}

}  // namespace payments
