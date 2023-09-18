// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/phone_number.h"

#include <string>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

constexpr char kLocale[] = "en_US";

struct MatchingTypesTestCase {
  std::u16string input;
  ServerFieldTypeSet expected_types;
};
// Constructs a `PhoneNumber` from `number` and verifies that the matching types
// on `input` equal `expected_type` on every test case in `tests`.
// If the `number` is provided without a country code, it's region defaults to
// `country`.
// `trunk_types_enabled` determines if
// `kAutofillEnableSupportForPhoneNumberTrunkTypes` is enabled.
void MatchingTypesTest(const std::u16string& number,
                       const std::u16string& country,
                       bool trunk_types_enabled,
                       const std::vector<MatchingTypesTestCase>& tests) {
  base::test::ScopedFeatureList trunk_types;
  trunk_types.InitWithFeatureState(
      features::kAutofillEnableSupportForPhoneNumberTrunkTypes,
      trunk_types_enabled);

  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, country);
  PhoneNumber phone_number(&profile);
  // `kLocale` is irrelevant, because `profile` has country information.
  phone_number.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), number, kLocale);
  for (const MatchingTypesTestCase& test : tests) {
    ServerFieldTypeSet matching_types;
    phone_number.GetMatchingTypes(test.input, kLocale, &matching_types);
    EXPECT_EQ(matching_types, test.expected_types);
  }
}

TEST(PhoneNumberTest, Matcher) {
  // Set phone number so country_code == 1, city_code = 650, number = 2345678.
  MatchingTypesTest(u"1 [650] 234-5678", u"US",
                    /*trunk_types_enabled=*/false,
                    {{std::u16string(), {EMPTY_TYPE}},
                     {u"1", {PHONE_HOME_COUNTRY_CODE}},
                     {u"16", {}},
                     {u"165", {}},
                     {u"1650", {}},
                     {u"16502", {}},
                     {u"165023", {}},
                     {u"1650234", {}},
                     {u"16502345678", {PHONE_HOME_WHOLE_NUMBER}},
                     {u"650", {PHONE_HOME_CITY_CODE}},
                     {u"2345678", {PHONE_HOME_NUMBER}},
                     {u"234", {PHONE_HOME_NUMBER_PREFIX}},
                     {u"5678", {PHONE_HOME_NUMBER_SUFFIX}},
                     {u"2345", {}},
                     {u"6502345678", {PHONE_HOME_CITY_AND_NUMBER}},
                     {u"(650)2345678", {PHONE_HOME_CITY_AND_NUMBER}}});
}

TEST(PhoneNumberTest, Matcher_DisambiguateWholeNumber) {
  MatchingTypesTest(u"(234) 567-8901", u"US",
                    /*trunk_types_enabled=*/false,
                    {{u"2345678901", {PHONE_HOME_CITY_AND_NUMBER}}});
  MatchingTypesTest(u"(234) 567-8901", u"US",
                    /*trunk_types_enabled=*/true,
                    {{u"2345678901",
                      {PHONE_HOME_CITY_AND_NUMBER,
                       PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}}});
}

// Verify that the derived types of German numbers are correct.
TEST(PhoneNumberTest, Matcher_DE) {
  MatchingTypesTest(u"+491741234567", u"DE",
                    /*trunk_types_enabled=*/false,
                    {{u"+491741234567", {PHONE_HOME_WHOLE_NUMBER}},
                     {u"01741234567", {PHONE_HOME_CITY_AND_NUMBER}},
                     {u"1234567", {PHONE_HOME_NUMBER}},
                     {u"174", {PHONE_HOME_CITY_CODE}}});
}

TEST(PhoneNumberTest, Matcher_TrunkTypes) {
  MatchingTypesTest(
      u"1 [650] 234-5678", u"US",
      /*trunk_types_enabled=*/true,
      {{u"650", {PHONE_HOME_CITY_CODE, PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX}},
       {u"6502345678",
        {PHONE_HOME_CITY_AND_NUMBER,
         PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}}});
}

TEST(PhoneNumberTest, Matcher_TrunkTypes_DE) {
  MatchingTypesTest(
      u"+491741234567", u"DE",
      /*trunk_types_enabled=*/true,
      {{u"174", {PHONE_HOME_CITY_CODE}},
       {u"0174", {PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX}},
       {u"1741234567", {PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}},
       {u"01741234567", {PHONE_HOME_CITY_AND_NUMBER}}});
}

