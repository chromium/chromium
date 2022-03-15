// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_type.h"

#include "base/notreached.h"
#include "base/strings/string_piece.h"

namespace autofill {

FieldTypeGroup GroupTypeOfServerFieldType(ServerFieldType field_type) {
  switch (field_type) {
    case NAME_HONORIFIC_PREFIX:
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
    case NAME_LAST_FIRST:
    case NAME_LAST_SECOND:
    case NAME_LAST_CONJUNCTION:
    case NAME_MIDDLE_INITIAL:
    case NAME_FULL:
    case NAME_SUFFIX:
    case NAME_FULL_WITH_HONORIFIC_PREFIX:
      return FieldTypeGroup::kName;

    case NAME_BILLING_FIRST:
    case NAME_BILLING_MIDDLE:
    case NAME_BILLING_LAST:
    case NAME_BILLING_MIDDLE_INITIAL:
    case NAME_BILLING_FULL:
    case NAME_BILLING_SUFFIX:
      return FieldTypeGroup::kNameBilling;

    case EMAIL_ADDRESS:
    case USERNAME_AND_EMAIL_ADDRESS:
      return FieldTypeGroup::kEmail;

    case PHONE_HOME_NUMBER:
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_COUNTRY_CODE:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_WHOLE_NUMBER:
    case PHONE_HOME_EXTENSION:
      return FieldTypeGroup::kPhoneHome;

    case PHONE_BILLING_NUMBER:
    case PHONE_BILLING_CITY_CODE:
    case PHONE_BILLING_COUNTRY_CODE:
    case PHONE_BILLING_CITY_AND_NUMBER:
    case PHONE_BILLING_WHOLE_NUMBER:
      return FieldTypeGroup::kPhoneBilling;

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
    case ADDRESS_HOME_HOUSE_NUMBER:
    case ADDRESS_HOME_PREMISE_NAME:
    case ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME:
    case ADDRESS_HOME_SUBPREMISE:
    case ADDRESS_HOME_OTHER_SUBUNIT:
    case ADDRESS_HOME_ADDRESS:
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
    case ADDRESS_HOME_FLOOR:
      return FieldTypeGroup::kAddressHome;

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
      return FieldTypeGroup::kAddressBilling;

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
      return FieldTypeGroup::kCreditCard;

    case COMPANY_NAME:
      return FieldTypeGroup::kCompany;

    case MERCHANT_PROMO_CODE:
      // TODO(crbug/1190334): Create new field type group kMerchantPromoCode.
      //                      (This involves updating many switch statements.)
      return FieldTypeGroup::kNoGroup;

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
      return FieldTypeGroup::kPasswordField;

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
    case UPI_VPA:
      return FieldTypeGroup::kNoGroup;

    case MAX_VALID_FIELD_TYPE:
      NOTREACHED();
      return FieldTypeGroup::kNoGroup;

    case USERNAME:
      return FieldTypeGroup::kUsernameField;

    case BIRTHDATE_DAY:
    case BIRTHDATE_MONTH:
    case BIRTHDATE_YEAR_4_DIGITS:
      return FieldTypeGroup::kBirthdateField;

    case PRICE:
    case SEARCH_TERM:
      return FieldTypeGroup::kUnfillable;

    case UNKNOWN_TYPE:
      return FieldTypeGroup::kNoGroup;
  }
  NOTREACHED();
  return FieldTypeGroup::kNoGroup;
}

FieldTypeGroup GroupTypeOfHtmlFieldType(HtmlFieldType field_type,
                                        HtmlFieldMode field_mode) {
  switch (field_type) {
    case HTML_TYPE_NAME:
    case HTML_TYPE_HONORIFIC_PREFIX:
    case HTML_TYPE_GIVEN_NAME:
    case HTML_TYPE_ADDITIONAL_NAME:
    case HTML_TYPE_ADDITIONAL_NAME_INITIAL:
    case HTML_TYPE_FAMILY_NAME:
      return field_mode == HTML_MODE_BILLING ? FieldTypeGroup::kNameBilling
                                             : FieldTypeGroup::kName;

    case HTML_TYPE_ORGANIZATION:
      return FieldTypeGroup::kCompany;

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
      return field_mode == HTML_MODE_BILLING ? FieldTypeGroup::kAddressBilling
                                             : FieldTypeGroup::kAddressHome;

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
      return FieldTypeGroup::kCreditCard;

    case HTML_TYPE_TRANSACTION_AMOUNT:
    case HTML_TYPE_TRANSACTION_CURRENCY:
      return FieldTypeGroup::kTransaction;

    case HTML_TYPE_TEL:
    case HTML_TYPE_TEL_COUNTRY_CODE:
    case HTML_TYPE_TEL_NATIONAL:
    case HTML_TYPE_TEL_AREA_CODE:
    case HTML_TYPE_TEL_LOCAL:
    case HTML_TYPE_TEL_LOCAL_PREFIX:
    case HTML_TYPE_TEL_LOCAL_SUFFIX:
    case HTML_TYPE_TEL_EXTENSION:
      return field_mode == HTML_MODE_BILLING ? FieldTypeGroup::kPhoneBilling
                                             : FieldTypeGroup::kPhoneHome;

    case HTML_TYPE_EMAIL:
      return FieldTypeGroup::kEmail;

    case HTML_TYPE_UPI_VPA:
      // TODO(crbug/702223): Add support for UPI-VPA.
      return FieldTypeGroup::kNoGroup;

    case HTML_TYPE_ONE_TIME_CODE:
      return FieldTypeGroup::kNoGroup;

    case HTML_TYPE_MERCHANT_PROMO_CODE:
      return FieldTypeGroup::kNoGroup;

    case HTML_TYPE_UNSPECIFIED:
    case HTML_TYPE_UNRECOGNIZED:
      return FieldTypeGroup::kNoGroup;
  }
  NOTREACHED();
  return FieldTypeGroup::kNoGroup;
}

AutofillType::AutofillType(ServerFieldType field_type)
    : server_type_(ToSafeServerFieldType(field_type, UNKNOWN_TYPE)),
      html_type_(HTML_TYPE_UNSPECIFIED),
      html_mode_(HTML_MODE_NONE) {}

AutofillType::AutofillType(HtmlFieldType field_type, HtmlFieldMode mode)
    : server_type_(UNKNOWN_TYPE), html_type_(field_type), html_mode_(mode) {}

FieldTypeGroup AutofillType::group() const {
  FieldTypeGroup result = FieldTypeGroup::kNoGroup;
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

    case HTML_TYPE_HONORIFIC_PREFIX:
      return NAME_HONORIFIC_PREFIX;

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
    case HTML_TYPE_ONE_TIME_CODE:
    case HTML_TYPE_MERCHANT_PROMO_CODE:
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

  return std::string(FieldTypeToStringPiece(html_type_));
}

// static
std::string AutofillType::ServerFieldTypeToString(ServerFieldType type) {
  return std::string(FieldTypeToStringPiece(type));
}

}  // namespace autofill
