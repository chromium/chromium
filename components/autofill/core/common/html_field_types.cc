// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/common/html_field_types.h"

#include "base/notreached.h"

namespace autofill {

std::string_view FieldTypeToStringView(HtmlFieldType type) {
  switch (type) {
    case HtmlFieldType::kUnspecified:
      return "HTML_TYPE_UNSPECIFIED";
    case HtmlFieldType::kName:
      return "HTML_TYPE_NAME";
    case HtmlFieldType::kHonorificPrefix:
      return "HTML_TYPE_HONORIFIC_PREFIX";
    case HtmlFieldType::kGivenName:
      return "HTML_TYPE_GIVEN_NAME";
    case HtmlFieldType::kAdditionalName:
      return "HTML_TYPE_ADDITIONAL_NAME";
    case HtmlFieldType::kFamilyName:
      return "HTML_TYPE_FAMILY_NAME";
    case HtmlFieldType::kOrganization:
      return "HTML_TYPE_ORGANIZATION";
    case HtmlFieldType::kStreetAddress:
      return "HTML_TYPE_STREET_ADDRESS";
    case HtmlFieldType::kAddressLine1:
      return "HTML_TYPE_ADDRESS_LINE1";
    case HtmlFieldType::kAddressLine2:
      return "HTML_TYPE_ADDRESS_LINE2";
    case HtmlFieldType::kAddressLine3:
      return "HTML_TYPE_ADDRESS_LINE3";
    case HtmlFieldType::kAddressLevel1:
      return "HTML_TYPE_ADDRESS_LEVEL1";
    case HtmlFieldType::kAddressLevel2:
      return "HTML_TYPE_ADDRESS_LEVEL2";
    case HtmlFieldType::kAddressLevel3:
      return "HTML_TYPE_ADDRESS_LEVEL3";
    case HtmlFieldType::kCountryCode:
      return "HTML_TYPE_COUNTRY_CODE";
    case HtmlFieldType::kCountryName:
      return "HTML_TYPE_COUNTRY_NAME";
    case HtmlFieldType::kPostalCode:
      return "HTML_TYPE_POSTAL_CODE";
    case HtmlFieldType::kCreditCardNameFull:
      return "HTML_TYPE_CREDIT_CARD_NAME_FULL";
    case HtmlFieldType::kCreditCardNameFirst:
      return "HTML_TYPE_CREDIT_CARD_NAME_FIRST";
    case HtmlFieldType::kCreditCardNameLast:
      return "HTML_TYPE_CREDIT_CARD_NAME_LAST";
    case HtmlFieldType::kCreditCardNumber:
      return "HTML_TYPE_CREDIT_CARD_NUMBER";
    case HtmlFieldType::kCreditCardExp:
      return "HTML_TYPE_CREDIT_CARD_EXP";
    case HtmlFieldType::kCreditCardExpMonth:
      return "HTML_TYPE_CREDIT_CARD_EXP_MONTH";
    case HtmlFieldType::kCreditCardExpYear:
      return "HTML_TYPE_CREDIT_CARD_EXP_YEAR";
    case HtmlFieldType::kCreditCardVerificationCode:
      return "HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE";
    case HtmlFieldType::kCreditCardType:
      return "HTML_TYPE_CREDIT_CARD_TYPE";
    case HtmlFieldType::kTel:
      return "HTML_TYPE_TEL";
    case HtmlFieldType::kTelCountryCode:
      return "HTML_TYPE_TEL_COUNTRY_CODE";
    case HtmlFieldType::kTelNational:
      return "HTML_TYPE_TEL_NATIONAL";
    case HtmlFieldType::kTelAreaCode:
      return "HTML_TYPE_TEL_AREA_CODE";
    case HtmlFieldType::kTelLocal:
      return "HTML_TYPE_TEL_LOCAL";
    case HtmlFieldType::kTelLocalPrefix:
      return "HTML_TYPE_TEL_LOCAL_PREFIX";
    case HtmlFieldType::kTelLocalSuffix:
      return "HTML_TYPE_TEL_LOCAL_SUFFIX";
    case HtmlFieldType::kTelExtension:
      return "HTML_TYPE_TEL_EXTENSION";
    case HtmlFieldType::kEmail:
      return "HTML_TYPE_EMAIL";
    case HtmlFieldType::kBirthdateDay:
      return "HTML_TYPE_BIRTHDATE_DAY";
    case HtmlFieldType::kBirthdateMonth:
      return "HTML_TYPE_BIRTHDATE_MONTH";
    case HtmlFieldType::kBirthdateYear:
      return "HTML_TYPE_BIRTHDATE_YEAR";
    case HtmlFieldType::kTransactionAmount:
      return "HTML_TYPE_TRANSACTION_AMOUNT";
    case HtmlFieldType::kTransactionCurrency:
      return "HTML_TYPE_TRANSACTION_CURRENCY";
    case HtmlFieldType::kAdditionalNameInitial:
      return "HTML_TYPE_ADDITIONAL_NAME_INITIAL";
    case HtmlFieldType::kCreditCardExpDate2DigitYear:
      return "HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR";
    case HtmlFieldType::kCreditCardExpDate4DigitYear:
      return "HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR";
    case HtmlFieldType::kCreditCardExp2DigitYear:
      return "HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR";
    case HtmlFieldType::kCreditCardExp4DigitYear:
      return "HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR";
    case HtmlFieldType::kOneTimeCode:
      return "HTML_TYPE_ONE_TIME_CODE";
    case HtmlFieldType::kMerchantPromoCode:
      return "HTML_TYPE_MERCHANT_PROMO_CODE";
    case HtmlFieldType::kIban:
      return "HTML_TYPE_IBAN";
    case HtmlFieldType::kUnrecognized:
      return "HTML_TYPE_UNRECOGNIZED";
  }

  NOTREACHED_IN_MIGRATION();
  return "";
}

std::string FieldTypeToString(HtmlFieldType type) {
  return std::string(FieldTypeToStringView(type));
}

std::string_view HtmlFieldModeToStringView(HtmlFieldMode mode) {
  switch (mode) {
    case HtmlFieldMode::kNone:
      return "";
    case HtmlFieldMode::kBilling:
      return "billing";
    case HtmlFieldMode::kShipping:
      return "shipping";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

std::string HtmlFieldModeToString(HtmlFieldMode mode) {
  return std::string(HtmlFieldModeToStringView(mode));
}

}  // namespace autofill