// Verify that `PhoneNumber::SetInfo()` correctly formats the incoming number.
// `kLocale` is irrelevant, as `profile` has a country.
TEST(PhoneNumberTest, SetInfo) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

  PhoneNumber phone(&profile);
  EXPECT_TRUE(phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER).empty());
  EXPECT_TRUE(phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale).empty());

  // Set the formatted info directly.
  EXPECT_TRUE(
      phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"(650) 234-5678", kLocale));
  EXPECT_EQ(u"(650) 234-5678", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(u"6502345678", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));

  // Unformatted numbers should be formatted.
  EXPECT_TRUE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"8887776666", kLocale));
  EXPECT_EQ(u"(888) 777-6666", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(u"8887776666", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));

  EXPECT_TRUE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"+18887776666", kLocale));
  EXPECT_EQ(u"1 888-777-6666", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(u"18887776666", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));

  // Differently formatted numbers should be left alone.
  EXPECT_TRUE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"800-432-8765", kLocale));
  EXPECT_EQ(u"800-432-8765", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(u"8004328765", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));

  // SetRawInfo should not try to format.
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"8004328765");
  EXPECT_EQ(u"8004328765", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  // Invalid numbers should not be stored. In the US, phone numbers cannot start
  // with the digit '1'.
  EXPECT_FALSE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"650111111", kLocale));
  EXPECT_TRUE(phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER).empty());
  EXPECT_TRUE(phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale).empty());

  // If the stored number is invalid due to metadata mismatch (non-existing
  // carrier code for example), but otherwise is a possible number and can be
  // parsed into different components, we should respond to queries with best
  // effort as if it is a valid number.
  EXPECT_TRUE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"5141231234", kLocale));
  EXPECT_EQ(u"5141231234", phone.GetInfo(PHONE_HOME_CITY_AND_NUMBER, kLocale));
  EXPECT_EQ(u"5141231234", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));
  EXPECT_EQ(u"514", phone.GetInfo(PHONE_HOME_CITY_CODE, kLocale));
}

TEST(PhoneNumberTest, InferCountryCallingCode) {
  base::test::ScopedFeatureList complement_calling_code_enabled;
  complement_calling_code_enabled.InitAndEnableFeature(
      features::kAutofillInferCountryCallingCode);

  AutofillProfile profile;
  PhoneNumber phone(&profile);

  // No country information available and thus no calling code inferred.
  EXPECT_TRUE(
      phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"(650) 234-5678", kLocale));
  EXPECT_TRUE(phone.GetInfo(PHONE_HOME_COUNTRY_CODE, kLocale).empty());
  EXPECT_EQ(u"6502345678", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));
  EXPECT_EQ(u"(650) 234-5678", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(u"6502345678", phone.GetInfo(PHONE_HOME_CITY_AND_NUMBER, kLocale));

  // With country information available, the calling code is inferred.
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  EXPECT_TRUE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"6502345678", kLocale));
  EXPECT_EQ(u"1", phone.GetInfo(PHONE_HOME_COUNTRY_CODE, kLocale));
  EXPECT_EQ(u"16502345678", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));
  EXPECT_EQ(u"1 650-234-5678", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(u"6502345678", phone.GetInfo(PHONE_HOME_CITY_AND_NUMBER, kLocale));

  // Pre-formatted number.
  // In this case the raw info is kept as-is, while the calling code is inferred
  // for the filling information.
  EXPECT_TRUE(
      phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"(650) 234-5678", kLocale));
  EXPECT_EQ(u"1", phone.GetInfo(PHONE_HOME_COUNTRY_CODE, kLocale));
  EXPECT_EQ(u"16502345678", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));
  EXPECT_EQ(u"(650) 234-5678", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(u"6502345678", phone.GetInfo(PHONE_HOME_CITY_AND_NUMBER, kLocale));

  // Different country.
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");
  EXPECT_TRUE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"015787912345", kLocale));
  EXPECT_EQ(u"49", phone.GetInfo(PHONE_HOME_COUNTRY_CODE, kLocale));
  EXPECT_EQ(u"+4915787912345", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));
  EXPECT_EQ(u"+49 1578 7912345", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(u"015787912345",
            phone.GetInfo(PHONE_HOME_CITY_AND_NUMBER, kLocale));
}

