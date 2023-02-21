// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_types.h"

#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

ServerFieldType ToSafeServerFieldType(
    std::underlying_type_t<ServerFieldType> raw_value,
    ServerFieldType fallback_value) {
  auto IsValid = [](std::underlying_type_t<ServerFieldType> t) {
    return NO_SERVER_DATA <= t && t < MAX_VALID_FIELD_TYPE &&
           // Work phone numbers (values [15,19]) are deprecated.
           !(15 <= t && t <= 19) &&
           // Cell phone numbers (values [25,29]) are deprecated.
           !(25 <= t && t <= 29) &&
           // Shipping addresses (values [44,50]) are deprecated.
           !(44 <= t && t <= 50) &&
           // Probably-account creation password (value 94) is deprecated.
           t != 94 &&
           // Billing addresses (values [37,43], 78, 80, 82, 84) are deprecated.
           !(37 <= t && t <= 43) && t != 78 && t != 80 && t != 82 && t != 84 &&
           // Billing phone numbers (values [62,66]) are deprecated.
           !(62 <= t && t <= 66) &&
           // Billing names (values [67,72]) are deprecated.
           !(67 <= t && t <= 72) &&
           // Fax numbers (values [20,24]) are deprecated.
           !(20 <= t && t <= 24) &&
           // Reserved for server-side only use.
           t != 127 && !(130 <= t && t <= 133);
  };
  return IsValid(raw_value) ? static_cast<ServerFieldType>(raw_value)
                            : fallback_value;
}

bool IsFillableFieldType(ServerFieldType field_type) {
  switch (field_type) {
    case NAME_HONORIFIC_PREFIX:
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
    case NAME_LAST_FIRST:
    case NAME_LAST_CONJUNCTION:
    case NAME_LAST_SECOND:
    case NAME_MIDDLE_INITIAL:
    case NAME_FULL:
    case NAME_FULL_WITH_HONORIFIC_PREFIX:
    case NAME_SUFFIX:
    case EMAIL_ADDRESS:
    case USERNAME_AND_EMAIL_ADDRESS:
    case PHONE_HOME_NUMBER:
    case PHONE_HOME_NUMBER_PREFIX:
    case PHONE_HOME_NUMBER_SUFFIX:
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
    case PHONE_HOME_COUNTRY_CODE:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
    case PHONE_HOME_WHOLE_NUMBER:
    case PHONE_HOME_EXTENSION:
    case ADDRESS_HOME_LINE1:
    case ADDRESS_HOME_LINE2:
    case ADDRESS_HOME_LINE3:
    case ADDRESS_HOME_APT_NUM:
    case ADDRESS_HOME_CITY:
    case ADDRESS_HOME_STATE:
    case ADDRESS_HOME_ZIP:
    case ADDRESS_HOME_COUNTRY:
    case ADDRESS_HOME_STREET_ADDRESS:
    case ADDRESS_HOME_SORTING_CODE:
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
    case ADDRESS_HOME_STREET_NAME:
    case ADDRESS_HOME_DEPENDENT_STREET_NAME:
    case ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME:
    case ADDRESS_HOME_HOUSE_NUMBER:
    case ADDRESS_HOME_PREMISE_NAME:
    case ADDRESS_HOME_SUBPREMISE:
    case ADDRESS_HOME_OTHER_SUBUNIT:
    case ADDRESS_HOME_ADDRESS:
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
    case ADDRESS_HOME_FLOOR:
      return true;

    case CREDIT_CARD_NAME_FULL:
    case CREDIT_CARD_NAME_FIRST:
    case CREDIT_CARD_NAME_LAST:
    case CREDIT_CARD_NUMBER:
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case CREDIT_CARD_TYPE:
    case CREDIT_CARD_VERIFICATION_CODE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      return true;

    case UPI_VPA:
      return base::FeatureList::IsEnabled(features::kAutofillSaveAndFillVPA);

    case IBAN_VALUE:
      return base::FeatureList::IsEnabled(features::kAutofillParseIBANFields);

    case COMPANY_NAME:
      return true;

    case MERCHANT_PROMO_CODE:
      return true;

    // Fillable credential fields.
    case USERNAME:
    case PASSWORD:
    case ACCOUNT_CREATION_PASSWORD:
    case CONFIRMATION_PASSWORD:
    case SINGLE_USERNAME:
      return true;

    // Not fillable credential fields.
    case NOT_PASSWORD:
    case NOT_USERNAME:
      return false;

    // Credential field types that the server should never return as
    // classifications.
    case NOT_ACCOUNT_CREATION_PASSWORD:
    case NEW_PASSWORD:
    case PROBABLY_NEW_PASSWORD:
    case NOT_NEW_PASSWORD:
    case ONE_TIME_CODE:
      return false;

    case NO_SERVER_DATA:
    case EMPTY_TYPE:
    case AMBIGUOUS_TYPE:
    case FIELD_WITH_DEFAULT_VALUE:
    case MERCHANT_EMAIL_SIGNUP:
    case PRICE:
    case NUMERIC_QUANTITY:
    case SEARCH_TERM:
    case BIRTHDATE_DAY:
    case BIRTHDATE_MONTH:
    case BIRTHDATE_4_DIGIT_YEAR:
    case UNKNOWN_TYPE:
    case MAX_VALID_FIELD_TYPE:
      return false;
  }
  return false;
}

