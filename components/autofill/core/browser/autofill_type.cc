// Copyright 2013 The Chromium Authors
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

    case EMAIL_ADDRESS:
    case USERNAME_AND_EMAIL_ADDRESS:
      return FieldTypeGroup::kEmail;

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
      return FieldTypeGroup::kPhoneHome;

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
    case ADDRESS_HOME_LANDMARK:
    case ADDRESS_HOME_BETWEEN_STREETS:
    case ADDRESS_HOME_ADMIN_LEVEL2:
      return FieldTypeGroup::kAddressHome;

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

    case IBAN_VALUE:
      return FieldTypeGroup::kIban;

    case COMPANY_NAME:
      return FieldTypeGroup::kCompany;

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
    case FIELD_WITH_DEFAULT_VALUE:
    case MERCHANT_EMAIL_SIGNUP:
    case MERCHANT_PROMO_CODE:
    case UPI_VPA:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      return FieldTypeGroup::kNoGroup;

    case MAX_VALID_FIELD_TYPE:
      NOTREACHED();
      return FieldTypeGroup::kNoGroup;

    case USERNAME:
      return FieldTypeGroup::kUsernameField;

    case BIRTHDATE_DAY:
    case BIRTHDATE_MONTH:
    case BIRTHDATE_4_DIGIT_YEAR:
      return FieldTypeGroup::kBirthdateField;

    case PRICE:
    case SEARCH_TERM:
    case NUMERIC_QUANTITY:
    case ONE_TIME_CODE:
      return FieldTypeGroup::kUnfillable;

    case UNKNOWN_TYPE:
      return FieldTypeGroup::kNoGroup;
  }
}

FieldTypeGroup GroupTypeOfHtmlFieldType(HtmlFieldType field_type,
                                        HtmlFieldMode field_mode) {
  switch (field_type) {
    case HtmlFieldType::kName:
    case HtmlFieldType::kHonorificPrefix:
    case HtmlFieldType::kGivenName:
    case HtmlFieldType::kAdditionalName:
    case HtmlFieldType::kAdditionalNameInitial:
    case HtmlFieldType::kFamilyName:
      return field_mode == HtmlFieldMode::kBilling
                 ? FieldTypeGroup::kNameBilling
                 : FieldTypeGroup::kName;

    case HtmlFieldType::kOrganization:
      return FieldTypeGroup::kCompany;

    case HtmlFieldType::kStreetAddress:
    case HtmlFieldType::kAddressLine1:
    case HtmlFieldType::kAddressLine2:
    case HtmlFieldType::kAddressLine3:
    case HtmlFieldType::kAddressLevel1:
    case HtmlFieldType::kAddressLevel2:
    case HtmlFieldType::kAddressLevel3:
    case HtmlFieldType::kCountryCode:
    case HtmlFieldType::kCountryName:
    case HtmlFieldType::kPostalCode:
    case HtmlFieldType::kFullAddress:
      return field_mode == HtmlFieldMode::kBilling
                 ? FieldTypeGroup::kAddressBilling
                 : FieldTypeGroup::kAddressHome;

    case HtmlFieldType::kCreditCardNameFull:
    case HtmlFieldType::kCreditCardNameFirst:
    case HtmlFieldType::kCreditCardNameLast:
    case HtmlFieldType::kCreditCardNumber:
    case HtmlFieldType::kCreditCardExp:
    case HtmlFieldType::kCreditCardExpDate2DigitYear:
    case HtmlFieldType::kCreditCardExpDate4DigitYear:
    case HtmlFieldType::kCreditCardExpMonth:
    case HtmlFieldType::kCreditCardExpYear:
    case HtmlFieldType::kCreditCardExp2DigitYear:
    case HtmlFieldType::kCreditCardExp4DigitYear:
    case HtmlFieldType::kCreditCardVerificationCode:
    case HtmlFieldType::kCreditCardType:
      return FieldTypeGroup::kCreditCard;

    case HtmlFieldType::kTransactionAmount:
    case HtmlFieldType::kTransactionCurrency:
      return FieldTypeGroup::kTransaction;

    case HtmlFieldType::kTel:
    case HtmlFieldType::kTelCountryCode:
    case HtmlFieldType::kTelNational:
    case HtmlFieldType::kTelAreaCode:
    case HtmlFieldType::kTelLocal:
    case HtmlFieldType::kTelLocalPrefix:
    case HtmlFieldType::kTelLocalSuffix:
    case HtmlFieldType::kTelExtension:
      return field_mode == HtmlFieldMode::kBilling
                 ? FieldTypeGroup::kPhoneBilling
                 : FieldTypeGroup::kPhoneHome;

    case HtmlFieldType::kEmail:
      return FieldTypeGroup::kEmail;

    case HtmlFieldType::kBirthdateDay:
    case HtmlFieldType::kBirthdateMonth:
    case HtmlFieldType::kBirthdateYear:
      return FieldTypeGroup::kBirthdateField;

    case HtmlFieldType::kUpiVpa:
      // TODO(crbug/702223): Add support for UPI-VPA.
      return FieldTypeGroup::kNoGroup;

    case HtmlFieldType::kOneTimeCode:
      return FieldTypeGroup::kNoGroup;

    case HtmlFieldType::kMerchantPromoCode:
      return FieldTypeGroup::kNoGroup;

    case HtmlFieldType::kIban:
      return FieldTypeGroup::kNoGroup;

    case HtmlFieldType::kUnspecified:
    case HtmlFieldType::kUnrecognized:
      return FieldTypeGroup::kNoGroup;
  }
  NOTREACHED();
  return FieldTypeGroup::kNoGroup;
}

