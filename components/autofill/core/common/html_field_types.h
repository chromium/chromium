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
enum HtmlFieldType {
  // Default type.
  HTML_TYPE_UNSPECIFIED,

  // Name types.
  HTML_TYPE_NAME,
  HTML_TYPE_HONORIFIC_PREFIX,
  HTML_TYPE_GIVEN_NAME,
  HTML_TYPE_ADDITIONAL_NAME,
  HTML_TYPE_FAMILY_NAME,

  // Business types.
  HTML_TYPE_ORGANIZATION,

  // Address types.
  HTML_TYPE_STREET_ADDRESS,
  HTML_TYPE_ADDRESS_LINE1,
  HTML_TYPE_ADDRESS_LINE2,
  HTML_TYPE_ADDRESS_LINE3,
  HTML_TYPE_ADDRESS_LEVEL1,  // For U.S. addresses, corresponds to the state.
  HTML_TYPE_ADDRESS_LEVEL2,  // For U.S. addresses, corresponds to the city.
  HTML_TYPE_ADDRESS_LEVEL3,  // An area that is more specific than LEVEL2.
  HTML_TYPE_COUNTRY_CODE,    // The ISO 3166-1-alpha-2 country code.
  HTML_TYPE_COUNTRY_NAME,    // The localized country name.
  HTML_TYPE_POSTAL_CODE,
  HTML_TYPE_FULL_ADDRESS,  // The complete address, formatted for display.

  // Credit card types.
  HTML_TYPE_CREDIT_CARD_NAME_FULL,
  HTML_TYPE_CREDIT_CARD_NAME_FIRST,
  HTML_TYPE_CREDIT_CARD_NAME_LAST,
  HTML_TYPE_CREDIT_CARD_NUMBER,
  HTML_TYPE_CREDIT_CARD_EXP,
  HTML_TYPE_CREDIT_CARD_EXP_MONTH,
  HTML_TYPE_CREDIT_CARD_EXP_YEAR,
  HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE,
  HTML_TYPE_CREDIT_CARD_TYPE,

  // Phone number types.
  HTML_TYPE_TEL,
  HTML_TYPE_TEL_COUNTRY_CODE,
  HTML_TYPE_TEL_NATIONAL,
  HTML_TYPE_TEL_AREA_CODE,
  HTML_TYPE_TEL_LOCAL,
  HTML_TYPE_TEL_LOCAL_PREFIX,
  HTML_TYPE_TEL_LOCAL_SUFFIX,
  HTML_TYPE_TEL_EXTENSION,

  // Email.
  HTML_TYPE_EMAIL,

  // Birthdate.
  HTML_TYPE_BIRTHDATE_DAY,
  HTML_TYPE_BIRTHDATE_MONTH,
  HTML_TYPE_BIRTHDATE_YEAR,

  // Transaction details.
  HTML_TYPE_TRANSACTION_AMOUNT,
  HTML_TYPE_TRANSACTION_CURRENCY,

  // Variants of type hints specified in the HTML specification that are
  // inferred based on a field's 'maxlength' attribute.
  // TODO(isherman): Remove these types, in favor of understanding maxlength
  // when filling fields.  See also: AutofillField::phone_part_.
  HTML_TYPE_ADDITIONAL_NAME_INITIAL,
  HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
  HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
  HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR,
  HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR,

  // Universal Payment Interface - Virtual Payment Address.
  HTML_TYPE_UPI_VPA,

  // Phone number verification one-time-codes.
  HTML_TYPE_ONE_TIME_CODE,

  // Promo code for merchant sites.
  HTML_TYPE_MERCHANT_PROMO_CODE,

  // International Bank Account Number (IBAN) for banking and merchant sites.
  HTML_TYPE_IBAN,

  // Non-standard autocomplete types.
  HTML_TYPE_UNRECOGNIZED,
};

// The list of all HTML autocomplete field mode hints supported by Chrome.
// See [ http://is.gd/whatwg_autocomplete ] for the full list of specced hints.
enum HtmlFieldMode : uint8_t {
  HTML_MODE_NONE,
  HTML_MODE_BILLING,
  HTML_MODE_SHIPPING,
};

// Maps HTML_MODE_BILLING and HTML_MODE_SHIPPING to their string constants, as
// specified in the autocomplete standard.
base::StringPiece HtmlFieldModeToStringPiece(HtmlFieldMode mode);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_HTML_FIELD_TYPES_H_
