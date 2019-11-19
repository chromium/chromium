// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/phone_number.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

TEST(PhoneNumberTest, Matcher) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  // Set phone number so country_code == 1, city_code = 650, number = 2345678.
  base::string16 phone(ASCIIToUTF16("1 [650] 234-5678"));
  PhoneNumber phone_number(&profile);
  phone_number.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), phone, "US");

  ServerFieldTypeSet matching_types;
  phone_number.GetMatchingTypes(base::string16(), "US", &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(EMPTY_TYPE) != matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("1"), "US", &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_COUNTRY_CODE) !=
              matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("16"), "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  phone_number.GetMatchingTypes(ASCIIToUTF16("165"), "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  phone_number.GetMatchingTypes(ASCIIToUTF16("1650"), "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  phone_number.GetMatchingTypes(ASCIIToUTF16("16502"), "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  phone_number.GetMatchingTypes(ASCIIToUTF16("165023"), "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  phone_number.GetMatchingTypes(ASCIIToUTF16("1650234"), "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("16502345678"), "US",
                                &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_WHOLE_NUMBER) !=
              matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("650"), "US", &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_CITY_CODE) !=
              matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("2345678"), "US", &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_NUMBER) != matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("234"), "US", &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_NUMBER) != matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("5678"), "US", &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_NUMBER) != matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("2345"), "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("6502345678"), "US",
                                &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_CITY_AND_NUMBER) !=
              matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("(650)2345678"), "US",
                                &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_CITY_AND_NUMBER) !=
              matching_types.end());
}

TEST(PhoneNumberTest, Matcher_DE) {
  // Verify that the derived types of German numbers are correct.
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("DE"));
  PhoneNumber phone_number(&profile);
  phone_number.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                       ASCIIToUTF16("+491741234567"), "DE");

  ServerFieldTypeSet matching_types;
  phone_number.GetMatchingTypes(ASCIIToUTF16("+491741234567"), "de-DE",
                                &matching_types);
  EXPECT_THAT(matching_types, testing::ElementsAre(PHONE_HOME_WHOLE_NUMBER));

  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("01741234567"), "de-DE",
                                &matching_types);
  EXPECT_THAT(matching_types, testing::ElementsAre(PHONE_HOME_CITY_AND_NUMBER));

  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("1234567"), "de-DE",
                                &matching_types);
  EXPECT_THAT(matching_types, testing::ElementsAre(PHONE_HOME_NUMBER));

  // TODO(crbug.com/638795) This is incorrect and should be 0174.
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("174"), "de-DE", &matching_types);
  EXPECT_THAT(matching_types, testing::ElementsAre(PHONE_HOME_CITY_CODE));
}

// Verify that PhoneNumber::SetInfo() correctly formats the incoming number.
TEST(PhoneNumberTest, SetInfo) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));

  PhoneNumber phone(&profile);
  EXPECT_EQ(base::string16(), phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  // Set the formatted info directly.
  EXPECT_TRUE(phone.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                            ASCIIToUTF16("(650) 234-5678"), "US"));
  EXPECT_EQ(ASCIIToUTF16("(650) 234-5678"),
            phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  // Unformatted numbers should be formatted.
  EXPECT_TRUE(phone.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                            ASCIIToUTF16("8887776666"), "US"));
  EXPECT_EQ(ASCIIToUTF16("(888) 777-6666"),
            phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_TRUE(phone.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                            ASCIIToUTF16("+18887776666"), "US"));
  EXPECT_EQ(ASCIIToUTF16("1 888-777-6666"),
            phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  // Differently formatted numbers should be left alone.
  EXPECT_TRUE(phone.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                            ASCIIToUTF16("800-432-8765"), "US"));
  EXPECT_EQ(ASCIIToUTF16("800-432-8765"),
            phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  // SetRawInfo should not try to format.
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("8004328765"));
  EXPECT_EQ(ASCIIToUTF16("8004328765"),
            phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  // Invalid numbers should not be stored.  In the US, phone numbers cannot
  // start with the digit '1'.
  EXPECT_FALSE(phone.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                             ASCIIToUTF16("650111111"), "US"));
  EXPECT_EQ(base::string16(), phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  // If the stored number is invalid due to metadata mismatch(non-existing
  // carrier code for example), but otherwise is a possible number and can be
  // parsed into different components, we should respond to queries with best
  // effort as if it is a valid number.
  EXPECT_TRUE(phone.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                            ASCIIToUTF16("5141231234"), "US"));
  EXPECT_EQ(ASCIIToUTF16("5141231234"),
            phone.GetInfo(PHONE_HOME_CITY_AND_NUMBER, "CA"));
  EXPECT_EQ(ASCIIToUTF16("5141231234"),
            phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, "CA"));
  EXPECT_EQ(ASCIIToUTF16("514"), phone.GetInfo(PHONE_HOME_CITY_CODE, "CA"));
}

// Test that cached phone numbers are correctly invalidated and updated.
TEST(PhoneNumberTest, UpdateCachedPhoneNumber) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));

  PhoneNumber phone(&profile);
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("6502345678"));
  EXPECT_EQ(ASCIIToUTF16("650"),
            phone.GetInfo(AutofillType(PHONE_HOME_CITY_CODE), "US"));

  // Update the area code.
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("8322345678"));
  EXPECT_EQ(ASCIIToUTF16("832"),
            phone.GetInfo(AutofillType(PHONE_HOME_CITY_CODE), "US"));

  // Change the phone number to have a UK format, but try to parse with the
  // wrong locale.
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("07023456789"));
  EXPECT_EQ(base::string16(),
            phone.GetInfo(AutofillType(PHONE_HOME_CITY_CODE), "US"));

  // Now try parsing using the correct locale.  Note that the profile's country
  // code should override the app locale, which is still set to "US".
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("GB"));
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("07023456789"));
  EXPECT_EQ(ASCIIToUTF16("70"),
            phone.GetInfo(AutofillType(PHONE_HOME_CITY_CODE), "US"));
}