AutofillType::AutofillType(ServerFieldType field_type)
    : server_type_(ToSafeServerFieldType(field_type, UNKNOWN_TYPE)) {}

AutofillType::AutofillType(HtmlFieldType field_type, HtmlFieldMode mode)
    : html_type_(field_type), html_mode_(mode) {}

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
  return server_type_ == UNKNOWN_TYPE &&
         (html_type_ == HtmlFieldType::kUnspecified ||
          html_type_ == HtmlFieldType::kUnrecognized);
}

ServerFieldType AutofillType::GetStorableType() const {
  if (server_type_ != UNKNOWN_TYPE)
    return server_type_;

  switch (html_type_) {
    case HtmlFieldType::kUnspecified:
      return UNKNOWN_TYPE;

    case HtmlFieldType::kName:
      return NAME_FULL;

    case HtmlFieldType::kHonorificPrefix:
      return NAME_HONORIFIC_PREFIX;

    case HtmlFieldType::kGivenName:
      return NAME_FIRST;

    case HtmlFieldType::kAdditionalName:
      return NAME_MIDDLE;

    case HtmlFieldType::kFamilyName:
      return NAME_LAST;

    case HtmlFieldType::kOrganization:
      return COMPANY_NAME;

    case HtmlFieldType::kStreetAddress:
      return ADDRESS_HOME_STREET_ADDRESS;

    case HtmlFieldType::kAddressLine1:
      return ADDRESS_HOME_LINE1;

    case HtmlFieldType::kAddressLine2:
      return ADDRESS_HOME_LINE2;

    case HtmlFieldType::kAddressLine3:
      return ADDRESS_HOME_LINE3;

    case HtmlFieldType::kAddressLevel1:
      return ADDRESS_HOME_STATE;

    case HtmlFieldType::kAddressLevel2:
      return ADDRESS_HOME_CITY;

    case HtmlFieldType::kAddressLevel3:
      return ADDRESS_HOME_DEPENDENT_LOCALITY;

    case HtmlFieldType::kCountryCode:
    case HtmlFieldType::kCountryName:
      return ADDRESS_HOME_COUNTRY;

    case HtmlFieldType::kPostalCode:
      return ADDRESS_HOME_ZIP;

    // Full address is composed of other types; it can't be stored.
    case HtmlFieldType::kFullAddress:
      return UNKNOWN_TYPE;

    case HtmlFieldType::kCreditCardNameFull:
      return CREDIT_CARD_NAME_FULL;

    case HtmlFieldType::kCreditCardNameFirst:
      return CREDIT_CARD_NAME_FIRST;

    case HtmlFieldType::kCreditCardNameLast:
      return CREDIT_CARD_NAME_LAST;

    case HtmlFieldType::kCreditCardNumber:
      return CREDIT_CARD_NUMBER;

    case HtmlFieldType::kCreditCardExp:
      return CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;

    case HtmlFieldType::kCreditCardExpMonth:
      return CREDIT_CARD_EXP_MONTH;

    case HtmlFieldType::kCreditCardExpYear:
      return CREDIT_CARD_EXP_4_DIGIT_YEAR;

    case HtmlFieldType::kCreditCardVerificationCode:
      return CREDIT_CARD_VERIFICATION_CODE;

    case HtmlFieldType::kCreditCardType:
      return CREDIT_CARD_TYPE;

    case HtmlFieldType::kTel:
      return PHONE_HOME_WHOLE_NUMBER;

    case HtmlFieldType::kTelCountryCode:
      return PHONE_HOME_COUNTRY_CODE;

    case HtmlFieldType::kTelNational:
      return PHONE_HOME_CITY_AND_NUMBER;

    case HtmlFieldType::kTelAreaCode:
      return PHONE_HOME_CITY_CODE;

    case HtmlFieldType::kTelLocal:
      return PHONE_HOME_NUMBER;

    case HtmlFieldType::kTelLocalPrefix:
      return PHONE_HOME_NUMBER_PREFIX;

    case HtmlFieldType::kTelLocalSuffix:
      return PHONE_HOME_NUMBER_SUFFIX;

    case HtmlFieldType::kTelExtension:
      return PHONE_HOME_EXTENSION;

    case HtmlFieldType::kEmail:
      return EMAIL_ADDRESS;

    case HtmlFieldType::kBirthdateDay:
      return BIRTHDATE_DAY;
    case HtmlFieldType::kBirthdateMonth:
      return BIRTHDATE_MONTH;
    case HtmlFieldType::kBirthdateYear:
      return BIRTHDATE_4_DIGIT_YEAR;

    case HtmlFieldType::kAdditionalNameInitial:
      return NAME_MIDDLE_INITIAL;

    case HtmlFieldType::kCreditCardExpDate2DigitYear:
      return CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;

    case HtmlFieldType::kCreditCardExpDate4DigitYear:
      return CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;

    case HtmlFieldType::kCreditCardExp2DigitYear:
      return CREDIT_CARD_EXP_2_DIGIT_YEAR;

    case HtmlFieldType::kCreditCardExp4DigitYear:
      return CREDIT_CARD_EXP_4_DIGIT_YEAR;

    case HtmlFieldType::kUpiVpa:
      return UPI_VPA;

    case HtmlFieldType::kOneTimeCode:
      return ONE_TIME_CODE;

    // These types aren't stored; they're transient.
    case HtmlFieldType::kTransactionAmount:
    case HtmlFieldType::kTransactionCurrency:
    case HtmlFieldType::kMerchantPromoCode:
    case HtmlFieldType::kIban:
      return UNKNOWN_TYPE;

    case HtmlFieldType::kUnrecognized:
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
