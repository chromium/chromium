// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_HTML_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_HTML_FIELD_TYPES_H_

#include <stdint.h>
#include "base/strings/string_piece_forward.h"

namespace autofill {

// The list of all HTML autocomplete field type hints supported by Chrome.
// See [ http://is.gd/whatwg_autocomplete ] for the full list of specced hints.
enum class HtmlFieldType {
  // Default type.
  kUnspecified,

  // Name types.
  kName,
  kHonorificPrefix,
  kGivenName,
  kAdditionalName,
  kFamilyName,

  // Business types.
  kOrganization,

  // Address types.
  kStreetAddress,
  kAddressLine1,
  kAddressLine2,
  kAddressLine3,
  kAddressLevel1,  // For U.S. addresses, corresponds to the state.
  kAddressLevel2,  // For U.S. addresses, corresponds to the city.
  kAddressLevel3,  // An area that is more specific than LEVEL2.
  kCountryCode,    // The ISO 3166-1-alpha-2 country code.
  kCountryName,    // The localized country name.
  kPostalCode,
  kFullAddress,  // The complete address, formatted for display.

  // Credit card types.
  kCreditCardNameFull,
  kCreditCardNameFirst,
  kCreditCardNameLast,
  kCreditCardNumber,
  kCreditCardExp,
  kCreditCardExpMonth,
  kCreditCardExpYear,
  kCreditCardVerificationCode,
  kCreditCardType,

  // Phone number types.
  kTel,
  kTelCountryCode,
  kTelNational,
  kTelAreaCode,
  kTelLocal,
  kTelLocalPrefix,
  kTelLocalSuffix,
  kTelExtension,

  // Email.
  kEmail,

  // Birthdate.
  kBirthdateDay,
  kBirthdateMonth,
  kBirthdateYear,

  // Transaction details.
  kTransactionAmount,
  kTransactionCurrency,

  // Variants of type hints specified in the HTML specification that are
  // inferred based on a field's 'maxlength' attribute.
  // TODO(isherman): Remove these types, in favor of understanding maxlength
  // when filling fields.  See also: AutofillField::phone_part_.
  kAdditionalNameInitial,
  kCreditCardExpDate2DigitYear,
  kCreditCardExpDate4DigitYear,
  kCreditCardExp2DigitYear,
  kCreditCardExp4DigitYear,

  // Universal Payment Interface - Virtual Payment Address.
  kUpiVpa,

  // Phone number verification one-time-codes.
  kOneTimeCode,

  // Promo code for merchant sites.
  kMerchantPromoCode,

  // International Bank Account Number (IBAN) for banking and merchant sites.
  kIban,

  // Non-standard autocomplete types.
  kUnrecognized,

  kMaxValue = kUnrecognized,
};

// The list of all HTML autocomplete field mode hints supported by Chrome.
// See [ http://is.gd/whatwg_autocomplete ] for the full list of specced hints.
enum class HtmlFieldMode : uint8_t {
  kNone,
  kBilling,
  kShipping,
  kMaxValue = kShipping,
};

// Maps HtmlFieldMode::kBilling and HtmlFieldMode::kShipping to
// their string constants, as specified in the autocomplete standard.
base::StringPiece HtmlFieldModeToStringPiece(HtmlFieldMode mode);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_HTML_FIELD_TYPES_H_
