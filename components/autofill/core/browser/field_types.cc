// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_types.h"

#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// This map should be extended for every added ServerFieldType.
// You are free to add or remove the String representation of ServerFieldType,
// but don't change any existing values, Android WebView presents them to
// Autofill Service as part of APIs.
static constexpr auto kTypeNameToFieldType =
    base::MakeFixedFlatMap<base::StringPiece, ServerFieldType>(
        {{"NO_SERVER_DATA", NO_SERVER_DATA},
         {"UNKNOWN_TYPE", UNKNOWN_TYPE},
         {"EMPTY_TYPE", EMPTY_TYPE},
         {"NAME_FIRST", NAME_FIRST},
         {"NAME_MIDDLE", NAME_MIDDLE},
         {"NAME_LAST", NAME_LAST},
         {"NAME_MIDDLE_INITIAL", NAME_MIDDLE_INITIAL},
         {"NAME_FULL", NAME_FULL},
         {"NAME_SUFFIX", NAME_SUFFIX},
         {"EMAIL_ADDRESS", EMAIL_ADDRESS},
         {"PHONE_HOME_NUMBER", PHONE_HOME_NUMBER},
         {"PHONE_HOME_CITY_CODE", PHONE_HOME_CITY_CODE},
         {"PHONE_HOME_COUNTRY_CODE", PHONE_HOME_COUNTRY_CODE},
         {"PHONE_HOME_CITY_AND_NUMBER", PHONE_HOME_CITY_AND_NUMBER},
         {"PHONE_HOME_WHOLE_NUMBER", PHONE_HOME_WHOLE_NUMBER},
         {"ADDRESS_HOME_LINE1", ADDRESS_HOME_LINE1},
         {"ADDRESS_HOME_LINE2", ADDRESS_HOME_LINE2},
         {"ADDRESS_HOME_APT_NUM", ADDRESS_HOME_APT_NUM},
         {"ADDRESS_HOME_CITY", ADDRESS_HOME_CITY},
         {"ADDRESS_HOME_STATE", ADDRESS_HOME_STATE},
         {"ADDRESS_HOME_ZIP", ADDRESS_HOME_ZIP},
         {"ADDRESS_HOME_COUNTRY", ADDRESS_HOME_COUNTRY},
         {"CREDIT_CARD_NAME_FULL", CREDIT_CARD_NAME_FULL},
         {"CREDIT_CARD_NUMBER", CREDIT_CARD_NUMBER},
         {"CREDIT_CARD_EXP_MONTH", CREDIT_CARD_EXP_MONTH},
         {"CREDIT_CARD_EXP_2_DIGIT_YEAR", CREDIT_CARD_EXP_2_DIGIT_YEAR},
         {"CREDIT_CARD_EXP_4_DIGIT_YEAR", CREDIT_CARD_EXP_4_DIGIT_YEAR},
         {"CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR",
          CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
         {"CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR",
          CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
         {"CREDIT_CARD_TYPE", CREDIT_CARD_TYPE},
         {"CREDIT_CARD_VERIFICATION_CODE", CREDIT_CARD_VERIFICATION_CODE},
         {"COMPANY_NAME", COMPANY_NAME},
         {"FIELD_WITH_DEFAULT_VALUE", FIELD_WITH_DEFAULT_VALUE},
         {"MERCHANT_EMAIL_SIGNUP", MERCHANT_EMAIL_SIGNUP},
         {"MERCHANT_PROMO_CODE", MERCHANT_PROMO_CODE},
         {"PASSWORD", PASSWORD},
         {"ACCOUNT_CREATION_PASSWORD", ACCOUNT_CREATION_PASSWORD},
         {"ADDRESS_HOME_STREET_ADDRESS", ADDRESS_HOME_STREET_ADDRESS},
         {"ADDRESS_HOME_SORTING_CODE", ADDRESS_HOME_SORTING_CODE},
         {"ADDRESS_HOME_DEPENDENT_LOCALITY", ADDRESS_HOME_DEPENDENT_LOCALITY},
         {"ADDRESS_HOME_LINE3", ADDRESS_HOME_LINE3},
         {"NOT_ACCOUNT_CREATION_PASSWORD", NOT_ACCOUNT_CREATION_PASSWORD},
         {"USERNAME", USERNAME},
         {"USERNAME_AND_EMAIL_ADDRESS", USERNAME_AND_EMAIL_ADDRESS},
         {"NEW_PASSWORD", NEW_PASSWORD},
         {"PROBABLY_NEW_PASSWORD", PROBABLY_NEW_PASSWORD},
         {"NOT_NEW_PASSWORD", NOT_NEW_PASSWORD},
         {"CREDIT_CARD_NAME_FIRST", CREDIT_CARD_NAME_FIRST},
         {"CREDIT_CARD_NAME_LAST", CREDIT_CARD_NAME_LAST},
         {"PHONE_HOME_EXTENSION", PHONE_HOME_EXTENSION},
         {"CONFIRMATION_PASSWORD", CONFIRMATION_PASSWORD},
         {"AMBIGUOUS_TYPE", AMBIGUOUS_TYPE},
         {"SEARCH_TERM", SEARCH_TERM},
         {"PRICE", PRICE},
         {"NOT_PASSWORD", NOT_PASSWORD},
         {"SINGLE_USERNAME", SINGLE_USERNAME},
         {"NOT_USERNAME", NOT_USERNAME},
         {"UPI_VPA", UPI_VPA},
         {"ADDRESS_HOME_STREET_NAME", ADDRESS_HOME_STREET_NAME},
         {"ADDRESS_HOME_HOUSE_NUMBER", ADDRESS_HOME_HOUSE_NUMBER},
         {"ADDRESS_HOME_SUBPREMISE", ADDRESS_HOME_SUBPREMISE},
         {"ADDRESS_HOME_OTHER_SUBUNIT", ADDRESS_HOME_OTHER_SUBUNIT},
         {"NAME_LAST_FIRST", NAME_LAST_FIRST},
         {"NAME_LAST_CONJUNCTION", NAME_LAST_CONJUNCTION},
         {"NAME_LAST_SECOND", NAME_LAST_SECOND},
         {"NAME_HONORIFIC_PREFIX", NAME_HONORIFIC_PREFIX},
         {"ADDRESS_HOME_PREMISE_NAME", ADDRESS_HOME_PREMISE_NAME},
         {"ADDRESS_HOME_DEPENDENT_STREET_NAME",
          ADDRESS_HOME_DEPENDENT_STREET_NAME},
         {"ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME",
          ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME},
         {"ADDRESS_HOME_ADDRESS", ADDRESS_HOME_ADDRESS},
         {"ADDRESS_HOME_ADDRESS_WITH_NAME", ADDRESS_HOME_ADDRESS_WITH_NAME},
         {"ADDRESS_HOME_FLOOR", ADDRESS_HOME_FLOOR},
         {"NAME_FULL_WITH_HONORIFIC_PREFIX", NAME_FULL_WITH_HONORIFIC_PREFIX},
         {"BIRTHDATE_DAY", BIRTHDATE_DAY},
         {"BIRTHDATE_MONTH", BIRTHDATE_MONTH},
         {"BIRTHDATE_4_DIGIT_YEAR", BIRTHDATE_4_DIGIT_YEAR},
         {"PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX",
          PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX},
         {"PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX",
          PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX},
         {"PHONE_HOME_NUMBER_PREFIX", PHONE_HOME_NUMBER_PREFIX},
         {"PHONE_HOME_NUMBER_SUFFIX", PHONE_HOME_NUMBER_SUFFIX},
         {"IBAN_VALUE", IBAN_VALUE},
         {"CREDIT_CARD_STANDALONE_VERIFICATION_CODE",
          CREDIT_CARD_STANDALONE_VERIFICATION_CODE},
         {"NUMERIC_QUANTITY", NUMERIC_QUANTITY},
         {"ONE_TIME_CODE", ONE_TIME_CODE},
         {"ADDRESS_HOME_LANDMARK", ADDRESS_HOME_LANDMARK},
         {"ADDRESS_HOME_BETWEEN_STREETS", ADDRESS_HOME_BETWEEN_STREETS},
         {"ADDRESS_HOME_ADMIN_LEVEL2", ADDRESS_HOME_ADMIN_LEVEL2},
         {"DELIVERY_INSTRUCTIONS", DELIVERY_INSTRUCTIONS},
         {"ADDRESS_HOME_OVERFLOW", ADDRESS_HOME_OVERFLOW},
         {"ADDRESS_HOME_STREET_LOCATION", ADDRESS_HOME_STREET_LOCATION},
         {"ADDRESS_HOME_BETWEEN_STREETS_1", ADDRESS_HOME_BETWEEN_STREETS_1},
         {"ADDRESS_HOME_BETWEEN_STREETS_2", ADDRESS_HOME_BETWEEN_STREETS_2},
         {"ADDRESS_HOME_OVERFLOW_AND_LANDMARK",
          ADDRESS_HOME_OVERFLOW_AND_LANDMARK},
         {"ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK",
          ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK},
         {"SINGLE_USERNAME_FORGOT_PASSWORD", SINGLE_USERNAME_FORGOT_PASSWORD}});

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
           t != 127 && !(130 <= t && t <= 132) && t != 134 &&
           !(137 <= t && t <= 139) && !(145 <= t && t <= 150) && t != 153 &&
           t != 155;
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
    case ADDRESS_HOME_LANDMARK:
    case ADDRESS_HOME_BETWEEN_STREETS:
    case ADDRESS_HOME_BETWEEN_STREETS_1:
    case ADDRESS_HOME_BETWEEN_STREETS_2:
    case ADDRESS_HOME_ADMIN_LEVEL2:
    case ADDRESS_HOME_OVERFLOW:
    case ADDRESS_HOME_STREET_LOCATION:
    case ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
    case ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
    case DELIVERY_INSTRUCTIONS:
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
    case SINGLE_USERNAME_FORGOT_PASSWORD:
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
  static const base::NoDestructor<
      base::flat_map<ServerFieldType, base::StringPiece>>
      kFieldTypeToTypeName(
          base::MakeFlatMap<ServerFieldType, base::StringPiece>(
              kTypeNameToFieldType, {}, [](const auto& item) {
                return std::make_pair(item.second, item.first);
              }));

  auto it = kFieldTypeToTypeName->find(type);
  if (it != kFieldTypeToTypeName->end()) {
    return it->second;
  }
  NOTREACHED_NORETURN();
}

ServerFieldType TypeNameToFieldType(base::StringPiece type_name) {
  auto* it = kTypeNameToFieldType.find(type_name);
  return it != kTypeNameToFieldType.end() ? it->second : UNKNOWN_TYPE;
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

base::StringPiece FieldTypeToDeveloperRepresentationString(
    ServerFieldType type) {
  switch (type) {
    case NO_SERVER_DATA:
    case UNKNOWN_TYPE:
    case FIELD_WITH_DEFAULT_VALUE:
    case EMPTY_TYPE:
    case NOT_ACCOUNT_CREATION_PASSWORD:
    case NOT_NEW_PASSWORD:
    case NOT_PASSWORD:
    case NOT_USERNAME:
    case AMBIGUOUS_TYPE:
    case NAME_SUFFIX:
    case ADDRESS_HOME_ADDRESS:
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
      return "";
    case NUMERIC_QUANTITY:
      return "Numeric quantity";
    case MERCHANT_EMAIL_SIGNUP:
      return "Merchant email signup";
    case MERCHANT_PROMO_CODE:
      return "Merchant promo code";
    case PASSWORD:
      return "Password";
    case ACCOUNT_CREATION_PASSWORD:
      return "Account creation password";
    case USERNAME:
    case SINGLE_USERNAME:
    case SINGLE_USERNAME_FORGOT_PASSWORD:
      return "Username";
    case USERNAME_AND_EMAIL_ADDRESS:
      return "Username and email";
    case PROBABLY_NEW_PASSWORD:
    case NEW_PASSWORD:
      return "New password";
    case CONFIRMATION_PASSWORD:
      return "Confirmation password";
    case SEARCH_TERM:
      return "Search term";
    case PRICE:
      return "Price";
    case NAME_HONORIFIC_PREFIX:
      return "Honorific prefix";
    case NAME_FULL_WITH_HONORIFIC_PREFIX:
      return "Full name with honorific prefix";
    case NAME_FIRST:
      return "First name";
    case NAME_MIDDLE:
      return "Middle name";
    case NAME_LAST:
      return "Last name";
    case NAME_LAST_FIRST:
      return "First last name";
    case NAME_LAST_CONJUNCTION:
      return "Last name conjunction";
    case NAME_LAST_SECOND:
      return "Second last name";
    case NAME_MIDDLE_INITIAL:
      return "Middle name initial";
    case NAME_FULL:
      return "Full name";
    case EMAIL_ADDRESS:
      return "Email address";
    case PHONE_HOME_NUMBER:
    case PHONE_HOME_WHOLE_NUMBER:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
      return "Phone number";
    case PHONE_HOME_NUMBER_PREFIX:
      return "Phone number prefix";
    case PHONE_HOME_NUMBER_SUFFIX:
      return "Phone number suffix";
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
      return "Phone number city code";
    case PHONE_HOME_COUNTRY_CODE:
      return "Phone number country code";
    case PHONE_HOME_EXTENSION:
      return "Phone number extension";
    case ADDRESS_HOME_FLOOR:
      return "Floor";
    case ADDRESS_HOME_LANDMARK:
      return "Landmark";
    case ADDRESS_HOME_STREET_NAME:
      return "Street name";
    case ADDRESS_HOME_DEPENDENT_STREET_NAME:
      return "Dependent home street name";
    case ADDRESS_HOME_HOUSE_NUMBER:
      return "House number";
    case ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME:
      return "Street and dependent street name";
    case ADDRESS_HOME_BETWEEN_STREETS:
      return "Address between-streets";
    case ADDRESS_HOME_BETWEEN_STREETS_1:
      return "Address between-streets 1";
    case ADDRESS_HOME_BETWEEN_STREETS_2:
      return "Address between-streets 2";
    case ADDRESS_HOME_LINE1:
      return "Address line 1";
    case ADDRESS_HOME_LINE2:
      return "Address line 2";
    case ADDRESS_HOME_LINE3:
      return "Address line 3";
    case ADDRESS_HOME_PREMISE_NAME:
      return "Address premise";
    case ADDRESS_HOME_SUBPREMISE:
      return "Address subpremise";
    case ADDRESS_HOME_OTHER_SUBUNIT:
      return "Address subunit";
    case ADDRESS_HOME_ADMIN_LEVEL2:
      return "Administrative area level 2";
    case ADDRESS_HOME_STREET_LOCATION:
      return "Street location";
    case ADDRESS_HOME_STREET_ADDRESS:
      return "Street address";
    case ADDRESS_HOME_SORTING_CODE:
      return "Sorting code";
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      return "Dependent locality";
    case ADDRESS_HOME_APT_NUM:
      return "Apt num";
    case ADDRESS_HOME_CITY:
      return "City";
    case ADDRESS_HOME_STATE:
      return "State";
    case ADDRESS_HOME_ZIP:
      return "Zip code";
    case ADDRESS_HOME_COUNTRY:
      return "Country";
    case ADDRESS_HOME_OVERFLOW:
      return "Address overflow";
    case ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
      return "Address overflow and landmark";
    case ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
      return "Address between-streets and landmark";
    case DELIVERY_INSTRUCTIONS:
      return "Delivery instructions";
    case BIRTHDATE_DAY:
      return "Birthdate day";
    case BIRTHDATE_MONTH:
      return "Birthdate month";
    case BIRTHDATE_4_DIGIT_YEAR:
      return "Birthdate year";
    case CREDIT_CARD_NAME_FULL:
      return "Credit card full name";
    case CREDIT_CARD_NAME_FIRST:
      return "Credit card first name";
    case CREDIT_CARD_NAME_LAST:
      return "Credit card last name";
    case CREDIT_CARD_NUMBER:
      return "Credit card number";
    case CREDIT_CARD_EXP_MONTH:
      return "Credit card exp month";
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return "Credit card exp year";
    case CREDIT_CARD_TYPE:
      return "Credit card type";
    case CREDIT_CARD_VERIFICATION_CODE:
      return "Credit card verification code";
    case COMPANY_NAME:
      return "Company name";
    case UPI_VPA:
      return "UPI VPA";
    case IBAN_VALUE:
      return "IBAN";
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
    case ONE_TIME_CODE:
      return "One time code";
    case MAX_VALID_FIELD_TYPE:
      return "";
  }
  NOTREACHED_NORETURN();
}

}  // namespace autofill
