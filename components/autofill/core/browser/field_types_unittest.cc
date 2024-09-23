// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_types.h"

#include "components/autofill/core/browser/field_type_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(FieldTypesTest, TypeStringConversion) {
  EXPECT_EQ(TypeNameToFieldType(FieldTypeToStringView(NO_SERVER_DATA)),
            NO_SERVER_DATA);
  for (int i = 0; i < MAX_VALID_FIELD_TYPE; ++i) {
    if (FieldType raw_value = static_cast<FieldType>(i);
        ToSafeFieldType(raw_value, NO_SERVER_DATA) != NO_SERVER_DATA) {
      EXPECT_EQ(TypeNameToFieldType(FieldTypeToStringView(raw_value)),
                raw_value);
    }
  }
}

TEST(FieldTypesTest, IsValidFieldType) {
  const std::set<FieldType> kValidFieldTypes{
      NO_SERVER_DATA,
      UNKNOWN_TYPE,
      EMPTY_TYPE,
      NAME_FIRST,
      NAME_MIDDLE,
      NAME_LAST,
      NAME_MIDDLE_INITIAL,
      NAME_FULL,
      NAME_SUFFIX,
      EMAIL_ADDRESS,
      PHONE_HOME_NUMBER,
      PHONE_HOME_NUMBER_PREFIX,
      PHONE_HOME_NUMBER_SUFFIX,
      PHONE_HOME_CITY_CODE,
      PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX,
      PHONE_HOME_COUNTRY_CODE,
      PHONE_HOME_CITY_AND_NUMBER,
      PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX,
      PHONE_HOME_WHOLE_NUMBER,
      ADDRESS_HOME_LINE1,
      ADDRESS_HOME_LINE2,
      ADDRESS_HOME_APT,
      ADDRESS_HOME_APT_NUM,
      ADDRESS_HOME_APT_TYPE,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY,
      CREDIT_CARD_NAME_FULL,
      CREDIT_CARD_NUMBER,
      CREDIT_CARD_EXP_MONTH,
      CREDIT_CARD_EXP_2_DIGIT_YEAR,
      CREDIT_CARD_EXP_4_DIGIT_YEAR,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
      CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
      CREDIT_CARD_STANDALONE_VERIFICATION_CODE,
      CREDIT_CARD_TYPE,
      CREDIT_CARD_VERIFICATION_CODE,
      COMPANY_NAME,
      FIELD_WITH_DEFAULT_VALUE,
      MERCHANT_EMAIL_SIGNUP,
      MERCHANT_PROMO_CODE,
      PASSWORD,
      ACCOUNT_CREATION_PASSWORD,
      ADDRESS_HOME_STREET_ADDRESS,
      ADDRESS_HOME_SORTING_CODE,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_LINE3,
      NOT_ACCOUNT_CREATION_PASSWORD,
      USERNAME,
      USERNAME_AND_EMAIL_ADDRESS,
      NEW_PASSWORD,
      PROBABLY_NEW_PASSWORD,
      NOT_NEW_PASSWORD,
      CREDIT_CARD_NAME_FIRST,
      CREDIT_CARD_NAME_LAST,
      PHONE_HOME_EXTENSION,
      CONFIRMATION_PASSWORD,
      AMBIGUOUS_TYPE,
      SEARCH_TERM,
      PRICE,
      NOT_PASSWORD,
      SINGLE_USERNAME,
      NOT_USERNAME,
      IBAN_VALUE,
      ADDRESS_HOME_STREET_NAME,
      ADDRESS_HOME_HOUSE_NUMBER,
      ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
      ADDRESS_HOME_SUBPREMISE,
      ADDRESS_HOME_OTHER_SUBUNIT,
      NAME_LAST_FIRST,
      NAME_LAST_CONJUNCTION,
      NAME_LAST_SECOND,
      NAME_HONORIFIC_PREFIX,
      ADDRESS_HOME_ADDRESS,
      ADDRESS_HOME_ADDRESS_WITH_NAME,
      ADDRESS_HOME_FLOOR,
      NUMERIC_QUANTITY,
      ONE_TIME_CODE,
      ADDRESS_HOME_LANDMARK,
      ADDRESS_HOME_BETWEEN_STREETS,
      ADDRESS_HOME_ADMIN_LEVEL2,
      DELIVERY_INSTRUCTIONS,
      ADDRESS_HOME_OVERFLOW,
      ADDRESS_HOME_STREET_LOCATION,
      ADDRESS_HOME_BETWEEN_STREETS_1,
      ADDRESS_HOME_BETWEEN_STREETS_2,
      ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
      ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
      ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY,
      ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK,
      ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK,
      SINGLE_USERNAME_FORGOT_PASSWORD,
      SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES,
      IMPROVED_PREDICTION};
  FieldType kInvalidValue = static_cast<FieldType>(123456);
  ASSERT_FALSE(kValidFieldTypes.count(kInvalidValue));
  for (int i = -10; i < MAX_VALID_FIELD_TYPE + 10; ++i) {
    FieldType raw_value = static_cast<FieldType>(i);
    EXPECT_EQ(ToSafeFieldType(raw_value, kInvalidValue),
              kValidFieldTypes.count(raw_value) ? raw_value : kInvalidValue);
  }
}

TEST(FieldTypesTest, TestWith2DigitExpirationYear) {
  FieldType assumed_field_type =
      ToSafeFieldType(CREDIT_CARD_EXP_2_DIGIT_YEAR, NO_SERVER_DATA);
  size_t result = DetermineExpirationYearLength(assumed_field_type);
  EXPECT_EQ(result, static_cast<size_t>(2));
}

TEST(FieldTypesTest, TestWith4DigitExpirationYear) {
  FieldType assumed_field_type =
      ToSafeFieldType(CREDIT_CARD_EXP_4_DIGIT_YEAR, NO_SERVER_DATA);
  size_t result = DetermineExpirationYearLength(assumed_field_type);
  EXPECT_EQ(result, static_cast<size_t>(4));
}

}  // namespace autofill