// Test that cached phone numbers are correctly invalidated and updated.
TEST(PhoneNumberTest, UpdateCachedPhoneNumber) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

  PhoneNumber phone(&profile);
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"6502345678");
  EXPECT_EQ(u"650", phone.GetInfo(AutofillType(PHONE_HOME_CITY_CODE), "US"));

  // Update the area code.
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"8322345678");
  EXPECT_EQ(u"832", phone.GetInfo(AutofillType(PHONE_HOME_CITY_CODE), "US"));

  // Change the phone number to have a UK format, but try to parse with the
  // wrong locale.
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"07023456789");
  EXPECT_EQ(std::u16string(),
            phone.GetInfo(AutofillType(PHONE_HOME_CITY_CODE), "US"));

  // Now try parsing using the correct locale.  Note that the profile's country
  // code should override the app locale, which is still set to "US".
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"GB");
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"07023456789");
  EXPECT_EQ(u"70", phone.GetInfo(AutofillType(PHONE_HOME_CITY_CODE), "US"));
}

TEST(PhoneNumberTest, PhoneCombineHelper) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

  PhoneNumber::PhoneCombineHelper number1;
  EXPECT_FALSE(number1.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), u"1"));
  EXPECT_TRUE(number1.SetInfo(AutofillType(PHONE_HOME_COUNTRY_CODE), u"1"));
  EXPECT_TRUE(number1.SetInfo(AutofillType(PHONE_HOME_CITY_CODE), u"650"));
  EXPECT_TRUE(number1.SetInfo(AutofillType(PHONE_HOME_NUMBER), u"2345678"));
  std::u16string parsed_phone;
  EXPECT_TRUE(number1.ParseNumber(profile, kLocale, &parsed_phone));
  // International format as it has a country code.
  EXPECT_EQ(u"1 650-234-5678", parsed_phone);

  PhoneNumber::PhoneCombineHelper number3;
  EXPECT_TRUE(number3.SetInfo(AutofillType(PHONE_HOME_CITY_CODE), u"650"));
  EXPECT_TRUE(number3.SetInfo(AutofillType(PHONE_HOME_NUMBER), u"2345680"));
  EXPECT_TRUE(number3.ParseNumber(profile, kLocale, &parsed_phone));
  // National format as it does not have a country code.
  EXPECT_EQ(u"(650) 234-5680", parsed_phone);

  PhoneNumber::PhoneCombineHelper number4;
  EXPECT_TRUE(number4.SetInfo(AutofillType(PHONE_HOME_CITY_CODE),
                              u"123"));  // Incorrect city code.
  EXPECT_TRUE(number4.SetInfo(AutofillType(PHONE_HOME_NUMBER), u"2345680"));
  EXPECT_TRUE(number4.ParseNumber(profile, kLocale, &parsed_phone));
  EXPECT_EQ(u"1232345680", parsed_phone);

  PhoneNumber::PhoneCombineHelper number5;
  EXPECT_TRUE(
      number5.SetInfo(AutofillType(PHONE_HOME_CITY_AND_NUMBER), u"6502345681"));
  EXPECT_TRUE(number5.ParseNumber(profile, kLocale, &parsed_phone));
  EXPECT_EQ(u"(650) 234-5681", parsed_phone);

  PhoneNumber::PhoneCombineHelper number6;
  EXPECT_TRUE(number6.SetInfo(AutofillType(PHONE_HOME_CITY_CODE), u"650"));
  EXPECT_TRUE(number6.SetInfo(AutofillType(PHONE_HOME_NUMBER_PREFIX), u"234"));
  EXPECT_TRUE(number6.SetInfo(AutofillType(PHONE_HOME_NUMBER_SUFFIX), u"5682"));
  EXPECT_TRUE(number6.ParseNumber(profile, kLocale, &parsed_phone));
  EXPECT_EQ(u"(650) 234-5682", parsed_phone);

  // Ensure parsing is possible when falling back to detecting the country code
  // based on the app locale.
  PhoneNumber::PhoneCombineHelper number7;
  EXPECT_TRUE(number7.SetInfo(AutofillType(PHONE_HOME_CITY_CODE), u"650"));
  EXPECT_TRUE(number7.SetInfo(AutofillType(PHONE_HOME_NUMBER_PREFIX), u"234"));
  EXPECT_TRUE(number7.SetInfo(AutofillType(PHONE_HOME_NUMBER_SUFFIX), u"5682"));
  EXPECT_TRUE(number7.ParseNumber(AutofillProfile(), kLocale, &parsed_phone));
  EXPECT_EQ(u"(650) 234-5682", parsed_phone);
}

