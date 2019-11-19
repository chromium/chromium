// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_type.h"

#include "base/logging.h"

namespace autofill {

FieldTypeGroup GroupTypeOfServerFieldType(ServerFieldType field_type) {
  switch (field_type) {
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
    case NAME_MIDDLE_INITIAL:
    case NAME_FULL:
    case NAME_SUFFIX:
      return NAME;

    case NAME_BILLING_FIRST:
    case NAME_BILLING_MIDDLE:
    case NAME_BILLING_LAST:
    case NAME_BILLING_MIDDLE_INITIAL:
    case NAME_BILLING_FULL:
    case NAME_BILLING_SUFFIX:
      return NAME_BILLING;

    case EMAIL_ADDRESS:
    case USERNAME_AND_EMAIL_ADDRESS:
      return EMAIL;

    case PHONE_HOME_NUMBER:
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_COUNTRY_CODE:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_WHOLE_NUMBER:
    case PHONE_HOME_EXTENSION:
      return PHONE_HOME;

    case PHONE_BILLING_NUMBER:
    case PHONE_BILLING_CITY_CODE:
    case PHONE_BILLING_COUNTRY_CODE:
    case PHONE_BILLING_CITY_AND_NUMBER:
    case PHONE_BILLING_WHOLE_NUMBER:
      return PHONE_BILLING;

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
    case ADDRESS_HOME_STREET:
    case ADDRESS_HOME_HOUSE_NUMBER:
    case ADDRESS_HOME_FLOOR:
    case ADDRESS_HOME_OTHER_SUBUNIT:
      return ADDRESS_HOME;

    case ADDRESS_BILLING_LINE1:
    case ADDRESS_BILLING_LINE2:
    case ADDRESS_BILLING_LINE3:
    case ADDRESS_BILLING_APT_NUM:
    case ADDRESS_BILLING_CITY:
    case ADDRESS_BILLING_STATE:
    case ADDRESS_BILLING_ZIP:
    case ADDRESS_BILLING_COUNTRY:
    case ADDRESS_BILLING_STREET_ADDRESS:
    case ADDRESS_BILLING_SORTING_CODE:
    case ADDRESS_BILLING_DEPENDENT_LOCALITY:
      return ADDRESS_BILLING;

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
      return CREDIT_CARD;

    case COMPANY_NAME:
      return COMPANY;

    case PASSWORD:
    case ACCOUNT_CREATION_PASSWORD:
    case NOT_ACCOUNT_CREATION_PASSWORD:
    case NEW_PASSWORD:
    case PROBABLY_NEW_PASSWORD:
    case NOT_NEW_PASSWORD:
    case CONFIRMATION_PASSWORD:
    case NOT_PASSWORD:
    case SINGLE_USERNAME:
    case NOT_USERNAME:
      return PASSWORD_FIELD;

    case NO_SERVER_DATA:
    case EMPTY_TYPE:
    case AMBIGUOUS_TYPE:
    case PHONE_FAX_NUMBER:
    case PHONE_FAX_CITY_CODE:
    case PHONE_FAX_COUNTRY_CODE:
    case PHONE_FAX_CITY_AND_NUMBER:
    case PHONE_FAX_WHOLE_NUMBER:
    case FIELD_WITH_DEFAULT_VALUE:
    case MERCHANT_EMAIL_SIGNUP:
    case MERCHANT_PROMO_CODE:
    case UPI_VPA:
      return NO_GROUP;

    case MAX_VALID_FIELD_TYPE:
      NOTREACHED();
      return NO_GROUP;

    case USERNAME:
      return USERNAME_FIELD;

    case PRICE:
    case SEARCH_TERM:
      return UNFILLABLE;

    case UNKNOWN_TYPE:
      return NO_GROUP;
  }
  NOTREACHED();
  return NO_GROUP;
}

FieldTypeGroup GroupTypeOfHtmlFieldType(HtmlFieldType field_type,
                                        HtmlFieldMode field_mode) {
  switch (field_type) {
    case HTML_TYPE_NAME:
    case HTML_TYPE_GIVEN_NAME:
    case HTML_TYPE_ADDITIONAL_NAME:
    case HTML_TYPE_ADDITIONAL_NAME_INITIAL:
    case HTML_TYPE_FAMILY_NAME:
      return field_mode == HTML_MODE_BILLING ? NAME_BILLING : NAME;

    case HTML_TYPE_ORGANIZATION:
      return COMPANY;

    case HTML_TYPE_STREET_ADDRESS:
    case HTML_TYPE_ADDRESS_LINE1:
    case HTML_TYPE_ADDRESS_LINE2:
    case HTML_TYPE_ADDRESS_LINE3:
    case HTML_TYPE_ADDRESS_LEVEL1:
    case HTML_TYPE_ADDRESS_LEVEL2:
    case HTML_TYPE_ADDRESS_LEVEL3:
    case HTML_TYPE_COUNTRY_CODE:
    case HTML_TYPE_COUNTRY_NAME:
    case HTML_TYPE_POSTAL_CODE:
    case HTML_TYPE_FULL_ADDRESS:
      return field_mode == HTML_MODE_BILLING ? ADDRESS_BILLING : ADDRESS_HOME;

    case HTML_TYPE_CREDIT_CARD_NAME_FULL:
    case HTML_TYPE_CREDIT_CARD_NAME_FIRST:
    case HTML_TYPE_CREDIT_CARD_NAME_LAST:
    case HTML_TYPE_CREDIT_CARD_NUMBER:
    case HTML_TYPE_CREDIT_CARD_EXP:
    case HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case HTML_TYPE_CREDIT_CARD_EXP_MONTH:
    case HTML_TYPE_CREDIT_CARD_EXP_YEAR:
    case HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE:
    case HTML_TYPE_CREDIT_CARD_TYPE:
      return CREDIT_CARD;

    case HTML_TYPE_TRANSACTION_AMOUNT:
    case HTML_TYPE_TRANSACTION_CURRENCY:
      return TRANSACTION;

    case HTML_TYPE_TEL:
    case HTML_TYPE_TEL_COUNTRY_CODE:
    case HTML_TYPE_TEL_NATIONAL:
    case HTML_TYPE_TEL_AREA_CODE:
    case HTML_TYPE_TEL_LOCAL:
    case HTML_TYPE_TEL_LOCAL_PREFIX:
    case HTML_TYPE_TEL_LOCAL_SUFFIX:
    case HTML_TYPE_TEL_EXTENSION:
      return field_mode == HTML_MODE_BILLING ? PHONE_BILLING : PHONE_HOME;

    case HTML_TYPE_EMAIL:
      return EMAIL;

    case HTML_TYPE_UPI_VPA:
      // TODO(crbug/702223): Add support for UPI-VPA.
      return NO_GROUP;

    case HTML_TYPE_UNSPECIFIED:
    case HTML_TYPE_UNRECOGNIZED:
      return NO_GROUP;
  }
  NOTREACHED();
  return NO_GROUP;
}

AutofillType::AutofillType(ServerFieldType field_type)
    : html_type_(HTML_TYPE_UNSPECIFIED), html_mode_(HTML_MODE_NONE) {
  if ((field_type < NO_SERVER_DATA || field_type >= MAX_VALID_FIELD_TYPE) ||
      (field_type >= 15 && field_type <= 19) ||
      (field_type >= 25 && field_type <= 29) ||
      (field_type >= 44 && field_type <= 50) || field_type == 94) {
    server_type_ = UNKNOWN_TYPE;
  } else {
    server_type_ = field_type;
  }
}

AutofillType::AutofillType(HtmlFieldType field_type, HtmlFieldMode mode)
    : server_type_(UNKNOWN_TYPE), html_type_(field_type), html_mode_(mode) {}

FieldTypeGroup AutofillType::group() const {
  FieldTypeGroup result = NO_GROUP;
  if (server_type_ != UNKNOWN_TYPE) {
    result = GroupTypeOfServerFieldType(server_type_);
  } else {
    result = GroupTypeOfHtmlFieldType(html_type_, html_mode_);
  }
  return result;
}

bool AutofillType::IsUnknown() const {
  return server_type_ == UNKNOWN_TYPE && (html_type_ == HTML_TYPE_UNSPECIFIED ||
                                          html_type_ == HTML_TYPE_UNRECOGNIZED);
}

ServerFieldType AutofillType::GetStorableType() const {
  // Map billing types to the equivalent non-billing types.
  switch (server_type_) {
    case ADDRESS_BILLING_LINE1:
      return ADDRESS_HOME_LINE1;

    case ADDRESS_BILLING_LINE2:
      return ADDRESS_HOME_LINE2;

    case ADDRESS_BILLING_LINE3:
      return ADDRESS_HOME_LINE3;

    case ADDRESS_BILLING_APT_NUM:
      return ADDRESS_HOME_APT_NUM;

    case ADDRESS_BILLING_CITY:
      return ADDRESS_HOME_CITY;

    case ADDRESS_BILLING_STATE:
      return ADDRESS_HOME_STATE;

    case ADDRESS_BILLING_ZIP:
      return ADDRESS_HOME_ZIP;

    case ADDRESS_BILLING_COUNTRY:
      return ADDRESS_HOME_COUNTRY;

    case PHONE_BILLING_WHOLE_NUMBER:
      return PHONE_HOME_WHOLE_NUMBER;

    case PHONE_BILLING_NUMBER:
      return PHONE_HOME_NUMBER;

    case PHONE_BILLING_CITY_CODE:
      return PHONE_HOME_CITY_CODE;

    case PHONE_BILLING_COUNTRY_CODE:
      return PHONE_HOME_COUNTRY_CODE;

    case PHONE_BILLING_CITY_AND_NUMBER:
      return PHONE_HOME_CITY_AND_NUMBER;

    case NAME_BILLING_FIRST:
      return NAME_FIRST;

    case NAME_BILLING_MIDDLE:
      return NAME_MIDDLE;

    case NAME_BILLING_LAST:
      return NAME_LAST;

    case NAME_BILLING_MIDDLE_INITIAL:
      return NAME_MIDDLE_INITIAL;

    case NAME_BILLING_FULL:
      return NAME_FULL;

    case NAME_BILLING_SUFFIX:
      return NAME_SUFFIX;

    case ADDRESS_BILLING_STREET_ADDRESS:
      return ADDRESS_HOME_STREET_ADDRESS;

    case ADDRESS_BILLING_SORTING_CODE:
      return ADDRESS_HOME_SORTING_CODE;

    case ADDRESS_BILLING_DEPENDENT_LOCALITY:
      return ADDRESS_HOME_DEPENDENT_LOCALITY;

    case UNKNOWN_TYPE:
      break;  // Try to parse HTML types instead.

    default:
      return server_type_;
  }

  switch (html_type_) {
    case HTML_TYPE_UNSPECIFIED:
      return UNKNOWN_TYPE;

    case HTML_TYPE_NAME:
      return NAME_FULL;

    case HTML_TYPE_GIVEN_NAME:
      return NAME_FIRST;

    case HTML_TYPE_ADDITIONAL_NAME:
      return NAME_MIDDLE;

    case HTML_TYPE_FAMILY_NAME:
      return NAME_LAST;

    case HTML_TYPE_ORGANIZATION:
      return COMPANY_NAME;

    case HTML_TYPE_STREET_ADDRESS:
      return ADDRESS_HOME_STREET_ADDRESS;

    case HTML_TYPE_ADDRESS_LINE1:
      return ADDRESS_HOME_LINE1;

    case HTML_TYPE_ADDRESS_LINE2:
      return ADDRESS_HOME_LINE2;

    case HTML_TYPE_ADDRESS_LINE3:
      return ADDRESS_HOME_LINE3;

    case HTML_TYPE_ADDRESS_LEVEL1:
      return ADDRESS_HOME_STATE;

    case HTML_TYPE_ADDRESS_LEVEL2:
      return ADDRESS_HOME_CITY;

    case HTML_TYPE_ADDRESS_LEVEL3:
      return ADDRESS_HOME_DEPENDENT_LOCALITY;

    case HTML_TYPE_COUNTRY_CODE:
    case HTML_TYPE_COUNTRY_NAME:
      return ADDRESS_HOME_COUNTRY;

    case HTML_TYPE_POSTAL_CODE:
      return ADDRESS_HOME_ZIP;

    // Full address is composed of other types; it can't be stored.
    case HTML_TYPE_FULL_ADDRESS:
      return UNKNOWN_TYPE;

    case HTML_TYPE_CREDIT_CARD_NAME_FULL:
      return CREDIT_CARD_NAME_FULL;

    case HTML_TYPE_CREDIT_CARD_NAME_FIRST:
      return CREDIT_CARD_NAME_FIRST;

    case HTML_TYPE_CREDIT_CARD_NAME_LAST:
      return CREDIT_CARD_NAME_LAST;

    case HTML_TYPE_CREDIT_CARD_NUMBER:
      return CREDIT_CARD_NUMBER;

    case HTML_TYPE_CREDIT_CARD_EXP:
      return CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;

    case HTML_TYPE_CREDIT_CARD_EXP_MONTH:
      return CREDIT_CARD_EXP_MONTH;

    case HTML_TYPE_CREDIT_CARD_EXP_YEAR:
      return CREDIT_CARD_EXP_4_DIGIT_YEAR;

    case HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE:
      return CREDIT_CARD_VERIFICATION_CODE;

    case HTML_TYPE_CREDIT_CARD_TYPE:
      return CREDIT_CARD_TYPE;

    case HTML_TYPE_TEL:
      return PHONE_HOME_WHOLE_NUMBER;

    case HTML_TYPE_TEL_COUNTRY_CODE:
      return PHONE_HOME_COUNTRY_CODE;

    case HTML_TYPE_TEL_NATIONAL:
      return PHONE_HOME_CITY_AND_NUMBER;

    case HTML_TYPE_TEL_AREA_CODE:
      return PHONE_HOME_CITY_CODE;

    case HTML_TYPE_TEL_LOCAL:
    case HTML_TYPE_TEL_LOCAL_PREFIX:
    case HTML_TYPE_TEL_LOCAL_SUFFIX:
      return PHONE_HOME_NUMBER;

    case HTML_TYPE_TEL_EXTENSION:
      return PHONE_HOME_EXTENSION;

    case HTML_TYPE_EMAIL:
      return EMAIL_ADDRESS;

    case HTML_TYPE_ADDITIONAL_NAME_INITIAL:
      return NAME_MIDDLE_INITIAL;

    case HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      return CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;

    case HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;

    case HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return CREDIT_CARD_EXP_2_DIGIT_YEAR;

    case HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return CREDIT_CARD_EXP_4_DIGIT_YEAR;

    case HTML_TYPE_UPI_VPA:
      return UPI_VPA;

    // These types aren't stored; they're transient.
    case HTML_TYPE_TRANSACTION_AMOUNT:
    case HTML_TYPE_TRANSACTION_CURRENCY:
      return UNKNOWN_TYPE;

    case HTML_TYPE_UNRECOGNIZED:
      return UNKNOWN_TYPE;
  }

  NOTREACHED();
  return UNKNOWN_TYPE;
}

std::string AutofillType::ToString() const {
  if (IsUnknown())
    return "UNKNOWN_TYPE";

  if (server_type_ != UNKNOWN_TYPE)
    return ServerFieldTypeToString(server_type_);

  switch (html_type_) {
    case HTML_TYPE_UNSPECIFIED:
      NOTREACHED();
      break;
    case HTML_TYPE_NAME:
      return "HTML_TYPE_NAME";
    case HTML_TYPE_GIVEN_NAME:
      return "HTML_TYPE_GIVEN_NAME";
    case HTML_TYPE_ADDITIONAL_NAME:
      return "HTML_TYPE_ADDITIONAL_NAME";
    case HTML_TYPE_FAMILY_NAME:
      return "HTML_TYPE_FAMILY_NAME";
    case HTML_TYPE_ORGANIZATION:
      return "HTML_TYPE_ORGANIZATION";
    case HTML_TYPE_STREET_ADDRESS:
      return "HTML_TYPE_STREET_ADDRESS";
    case HTML_TYPE_ADDRESS_LINE1:
      return "HTML_TYPE_ADDRESS_LINE1";
    case HTML_TYPE_ADDRESS_LINE2:
      return "HTML_TYPE_ADDRESS_LINE2";
    case HTML_TYPE_ADDRESS_LINE3:
      return "HTML_TYPE_ADDRESS_LINE3";
    case HTML_TYPE_ADDRESS_LEVEL1:
      return "HTML_TYPE_ADDRESS_LEVEL1";
    case HTML_TYPE_ADDRESS_LEVEL2:
      return "HTML_TYPE_ADDRESS_LEVEL2";
    case HTML_TYPE_ADDRESS_LEVEL3:
      return "HTML_TYPE_ADDRESS_LEVEL3";
    case HTML_TYPE_COUNTRY_CODE:
      return "HTML_TYPE_COUNTRY_CODE";
    case HTML_TYPE_COUNTRY_NAME:
      return "HTML_TYPE_COUNTRY_NAME";
    case HTML_TYPE_POSTAL_CODE:
      return "HTML_TYPE_POSTAL_CODE";
    case HTML_TYPE_FULL_ADDRESS:
      return "HTML_TYPE_FULL_ADDRESS";
    case HTML_TYPE_CREDIT_CARD_NAME_FULL:
      return "HTML_TYPE_CREDIT_CARD_NAME_FULL";
    case HTML_TYPE_CREDIT_CARD_NAME_FIRST:
      return "HTML_TYPE_CREDIT_CARD_NAME_FIRST";
    case HTML_TYPE_CREDIT_CARD_NAME_LAST:
      return "HTML_TYPE_CREDIT_CARD_NAME_LAST";
    case HTML_TYPE_CREDIT_CARD_NUMBER:
      return "HTML_TYPE_CREDIT_CARD_NUMBER";
    case HTML_TYPE_CREDIT_CARD_EXP:
      return "HTML_TYPE_CREDIT_CARD_EXP";
    case HTML_TYPE_CREDIT_CARD_EXP_MONTH:
      return "HTML_TYPE_CREDIT_CARD_EXP_MONTH";
    case HTML_TYPE_CREDIT_CARD_EXP_YEAR:
      return "HTML_TYPE_CREDIT_CARD_EXP_YEAR";
    case HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE:
      return "HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE";
    case HTML_TYPE_CREDIT_CARD_TYPE:
      return "HTML_TYPE_CREDIT_CARD_TYPE";
    case HTML_TYPE_TEL:
      return "HTML_TYPE_TEL";
    case HTML_TYPE_TEL_COUNTRY_CODE:
      return "HTML_TYPE_TEL_COUNTRY_CODE";
    case HTML_TYPE_TEL_NATIONAL:
      return "HTML_TYPE_TEL_NATIONAL";
    case HTML_TYPE_TEL_AREA_CODE:
      return "HTML_TYPE_TEL_AREA_CODE";
    case HTML_TYPE_TEL_LOCAL:
      return "HTML_TYPE_TEL_LOCAL";
    case HTML_TYPE_TEL_LOCAL_PREFIX:
      return "HTML_TYPE_TEL_LOCAL_PREFIX";
    case HTML_TYPE_TEL_LOCAL_SUFFIX:
      return "HTML_TYPE_TEL_LOCAL_SUFFIX";
    case HTML_TYPE_TEL_EXTENSION:
      return "HTML_TYPE_TEL_EXTENSION";
    case HTML_TYPE_EMAIL:
      return "HTML_TYPE_EMAIL";
    case HTML_TYPE_ADDITIONAL_NAME_INITIAL:
      return "HTML_TYPE_ADDITIONAL_NAME_INITIAL";
    case HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      return "HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR";
    case HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return "HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR";
    case HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return "HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR";
    case HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return "HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR";
    case HTML_TYPE_TRANSACTION_AMOUNT:
      return "HTML_TRANSACTION_AMOUNT";
    case HTML_TYPE_TRANSACTION_CURRENCY:
      return "HTML_TRANSACTION_CURRENCY";
    case HTML_TYPE_UPI_VPA:
      return "HTML_TYPE_UPI_VPA";
    case HTML_TYPE_UNRECOGNIZED:
      return "HTML_TYPE_UNRECOGNIZED";
  }

  NOTREACHED();
  return std::string();
}

// static
std::string AutofillType::ServerFieldTypeToString(ServerFieldType type) {
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
    case NAME_FIRST:
      return "NAME_FIRST";
    case NAME_MIDDLE:
      return "NAME_MIDDLE";
    case NAME_LAST:
      return "NAME_LAST";
    case NAME_MIDDLE_INITIAL:
      return "NAME_MIDDLE_INITIAL";
    case NAME_FULL:
      return "NAME_FULL";
    case NAME_SUFFIX:
      return "NAME_SUFFIX";
    case NAME_BILLING_FIRST:
      return "NAME_BILLING_FIRST";
    case NAME_BILLING_MIDDLE:
      return "NAME_BILLING_MIDDLE";
    case NAME_BILLING_LAST:
      return "NAME_BILLING_LAST";
    case NAME_BILLING_MIDDLE_INITIAL:
      return "NAME_BILLING_MIDDLE_INITIAL";
    case NAME_BILLING_FULL:
      return "NAME_BILLING_FULL";
    case NAME_BILLING_SUFFIX:
      return "NAME_BILLING_SUFFIX";
    case EMAIL_ADDRESS:
      return "EMAIL_ADDRESS";
    case PHONE_HOME_NUMBER:
      return "PHONE_HOME_NUMBER";
    case PHONE_HOME_CITY_CODE:
      return "PHONE_HOME_CITY_CODE";
    case PHONE_HOME_COUNTRY_CODE:
      return "PHONE_HOME_COUNTRY_CODE";
    case PHONE_HOME_CITY_AND_NUMBER:
      return "PHONE_HOME_CITY_AND_NUMBER";
    case PHONE_HOME_WHOLE_NUMBER:
      return "PHONE_HOME_WHOLE_NUMBER";
    case PHONE_HOME_EXTENSION:
      return "PHONE_HOME_EXTENSION";
    case PHONE_FAX_NUMBER:
      return "PHONE_FAX_NUMBER";
    case PHONE_FAX_CITY_CODE:
      return "PHONE_FAX_CITY_CODE";
    case PHONE_FAX_COUNTRY_CODE:
      return "PHONE_FAX_COUNTRY_CODE";
    case PHONE_FAX_CITY_AND_NUMBER:
      return "PHONE_FAX_CITY_AND_NUMBER";
    case PHONE_FAX_WHOLE_NUMBER:
      return "PHONE_FAX_WHOLE_NUMBER";
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
    case ADDRESS_BILLING_LINE1:
      return "ADDRESS_BILLING_LINE1";
    case ADDRESS_BILLING_LINE2:
      return "ADDRESS_BILLING_LINE2";
    case ADDRESS_BILLING_LINE3:
      return "ADDRESS_BILLING_LINE3";
    case ADDRESS_BILLING_APT_NUM:
      return "ADDRESS_BILLING_APT_NUM";
    case ADDRESS_BILLING_CITY:
      return "ADDRESS_BILLING_CITY";
    case ADDRESS_BILLING_STATE:
      return "ADDRESS_BILLING_STATE";
    case ADDRESS_BILLING_ZIP:
      return "ADDRESS_BILLING_ZIP";
    case ADDRESS_BILLING_COUNTRY:
      return "ADDRESS_BILLING_COUNTRY";
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
    case PHONE_BILLING_NUMBER:
      return "PHONE_BILLING_NUMBER";
    case PHONE_BILLING_CITY_CODE:
      return "PHONE_BILLING_CITY_CODE";
    case PHONE_BILLING_COUNTRY_CODE:
      return "PHONE_BILLING_COUNTRY_CODE";
    case PHONE_BILLING_CITY_AND_NUMBER:
      return "PHONE_BILLING_CITY_AND_NUMBER";
    case PHONE_BILLING_WHOLE_NUMBER:
      return "PHONE_BILLING_WHOLE_NUMBER";
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
    case ADDRESS_BILLING_STREET_ADDRESS:
      return "ADDRESS_BILLING_STREET_ADDRESS";
    case ADDRESS_HOME_SORTING_CODE:
      return "ADDRESS_HOME_SORTING_CODE";
    case ADDRESS_BILLING_SORTING_CODE:
      return "ADDRESS_BILLING_SORTING_CODE";
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      return "ADDRESS_HOME_DEPENDENT_LOCALITY";
    case ADDRESS_BILLING_DEPENDENT_LOCALITY:
      return "ADDRESS_BILLING_DEPENDENT_LOCALITY";
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
    case NOT_PASSWORD:
      return "NOT_PASSWORD";
    case SINGLE_USERNAME:
      return "SINGLE_USERNAME";
    case NOT_USERNAME:
      return "NOT_USERNAME";
    case UPI_VPA:
      return "UPI_VPA";
    case ADDRESS_HOME_STREET:
      return "ADDRESS_HOME_STREET";
    case ADDRESS_HOME_HOUSE_NUMBER:
      return "ADDRESS_HOME_HOUSE_NUMBER";
    case ADDRESS_HOME_FLOOR:
      return "ADDRESS_HOME_FLOOR";
    case ADDRESS_HOME_OTHER_SUBUNIT:
      return "ADDRESS_HOME_OTHER_SUBUNIT";
    case AMBIGUOUS_TYPE:
      return "AMBIGUOUS_TYPE";
    case MAX_VALID_FIELD_TYPE:
      return std::string();
  }

  NOTREACHED();
  return std::string();
}

}  // namespace autofill