base::StringPiece FieldTypeToStringPiece(ServerFieldType type) {
  // You are free to add or remove the String representation of ServerFieldType,
  // but don't change any existing values, Android WebView presents them to
  // Autofill Service as part of APIs.
  switch (type) {
    case NO_SERVER_DATA:
      return "NO_SERVER_DATA";
    case UNKNOWN_TYPE:
      return "UNKNOWN_TYPE";
    case EMPTY_TYPE:
      return "EMPTY_TYPE";
    case NAME_HONORIFIC_PREFIX:
      return "NAME_HONORIFIC_PREFIX";
    case NAME_FULL_WITH_HONORIFIC_PREFIX:
      return "NAME_FULL_WITH_HONORIFIC_PREFIX";
    case NAME_FIRST:
      return "NAME_FIRST";
    case NAME_MIDDLE:
      return "NAME_MIDDLE";
    case NAME_LAST:
      return "NAME_LAST";
    case NAME_LAST_FIRST:
      return "NAME_LAST_FIRST";
    case NAME_LAST_CONJUNCTION:
      return "NAME_LAST_CONJUNCTION";
    case NAME_LAST_SECOND:
      return "NAME_LAST_SECOND";
    case NAME_MIDDLE_INITIAL:
      return "NAME_MIDDLE_INITIAL";
    case NAME_FULL:
      return "NAME_FULL";
    case NAME_SUFFIX:
      return "NAME_SUFFIX";
    case EMAIL_ADDRESS:
      return "EMAIL_ADDRESS";
    case PHONE_HOME_NUMBER:
      return "PHONE_HOME_NUMBER";
    case PHONE_HOME_NUMBER_PREFIX:
      return "PHONE_HOME_NUMBER_PREFIX";
    case PHONE_HOME_NUMBER_SUFFIX:
      return "PHONE_HOME_NUMBER_SUFFIX";
    case PHONE_HOME_CITY_CODE:
      return "PHONE_HOME_CITY_CODE";
    case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
      return "PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX";
    case PHONE_HOME_COUNTRY_CODE:
      return "PHONE_HOME_COUNTRY_CODE";
    case PHONE_HOME_CITY_AND_NUMBER:
      return "PHONE_HOME_CITY_AND_NUMBER";
    case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
      return "PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX";
    case PHONE_HOME_WHOLE_NUMBER:
      return "PHONE_HOME_WHOLE_NUMBER";
    case PHONE_HOME_EXTENSION:
      return "PHONE_HOME_EXTENSION";
    case ADDRESS_HOME_ADDRESS:
      return "ADDRESS_HOME_ADDRESS";
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
      return "ADDRESS_HOME_ADDRESS_WITH_NAME";
    case ADDRESS_HOME_FLOOR:
      return "ADDRESS_HOME_FLOOR";
    case ADDRESS_HOME_LINE1:
      return "ADDRESS_HOME_LINE1";
    case ADDRESS_HOME_LINE2:
      return "ADDRESS_HOME_LINE2";
    case ADDRESS_HOME_LINE3:
      return "ADDRESS_HOME_LINE3";
    case ADDRESS_HOME_APT_NUM:
      return "ADDRESS_HOME_APT_NUM";
    case ADDRESS_HOME_CITY:
      return "ADDRESS_HOME_CITY";
    case ADDRESS_HOME_STATE:
      return "ADDRESS_HOME_STATE";
    case ADDRESS_HOME_ZIP:
      return "ADDRESS_HOME_ZIP";
    case ADDRESS_HOME_COUNTRY:
      return "ADDRESS_HOME_COUNTRY";
    case BIRTHDATE_DAY:
      return "BIRTHDATE_DAY";
    case BIRTHDATE_MONTH:
      return "BIRTHDATE_MONTH";
    case BIRTHDATE_4_DIGIT_YEAR:
      return "BIRTHDATE_YEAR_4_DIGITS";
    case CREDIT_CARD_NAME_FULL:
      return "CREDIT_CARD_NAME_FULL";
    case CREDIT_CARD_NAME_FIRST:
      return "CREDIT_CARD_NAME_FIRST";
    case CREDIT_CARD_NAME_LAST:
      return "CREDIT_CARD_NAME_LAST";
    case CREDIT_CARD_NUMBER:
      return "CREDIT_CARD_NUMBER";
    case CREDIT_CARD_EXP_MONTH:
      return "CREDIT_CARD_EXP_MONTH";
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_2_DIGIT_YEAR";
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_4_DIGIT_YEAR";
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR";
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR";
    case CREDIT_CARD_TYPE:
      return "CREDIT_CARD_TYPE";
    case CREDIT_CARD_VERIFICATION_CODE:
      return "CREDIT_CARD_VERIFICATION_CODE";
    case COMPANY_NAME:
      return "COMPANY_NAME";
    case FIELD_WITH_DEFAULT_VALUE:
      return "FIELD_WITH_DEFAULT_VALUE";
    case MERCHANT_EMAIL_SIGNUP:
      return "MERCHANT_EMAIL_SIGNUP";
    case MERCHANT_PROMO_CODE:
      return "MERCHANT_PROMO_CODE";
    case PASSWORD:
      return "PASSWORD";
    case ACCOUNT_CREATION_PASSWORD:
      return "ACCOUNT_CREATION_PASSWORD";
    case ADDRESS_HOME_STREET_ADDRESS:
      return "ADDRESS_HOME_STREET_ADDRESS";
    case ADDRESS_HOME_SORTING_CODE:
      return "ADDRESS_HOME_SORTING_CODE";
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      return "ADDRESS_HOME_DEPENDENT_LOCALITY";
    case NOT_ACCOUNT_CREATION_PASSWORD:
      return "NOT_ACCOUNT_CREATION_PASSWORD";
    case USERNAME:
      return "USERNAME";
    case USERNAME_AND_EMAIL_ADDRESS:
      return "USERNAME_AND_EMAIL_ADDRESS";
    case NEW_PASSWORD:
      return "NEW_PASSWORD";
    case PROBABLY_NEW_PASSWORD:
      return "PROBABLY_NEW_PASSWORD";
    case NOT_NEW_PASSWORD:
      return "NOT_NEW_PASSWORD";
    case CONFIRMATION_PASSWORD:
      return "CONFIRMATION_PASSWORD";
    case SEARCH_TERM:
      return "SEARCH_TERM";
    case PRICE:
      return "PRICE";
    case NUMERIC_QUANTITY:
      return "NUMERIC_QUANTITY";
    case NOT_PASSWORD:
      return "NOT_PASSWORD";
    case SINGLE_USERNAME:
      return "SINGLE_USERNAME";
    case NOT_USERNAME:
      return "NOT_USERNAME";
    case UPI_VPA:
      return "UPI_VPA";
    case ADDRESS_HOME_STREET_NAME:
      return "ADDRESS_HOME_STREET_NAME";
    case ADDRESS_HOME_DEPENDENT_STREET_NAME:
      return "ADDRESS_HOME_DEPENDENT_STREET_NAME";
    case ADDRESS_HOME_HOUSE_NUMBER:
      return "ADDRESS_HOME_HOUSE_NUMBER";
    case ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME:
      return "ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME";
    case ADDRESS_HOME_PREMISE_NAME:
      return "ADDRESS_HOME_PREMISE_NAME";
    case ADDRESS_HOME_SUBPREMISE:
      return "ADDRESS_HOME_SUBPREMISE";
    case ADDRESS_HOME_OTHER_SUBUNIT:
      return "ADDRESS_HOME_OTHER_SUBUNIT";
    case AMBIGUOUS_TYPE:
      return "AMBIGUOUS_TYPE";
    case IBAN_VALUE:
      return "IBAN_VALUE";
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      return "CREDIT_CARD_STANDALONE_VERIFICATION_CODE";
    case ONE_TIME_CODE:
      return "ONE_TIME_CODE";
    case MAX_VALID_FIELD_TYPE:
      return "";
  }

  NOTREACHED();
  return "";
}

std::ostream& operator<<(std::ostream& o, ServerFieldTypeSet field_type_set) {
  o << "[";
  bool first = true;
  for (const auto type : field_type_set) {
    if (!first) {
      o << ", ";
    } else {
      first = false;
    }
    o << FieldTypeToStringPiece(type);
  }
  o << "]";
  return o;
}

}  // namespace autofill