TEST(PhoneNumberTest, HelperSetsAllPhoneFieldTypes) {
  AutofillProfile profile;
  PhoneNumber phone_number(&profile);

  ServerFieldTypeSet types;
  profile.GetSupportedTypes(&types);
  std::vector<ServerFieldType> fields{types.begin(), types.end()};
  base::EraseIf(fields, [](ServerFieldType type) {
    return AutofillType(type).group() != FieldTypeGroup::kPhone;
  });

  base::ranges::for_each(fields, [](ServerFieldType type) {
    PhoneNumber::PhoneCombineHelper helper;
    EXPECT_TRUE(helper.SetInfo(AutofillType(type), u"123"));
  });
}

TEST(PhoneNumberTest, InternationalPhoneHomeCityAndNumber_US) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  // Set phone number so country_code == 1, city_code = 650, number = 2345678.
  std::u16string phone(u"+1 (650) 234-5678");
  PhoneNumber phone_number(&profile);
  phone_number.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), phone, "en-US");
  EXPECT_EQ(u"6502345678",
            phone_number.GetInfo(PHONE_HOME_CITY_AND_NUMBER, "en-US"));
}

// This is a regression test for crbug.com/638795.
TEST(PhoneNumberTest, InternationalPhoneHomeCityAndNumber_DE) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");
  // Set phone number so country_code == 49, city_code = 174, number = 12 34
  // 567.
  std::u16string phone(u"+49 (174) 12 34 567");
  PhoneNumber phone_number(&profile);
  phone_number.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), phone, "en-US");
  // Note that for German numbers (unlike US numbers), the
  // PHONE_HOME_CITY_AND_NUMBER should start with a 0.
  EXPECT_EQ(u"01741234567",
            phone_number.GetInfo(PHONE_HOME_CITY_AND_NUMBER, "en-US"));
}

TEST(PhoneNumberTest, TrunkPrefix) {
  AutofillProfile profile;

  // Constructs a `PhoneNumber` object from `number` and verifies that the
  // city-code and city-and-number types with and without trunk prefix are
  // computed correctly.
  auto TestNumber = [&](const std::u16string& number,
                        const std::u16string& city_code_with_trunk,
                        const std::u16string& city_code_without_trunk,
                        const std::u16string& city_number_with_trunk,
                        const std::u16string& city_number_without_trunk) {
    // The `locale` is irrelevant, as the `profile` has country information.
    const std::string locale = "en-US";
    PhoneNumber phone_number(&profile);
    phone_number.SetInfo(PHONE_HOME_WHOLE_NUMBER, number, locale);
    EXPECT_EQ(
        city_code_with_trunk,
        phone_number.GetInfo(PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX, locale));
    EXPECT_EQ(city_code_without_trunk,
              phone_number.GetInfo(PHONE_HOME_CITY_CODE, locale));
    EXPECT_EQ(city_number_with_trunk,
              phone_number.GetInfo(PHONE_HOME_CITY_AND_NUMBER, locale));
    EXPECT_EQ(city_number_without_trunk,
              phone_number.GetInfo(
                  PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX, locale));
  };

  // US: No trunk prefixes used.
  {
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
    TestNumber(u"+1 (650) 234-5678", u"650", u"650", u"6502345678",
               u"6502345678");
    TestNumber(u"800-555-0199", u"800", u"800", u"8005550199", u"8005550199");
  }

  // DE: Trunk prefix used in international format.
  {
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");
    TestNumber(u"+49 (174) 12 34 567", u"0174", u"174", u"01741234567",
               u"1741234567");
    TestNumber(u"0 1578 7912345", u"01578", u"1578", u"015787912345",
               u"15787912345");
  }

  // IT: A leading zero, which is not a trunk prefix, is used in national and
  // international format for landline number. Mobile numbers are never prefixed
  // with a 0.
  {
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"IT");
    // Landline.
    TestNumber(u"+39 06 85870848", u"06", u"06", u"0685870848", u"0685870848");
    TestNumber(u"06 85870848", u"06", u"06", u"0685870848", u"0685870848");
    // Mobile.
    TestNumber(u"+39 338 1234567", u"338", u"338", u"3381234567",
               u"3381234567");
    TestNumber(u"338 1234567", u"338", u"338", u"3381234567", u"3381234567");
  }

  // RU: An 8 is used as a trunk prefix.
  {
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"RU");
    TestNumber(u"+7 495 123 45 67", u"8495", u"495", u"84951234567",
               u"4951234567");
  }
}