TEST(PhoneNumberTest, PhoneCombineHelper) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));

  PhoneNumber::PhoneCombineHelper number1;
  EXPECT_FALSE(
      number1.SetInfo(AutofillType(ADDRESS_BILLING_CITY), ASCIIToUTF16("1")));
  EXPECT_TRUE(number1.SetInfo(AutofillType(PHONE_HOME_COUNTRY_CODE),
                              ASCIIToUTF16("1")));
  EXPECT_TRUE(
      number1.SetInfo(AutofillType(PHONE_HOME_CITY_CODE), ASCIIToUTF16("650")));
  EXPECT_TRUE(number1.SetInfo(AutofillType(PHONE_HOME_NUMBER),
                              ASCIIToUTF16("2345678")));
  base::string16 parsed_phone;
  EXPECT_TRUE(number1.ParseNumber(profile, "en-US", &parsed_phone));
  // International format as it has a country code.
  EXPECT_EQ(ASCIIToUTF16("1 650-234-5678"), parsed_phone);

  PhoneNumber::PhoneCombineHelper number3;
  EXPECT_TRUE(
      number3.SetInfo(AutofillType(PHONE_HOME_CITY_CODE), ASCIIToUTF16("650")));
  EXPECT_TRUE(number3.SetInfo(AutofillType(PHONE_HOME_NUMBER),
                              ASCIIToUTF16("2345680")));
  EXPECT_TRUE(number3.ParseNumber(profile, "en-US", &parsed_phone));
  // National format as it does not have a country code.
  EXPECT_EQ(ASCIIToUTF16("(650) 234-5680"), parsed_phone);

  PhoneNumber::PhoneCombineHelper number4;
  EXPECT_TRUE(number4.SetInfo(AutofillType(PHONE_HOME_CITY_CODE),
                              ASCIIToUTF16("123")));  // Incorrect city code.
  EXPECT_TRUE(number4.SetInfo(AutofillType(PHONE_HOME_NUMBER),
                              ASCIIToUTF16("2345680")));
  EXPECT_TRUE(number4.ParseNumber(profile, "en-US", &parsed_phone));
  EXPECT_EQ(ASCIIToUTF16("1232345680"), parsed_phone);

  PhoneNumber::PhoneCombineHelper number5;
  EXPECT_TRUE(number5.SetInfo(AutofillType(PHONE_HOME_CITY_AND_NUMBER),
                              ASCIIToUTF16("6502345681")));
  EXPECT_TRUE(number5.ParseNumber(profile, "en-US", &parsed_phone));
  EXPECT_EQ(ASCIIToUTF16("(650) 234-5681"), parsed_phone);

  PhoneNumber::PhoneCombineHelper number6;
  EXPECT_TRUE(
      number6.SetInfo(AutofillType(PHONE_HOME_CITY_CODE), ASCIIToUTF16("650")));
  EXPECT_TRUE(
      number6.SetInfo(AutofillType(PHONE_HOME_NUMBER), ASCIIToUTF16("234")));
  EXPECT_TRUE(
      number6.SetInfo(AutofillType(PHONE_HOME_NUMBER), ASCIIToUTF16("5682")));
  EXPECT_TRUE(number6.ParseNumber(profile, "en-US", &parsed_phone));
  EXPECT_EQ(ASCIIToUTF16("(650) 234-5682"), parsed_phone);

  // Ensure parsing is possible when falling back to detecting the country code
  // based on the app locale.
  PhoneNumber::PhoneCombineHelper number7;
  EXPECT_TRUE(
      number7.SetInfo(AutofillType(PHONE_HOME_CITY_CODE), ASCIIToUTF16("650")));
  EXPECT_TRUE(
      number7.SetInfo(AutofillType(PHONE_HOME_NUMBER), ASCIIToUTF16("234")));
  EXPECT_TRUE(
      number7.SetInfo(AutofillType(PHONE_HOME_NUMBER), ASCIIToUTF16("5682")));
  EXPECT_TRUE(number7.ParseNumber(AutofillProfile(), "en-US", &parsed_phone));
  EXPECT_EQ(ASCIIToUTF16("(650) 234-5682"), parsed_phone);
}

TEST(PhoneNumberTest, InternationalPhoneHomeCityAndNumber_US) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  // Set phone number so country_code == 1, city_code = 650, number = 2345678.
  base::string16 phone(ASCIIToUTF16("+1 (650) 234-5678"));
  PhoneNumber phone_number(&profile);
  phone_number.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), phone, "en-US");
  EXPECT_EQ(ASCIIToUTF16("6502345678"),
            phone_number.GetInfo(PHONE_HOME_CITY_AND_NUMBER, "en-US"));
}

// This is a regression test for crbug.com/638795.
TEST(PhoneNumberTest, InternationalPhoneHomeCityAndNumber_DE) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("DE"));
  // Set phone number so country_code == 49, city_code = 174, number = 12 34
  // 567.
  base::string16 phone(ASCIIToUTF16("+49 (174) 12 34 567"));
  PhoneNumber phone_number(&profile);
  phone_number.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), phone, "en-US");
  // Note that for German numbers (unlike US numbers), the
  // PHONE_HOME_CITY_AND_NUMBER should start with a 0.
  EXPECT_EQ(ASCIIToUTF16("01741234567"),
            phone_number.GetInfo(PHONE_HOME_CITY_AND_NUMBER, "en-US"));
}

}  // namespace autofill