// Tests that PHONE_HOME_NUMBER_PREFIX and PHONE_HOME_NUMBER_PREFIX are
// extracted correctly.
TEST(PhoneNumberTest, NumberPreAndSuffixes) {
  AutofillProfile profile;

  // Constructs a `PhoneNumber` object from `number` and verifies that the
  // pre- and suffix match the expectation.
  auto TestNumber = [&](const std::u16string& number,
                        const std::u16string& prefix,
                        const std::u16string& suffix) {
    // The `locale` is irrelevant, as the `profile` has country information.
    const std::string locale = "en-US";
    PhoneNumber phone_number(&profile);
    EXPECT_TRUE(phone_number.SetInfo(PHONE_HOME_WHOLE_NUMBER, number, locale));
    EXPECT_EQ(prefix, phone_number.GetInfo(PHONE_HOME_NUMBER_PREFIX, locale));
    EXPECT_EQ(suffix, phone_number.GetInfo(PHONE_HOME_NUMBER_SUFFIX, locale));
  };

  // US
  {
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
    TestNumber(u"(650) 234-5678", u"234", u"5678");
  }
  // JP
  {
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"JP");
    TestNumber(u"03-3224-9999", u"3224", u"9999");   // Landline
    TestNumber(u"090-1234-5678", u"1234", u"5678");  // Mobile
    TestNumber(u"+81 824-86-3123", u"86", u"3123");  // Different length prefix
  }
  // DE
  {
    // Emergency numbers can be shorter than 4 digits. Make sure we don't crash.
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");
    TestNumber(u"110", u"", u"110");
  }
}

// Tests that extensions are not stored and even stripped from the raw info.
TEST(PhoneNumberTest, Extension) {
  AutofillProfile profile;
  PhoneNumber phone(&profile);
  const std::string locale = "en-US";
  EXPECT_TRUE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"(650) 234-2345 ext. 234",
                            locale));
  EXPECT_EQ(u"(650) 234-2345", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(u"6502342345", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, locale));
  EXPECT_TRUE(phone.GetInfo(PHONE_HOME_EXTENSION, locale).empty());
}

// Tests whether the |PHONE_HOME_COUNTRY_CODE| is added to the set of matching
// types.
TEST(PhoneNumberTest, CountryCodeInMatchingTypes) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  // Set the phone number such that country_code == 1, city_code = 650,
  // number = 2345678.
  std::u16string phone(u"1 [650] 234-5678");
  PhoneNumber phone_number(&profile);
  phone_number.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), phone, "US");

  std::vector<const char*> test_cases = {"+1", "1", "(+1) United States",
                                         "US (+1)"};

  for (size_t i = 0; i < test_cases.size(); i++) {
    SCOPED_TRACE(testing::Message() << "i(US) = " << i);

    ServerFieldTypeSet matching_types;
    phone_number.GetMatchingTypes(ASCIIToUTF16(test_cases[i]), "US",
                                  &matching_types);

    EXPECT_THAT(matching_types, testing::ElementsAre(PHONE_HOME_COUNTRY_CODE));
  }

  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");
  std::u16string de_phone(u"+49 0151 6679586");
  PhoneNumber phone_number_de(&profile);
  constexpr char kLocaleDE[] = "de_DE";
  phone_number_de.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), de_phone,
                          kLocaleDE);

  test_cases = {"49", "+49", "(+49) DE", "(0049) DE", "0049"};
  for (size_t i = 0; i < test_cases.size(); i++) {
    SCOPED_TRACE(testing::Message() << "i(DE) = " << i);

    ServerFieldTypeSet matching_types;
    phone_number_de.GetMatchingTypes(ASCIIToUTF16(test_cases[i]), kLocaleDE,
                                     &matching_types);

    EXPECT_THAT(matching_types, testing::ElementsAre(PHONE_HOME_COUNTRY_CODE));
  }
}

// Tests that the |PHONE_HOME_COUNTRY_CODE| should not be added to the set of
// matching types.
TEST(PhoneNumberTest, CountryCodeNotInMatchingTypes) {
  AutofillProfile profile;
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  // Set phone number so country_code == 1, city_code = 650, number = 2345678.
  std::u16string phone(u"1 [650] 234-5678");
  PhoneNumber phone_number(&profile);
  phone_number.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), phone, kLocale);

  std::vector<const char*> test_cases = {
      "01", "+16502", "11", "211", "0001", "++1", "+1abc2", "001abc2", "01"};

  for (size_t i = 0; i < test_cases.size(); i++) {
    SCOPED_TRACE(testing::Message() << "i = " << i);

    ServerFieldTypeSet matching_types;
    phone_number.GetMatchingTypes(ASCIIToUTF16(test_cases[i]), kLocale,
                                  &matching_types);

    EXPECT_THAT(matching_types, testing::IsEmpty());
  }
}

}  // namespace autofill
