// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/phone_number.h"

#include <string>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

constexpr char kLocale[] = "en_US";

struct MatchingTypesTestCase {
  std::u16string input;
  FieldTypeSet expected_types;
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
  SCOPED_TRACE(::testing::Message() << "number: " << number);
  base::test::ScopedFeatureList trunk_types;
  trunk_types.InitWithFeatureState(
      features::kAutofillEnableSupportForPhoneNumberTrunkTypes,
      trunk_types_enabled);

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, country);
  PhoneNumber phone_number(&profile);
  // `kLocale` is irrelevant, because `profile` has country information.
  phone_number.SetInfo(PHONE_HOME_WHOLE_NUMBER, number, kLocale);
  for (const MatchingTypesTestCase& test : tests) {
    SCOPED_TRACE(::testing::Message() << "test.input: " << test.input);
    FieldTypeSet matching_types;
    phone_number.GetMatchingTypesWithProfileSources(test.input, kLocale,
                                                    &matching_types, nullptr);
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
  // Set phone number so city_code = 650, number = 2345678.
  MatchingTypesTest(
      u"650 234-5678", u"US",
      /*trunk_types_enabled=*/false,
      {{std::u16string(), {EMPTY_TYPE}},
       {u"1",
        base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
            ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
            : FieldTypeSet{}},
       {u"16", {}},
       {u"165", {}},
       {u"1650", {}},
       {u"16502", {}},
       {u"165023", {}},
       {u"1650234", {}},
       {u"16502345678",
        base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
            ? FieldTypeSet{PHONE_HOME_WHOLE_NUMBER}
            : FieldTypeSet{}},
       {u"650", {PHONE_HOME_CITY_CODE}},
       {u"2345678", {PHONE_HOME_NUMBER}},
       {u"234", {PHONE_HOME_NUMBER_PREFIX}},
       {u"5678", {PHONE_HOME_NUMBER_SUFFIX}},
       {u"2345", {}},
       {u"6502345678", {PHONE_HOME_CITY_AND_NUMBER}},
       {u"(650)2345678", {PHONE_HOME_CITY_AND_NUMBER}}});
}

TEST(PhoneNumberTest, Matcher_TrunkTypes) {
  // The following tests the same numbers as above but with trunk_types_enabled
  // = true;

  // Set phone number so country_code == 1, city_code = 650, number = 2345678.
  MatchingTypesTest(
      u"1 [650] 234-5678", u"US",
      /*trunk_types_enabled=*/true,
      {{u"1", {PHONE_HOME_COUNTRY_CODE}},
       {u"+1", {PHONE_HOME_COUNTRY_CODE}},
       {u"(+1) United States", {PHONE_HOME_COUNTRY_CODE}},
       {u"US (+1)", {PHONE_HOME_COUNTRY_CODE}},
       {u"16502345678", {PHONE_HOME_WHOLE_NUMBER}},
       // The following has a different expectation if
       // trunk_types_enabled = true
       {u"650", {PHONE_HOME_CITY_CODE, PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX}},
       {u"2345678", {PHONE_HOME_NUMBER}},
       {u"234", {PHONE_HOME_NUMBER_PREFIX}},
       {u"5678", {PHONE_HOME_NUMBER_SUFFIX}},
       // The following two have different expectations if
       // trunk_types_enabled = true.
       {u"6502345678",
        {PHONE_HOME_CITY_AND_NUMBER,
         PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}},
       {u"(650)2345678",
        {PHONE_HOME_CITY_AND_NUMBER,
         PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}}});
  // Set phone number so city_code = 650, number = 2345678.
  MatchingTypesTest(
      u"650 234-5678", u"US",
      /*trunk_types_enabled=*/true,
      {{u"1",
        base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
            ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
            : FieldTypeSet{}},
       {u"+1",
        base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
            ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
            : FieldTypeSet{}},
       {u"(+1) United States",
        base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
            ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
            : FieldTypeSet{}},
       {u"US (+1)",
        base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
            ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
            : FieldTypeSet{}},
       // The international number is not recognized.
       {u"16502345678",
        base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
            ? FieldTypeSet{PHONE_HOME_WHOLE_NUMBER}
            : FieldTypeSet{}},
       {u"650", {PHONE_HOME_CITY_CODE, PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX}},
       {u"2345678", {PHONE_HOME_NUMBER}},
       {u"234", {PHONE_HOME_NUMBER_PREFIX}},
       {u"5678", {PHONE_HOME_NUMBER_SUFFIX}},
       {u"6502345678",
        {PHONE_HOME_CITY_AND_NUMBER,
         PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}},
       {u"(650)2345678",
        {PHONE_HOME_CITY_AND_NUMBER,
         PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}}});
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

// Tests that the |PHONE_HOME_COUNTRY_CODE| should not be added to the set of
// matching types.
TEST(PhoneNumberTest, CountryCodeNotInMatchingTypes) {
  MatchingTypesTest(u"1 [650] 234-5678", u"US",
                    /*trunk_types_enabled=*/false,
                    {{u"01", {}},
                     {u"+16502", {}},
                     {u"11", {}},
                     {u"211", {}},
                     {u"0001", {}},
                     {u"++1", {}},
                     {u"+1abc2", {}},
                     {u"001abc2", {}},
                     {u"01", {}}});
}

// Verify that the derived types of German numbers are correct.
TEST(PhoneNumberTest, Matcher_DE) {
  // Crowdsourcing is broken. A number of cases are very surprising.
  MatchingTypesTest(u"+491741234567", u"DE",
                    /*trunk_types_enabled=*/false,
                    {
                        {u"49", {PHONE_HOME_COUNTRY_CODE}},
                        {u"+49", {PHONE_HOME_COUNTRY_CODE}},
                        {u"0049", {PHONE_HOME_COUNTRY_CODE}},
                        {u"(+49) DE", {PHONE_HOME_COUNTRY_CODE}},
                        {u"(0049) DE", {PHONE_HOME_COUNTRY_CODE}},
                        {u"174", {PHONE_HOME_CITY_CODE}},
                        // TODO(crbug.com/40220393): This should match
                        // PHONE_HOME_CITY_CODE!
                        {u"0174", {}},
                        {u"1234567", {PHONE_HOME_NUMBER}},
                        // TODO(crbug.com/40220393): This should match
                        // PHONE_HOME_CITY_AND_NUMBER!
                        {u"1741234567", {}},
                        {u"01741234567", {PHONE_HOME_CITY_AND_NUMBER}},
                        {u"(0174)1234567", {PHONE_HOME_CITY_AND_NUMBER}},
                        {u"0174 1234567", {PHONE_HOME_CITY_AND_NUMBER}},
                        {u"+491741234567", {PHONE_HOME_WHOLE_NUMBER}},
                        {u"00491741234567", {PHONE_HOME_WHOLE_NUMBER}},
                        {u"004901741234567", {PHONE_HOME_WHOLE_NUMBER}},
                    });
  MatchingTypesTest(
      u"0174 1234567", u"DE",
      /*trunk_types_enabled=*/false,
      {
          {u"49", base::FeatureList::IsEnabled(
                      features::kAutofillInferCountryCallingCode)
                      ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
                      : FieldTypeSet{}},
          {u"+49", base::FeatureList::IsEnabled(
                       features::kAutofillInferCountryCallingCode)
                       ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
                       : FieldTypeSet{}},
          {u"0049", base::FeatureList::IsEnabled(
                        features::kAutofillInferCountryCallingCode)
                        ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
                        : FieldTypeSet{}},
          {u"(+49) DE", base::FeatureList::IsEnabled(
                            features::kAutofillInferCountryCallingCode)
                            ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
                            : FieldTypeSet{}},
          {u"(0049) DE", base::FeatureList::IsEnabled(
                             features::kAutofillInferCountryCallingCode)
                             ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
                             : FieldTypeSet{}},
          {u"174", {PHONE_HOME_CITY_CODE}},
          {u"0174", {}},
          {u"1234567", {PHONE_HOME_NUMBER}},
          {u"01741234567", {PHONE_HOME_CITY_AND_NUMBER}},
          {u"(0174)1234567", {PHONE_HOME_CITY_AND_NUMBER}},
          {u"0174 1234567", {PHONE_HOME_CITY_AND_NUMBER}},
          // The international phone number is unknown.
          {u"+491741234567", base::FeatureList::IsEnabled(
                                 features::kAutofillInferCountryCallingCode)
                                 ? FieldTypeSet{PHONE_HOME_WHOLE_NUMBER}
                                 : FieldTypeSet{}},
          {u"00491741234567", base::FeatureList::IsEnabled(
                                  features::kAutofillInferCountryCallingCode)
                                  ? FieldTypeSet{PHONE_HOME_WHOLE_NUMBER}
                                  : FieldTypeSet{}},
          {u"004901741234567", base::FeatureList::IsEnabled(
                                   features::kAutofillInferCountryCallingCode)
                                   ? FieldTypeSet{PHONE_HOME_WHOLE_NUMBER}
                                   : FieldTypeSet{}},
      });
}

TEST(PhoneNumberTest, Matcher_TrunkTypes_DE) {
  MatchingTypesTest(
      u"+491741234567", u"DE",
      /*trunk_types_enabled=*/true,
      {
          {u"49", {PHONE_HOME_COUNTRY_CODE}},
          {u"+49", {PHONE_HOME_COUNTRY_CODE}},
          {u"0049", {PHONE_HOME_COUNTRY_CODE}},
          {u"174", {PHONE_HOME_CITY_CODE}},
          {u"0174", {PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX}},
          {u"1234567", {PHONE_HOME_NUMBER}},
          {u"1741234567", {PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}},
          {u"01741234567", {PHONE_HOME_CITY_AND_NUMBER}},
          {u"+491741234567", {PHONE_HOME_WHOLE_NUMBER}},
          {u"00491741234567", {PHONE_HOME_WHOLE_NUMBER}},
          {u"004901741234567", {PHONE_HOME_WHOLE_NUMBER}},
      });
  MatchingTypesTest(
      u"0174 1234567", u"DE",
      /*trunk_types_enabled=*/true,
      {
          {u"49", base::FeatureList::IsEnabled(
                      features::kAutofillInferCountryCallingCode)
                      ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
                      : FieldTypeSet{}},
          {u"+49", base::FeatureList::IsEnabled(
                       features::kAutofillInferCountryCallingCode)
                       ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
                       : FieldTypeSet{}},
          {u"0049", base::FeatureList::IsEnabled(
                        features::kAutofillInferCountryCallingCode)
                        ? FieldTypeSet{PHONE_HOME_COUNTRY_CODE}
                        : FieldTypeSet{}},
          {u"174", {PHONE_HOME_CITY_CODE}},
          {u"0174", {PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX}},
          {u"1234567", {PHONE_HOME_NUMBER}},
          {u"1741234567", {PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}},
          {u"01741234567", {PHONE_HOME_CITY_AND_NUMBER}},
          // The international phone number is unknown.
          {u"+491741234567", base::FeatureList::IsEnabled(
                                 features::kAutofillInferCountryCallingCode)
                                 ? FieldTypeSet{PHONE_HOME_WHOLE_NUMBER}
                                 : FieldTypeSet{}},
          {u"00491741234567", base::FeatureList::IsEnabled(
                                  features::kAutofillInferCountryCallingCode)
                                  ? FieldTypeSet{PHONE_HOME_WHOLE_NUMBER}
                                  : FieldTypeSet{}},
          {u"004901741234567", base::FeatureList::IsEnabled(
                                   features::kAutofillInferCountryCallingCode)
                                   ? FieldTypeSet{PHONE_HOME_WHOLE_NUMBER}
                                   : FieldTypeSet{}},
      });
}

// Verify that `PhoneNumber::SetInfo()` correctly formats the incoming number.
// `kLocale` is irrelevant, as `profile` has a country.
TEST(PhoneNumberTest, SetInfo) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

  PhoneNumber phone(&profile);
  EXPECT_TRUE(phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER).empty());
  EXPECT_TRUE(phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale).empty());

  // Set the formatted info directly.
  EXPECT_TRUE(
      phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"(650) 234-5678", kLocale));
  EXPECT_EQ(
      base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
          ? u"1 650-234-5678"
          : u"(650) 234-5678",
      phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(
      base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
          ? u"16502345678"
          : u"6502345678",
      phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));

  // Unformatted numbers should be formatted.
  EXPECT_TRUE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"8887776666", kLocale));
  EXPECT_EQ(
      base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
          ? u"1 888-777-6666"
          : u"(888) 777-6666",
      phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(
      base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
          ? u"18887776666"
          : u"8887776666",
      phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));

  EXPECT_TRUE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"+18887776666", kLocale));
  EXPECT_EQ(u"1 888-777-6666", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(u"18887776666", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));

  // Differently formatted numbers should be left formatted as is, unless
  // kAutofillPreferParsedPhoneNumbers is enabled.
  EXPECT_TRUE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"800-432-8765", kLocale));
  if (base::FeatureList::IsEnabled(
          features::kAutofillPreferParsedPhoneNumber)) {
    EXPECT_EQ(
        base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
            ? u"1 800-432-8765"
            : u"(800) 432-8765",
        phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  } else {
    EXPECT_EQ(u"800-432-8765", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  }
  EXPECT_EQ(
      base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
          ? u"18004328765"
          : u"8004328765",
      phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));

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
  EXPECT_EQ(
      base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
          ? u"+15141231234"
          : u"5141231234",
      phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));
  EXPECT_EQ(u"514", phone.GetInfo(PHONE_HOME_CITY_CODE, kLocale));
}

TEST(PhoneNumberTest, InferCountryCallingCode) {
  base::test::ScopedFeatureList complement_calling_code_enabled;
  complement_calling_code_enabled.InitWithFeatures(
      {features::kAutofillPreferParsedPhoneNumber,
       features::kAutofillInferCountryCallingCode},
      /*disabled_features=*/{});

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
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
  // In this case the calling code is inferred for the raw info and the filling
  // information.
  EXPECT_TRUE(
      phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"(650) 234-5678", kLocale));
  EXPECT_EQ(u"1", phone.GetInfo(PHONE_HOME_COUNTRY_CODE, kLocale));
  EXPECT_EQ(u"16502345678", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));
  EXPECT_EQ(u"1 650-234-5678", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
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
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

  PhoneNumber phone(&profile);
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"6502345678");
  EXPECT_EQ(u"650", phone.GetInfo(PHONE_HOME_CITY_CODE, "US"));

  // Update the area code.
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"8322345678");
  EXPECT_EQ(u"832", phone.GetInfo(PHONE_HOME_CITY_CODE, "US"));

  // Change the phone number to have a UK format, but try to parse with the
  // wrong locale.
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"07023456789");
  EXPECT_EQ(std::u16string(), phone.GetInfo(PHONE_HOME_CITY_CODE, "US"));

  // Now try parsing using the correct locale.  Note that the profile's country
  // code should override the app locale, which is still set to "US".
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"GB");
  phone.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"07023456789");
  EXPECT_EQ(u"70", phone.GetInfo(PHONE_HOME_CITY_CODE, "US"));
}

TEST(PhoneNumberTest, PhoneCombineHelper) {
  // PhoneCombineHelper is largely covered via PhoneImportAndGetTest. This
  // just tests some remaining edge cases:

  // Check that `SetInfo` fails for an address country (rather than a
  // phone country):
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  PhoneNumber::PhoneCombineHelper number1;

  // Ensure parsing is possible when falling back to detecting the country code
  // based on the app locale.
  std::u16string parsed_phone;
  PhoneNumber::PhoneCombineHelper number2;
  number2.SetInfo(PHONE_HOME_CITY_CODE, u"650");
  number2.SetInfo(PHONE_HOME_NUMBER_PREFIX, u"234");
  number2.SetInfo(PHONE_HOME_NUMBER_SUFFIX, u"5682");
  EXPECT_TRUE(number2.ParseNumber(
      // No country code is specified here:
      AutofillProfile(i18n_model_definition::kLegacyHierarchyCountryCode),
      kLocale, &parsed_phone));
  EXPECT_EQ(u"(650) 234-5682", parsed_phone);
}

TEST(PhoneNumberTest, HelperSetsAllPhoneFieldTypes) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  PhoneNumber phone_number(&profile);

  FieldTypeSet types;
  profile.GetSupportedTypes(&types);
  std::vector<FieldType> fields{types.begin(), types.end()};
  std::erase_if(fields, [](FieldType type) {
    return GroupTypeOfFieldType(type) != FieldTypeGroup::kPhone;
  });

  base::ranges::for_each(fields, [](FieldType type) {
    PhoneNumber::PhoneCombineHelper helper;
    helper.SetInfo(type, u"123");
  });
}

TEST(PhoneNumberTest, InternationalPhoneHomeCityAndNumber_US) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  // Set phone number so country_code == 1, city_code = 650, number = 2345678.
  std::u16string phone(u"+1 (650) 234-5678");
  PhoneNumber phone_number(&profile);
  phone_number.SetInfo(PHONE_HOME_WHOLE_NUMBER, phone, "en-US");
  EXPECT_EQ(u"6502345678",
            phone_number.GetInfo(PHONE_HOME_CITY_AND_NUMBER, "en-US"));
}

// This is a regression test for crbug.com/638795.
TEST(PhoneNumberTest, InternationalPhoneHomeCityAndNumber_DE) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");
  // Set phone number so country_code == 49, city_code = 174, number = 12 34
  // 567.
  std::u16string phone(u"+49 (174) 12 34 567");
  PhoneNumber phone_number(&profile);
  phone_number.SetInfo(PHONE_HOME_WHOLE_NUMBER, phone, "en-US");
  // Note that for German numbers (unlike US numbers), the
  // PHONE_HOME_CITY_AND_NUMBER should start with a 0.
  EXPECT_EQ(u"01741234567",
            phone_number.GetInfo(PHONE_HOME_CITY_AND_NUMBER, "en-US"));
}

TEST(PhoneNumberTest, TrunkPrefix) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);

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
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);

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
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  PhoneNumber phone(&profile);
  const std::string locale = "en-US";
  EXPECT_TRUE(phone.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"(650) 234-2345 ext. 234",
                            locale));
  EXPECT_EQ(u"(650) 234-2345", phone.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(u"6502342345", phone.GetInfo(PHONE_HOME_WHOLE_NUMBER, locale));
  EXPECT_TRUE(phone.GetInfo(PHONE_HOME_EXTENSION, locale).empty());
}

// The PhoneImportAndGetTest testcases simulate the steps of observing a phone
// number submission on a website (possibly spread over multiple fields) which
// is imported into an address profile, and retrieving the values that would be
// filled into a website.
// Importing a phone number actually happens in
// FormDataImporter::SetPhoneNumber, which is out of scope for this test but so
// small that we replicate it in the test.
struct PhoneImportAndGetTestCase {
  struct FieldTypeAndValue {
    FieldType field_type;
    std::u16string value;
  };

  // Field type / field value pairs that represent the values a user submits
  // on a form.
  std::vector<FieldTypeAndValue> observed_fields;
  // The default region assumed for parsing. In real executions this is derived
  // from the form, a GeoIP or the locale.
  std::u16string default_country;
  // The value that is persisted on disk, shown in save prompts and on
  // chrome://settings/addresses (the "raw value").
  std::u16string expected_stored_number;

  // The values we expect to get at filling time
  std::vector<FieldTypeAndValue> expected_values;
};

class PhoneImportAndGetTest
    : public ::testing::TestWithParam<PhoneImportAndGetTestCase> {};

TEST_P(PhoneImportAndGetTest, TestSettingAndParsing) {
  const PhoneImportAndGetTestCase& test = GetParam();

  SCOPED_TRACE(::testing::Message() << [&] {
    std::ostringstream result;
    result << "observed_fields:\n";
    for (const auto& [field_type, value] : test.observed_fields) {
      result << FieldTypeToStringView(field_type) << ": " << value << "\n";
    }
    result << "default_country: " << test.default_country << "\n";
    result << "expected_stored_number: " << test.expected_stored_number << "\n";
    return result.str();
  }());

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, test.default_country);

  // The locale currently plays no role in the test because the profile contains
  // a country. This just stays here to be as close as possible to actual user
  // experiences.
  std::string faked_app_locale;
  if (test.default_country == u"DE") {
    faked_app_locale = "de_DE";
  } else if (test.default_country == u"US") {
    faked_app_locale = "en_US";
  } else {
    ASSERT_TRUE(false);
  }

  // Gather all data in the PhoneCombineHelper.
  PhoneNumber::PhoneCombineHelper number;
  for (const auto& [field_type, value] : test.observed_fields) {
    number.SetInfo(field_type, value);
  }

  ASSERT_TRUE(PhoneNumber::ImportPhoneNumberToProfile(number, faked_app_locale,
                                                      profile));

  // Verify that the raw value stored is as expected.
  EXPECT_EQ(test.expected_stored_number,
            profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  // Verify the values that would be filled on webforms.
  for (const auto& [field_type, expected_value] : test.expected_values) {
    SCOPED_TRACE(::testing::Message()
                 << "GetInfo(" << FieldTypeToStringView(field_type) << ")");
    EXPECT_EQ(expected_value, profile.GetInfo(field_type, faked_app_locale));
  }
}

INSTANTIATE_TEST_SUITE_P(
    PhoneNumber,
    PhoneImportAndGetTest,
    testing::Values(
        // If a country code is set, the number is stored in international
        // format.
        PhoneImportAndGetTestCase{
            // The values a user submits on a form.
            .observed_fields = {{PHONE_HOME_COUNTRY_CODE, u"1"},
                                {PHONE_HOME_CITY_CODE, u"650"},
                                {PHONE_HOME_NUMBER, u"2345678"}},
            // The country of the address profile and also the locale used for
            // the test.
            .default_country = u"US",
            // What's stored on disk.
            .expected_stored_number = u"1 650-234-5678",
            // What's returned from GetInfo for filling.
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"1"},
                                {PHONE_HOME_CITY_CODE, u"650"},
                                {PHONE_HOME_NUMBER, u"2345678"},
                                // GetInfo strips special symbols and
                                // whitespaces.
                                {PHONE_HOME_WHOLE_NUMBER, u"16502345678"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"6502345678"}}},
        // If a number is given as city code and local number but does not have
        // a country code, the phone number is stored in national format. The
        // country code is not inferred, unless AutofillInferCountryCallingCode
        // is enabled.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_CODE, u"650"},
                                {PHONE_HOME_NUMBER, u"2345680"}},
            .default_country = u"US",
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillInferCountryCallingCode)
                    ? u"1 650-234-5680"
                    : u"(650) 234-5680",
            .expected_values = {{// No country code was set.
                                 PHONE_HOME_COUNTRY_CODE,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"1"
                                     : u""},
                                {PHONE_HOME_CITY_CODE, u"650"},
                                {PHONE_HOME_NUMBER, u"2345680"},
                                // Whole number is in national format because no
                                // country code was set.
                                {PHONE_HOME_WHOLE_NUMBER,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"16502345680"
                                     : u"6502345680"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"6502345680"}}},
        // If the city code is incorrect, the phone number cannot be interpreted
        // and is just stored as a sequence of digits.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_CODE, u"123"},
                                {PHONE_HOME_NUMBER, u"2345680"}},
            .default_country = u"US",
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillInferCountryCallingCode)
                    ? u"+1 1232345680"
                    : u"1232345680",
            .expected_values = {{// No country code was set.
                                 PHONE_HOME_COUNTRY_CODE,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"1"
                                     : u""},
                                {PHONE_HOME_CITY_CODE, u""},
                                // Because the areacode is invalid the entire
                                // number is interpreted as a local number and
                                // also returned for PHONE_HOME_WHOLE_NUMBER and
                                // PHONE_HOME_CITY_AND_NUMBER.
                                {PHONE_HOME_NUMBER, u"1232345680"},
                                {PHONE_HOME_WHOLE_NUMBER,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"+11232345680"
                                     : u"1232345680"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"1232345680"}}},
        // If the phone number is submitted as PHONE_HOME_CITY_AND_NUMBER,
        // the persisted internal value is a formatted national number, unless
        // AutofillInferCountryCallingCode is enabled.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_AND_NUMBER, u"6502345681"}},
            .default_country = u"US",
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillInferCountryCallingCode)
                    ? u"1 650-234-5681"
                    : u"(650) 234-5681",
            // .expected_values were already covered for this format above.
        },

        // If the phone number is submitted as city code, number and prefix,
        // the persisted internal value is a formatted national number.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_CODE, u"650"},
                                {PHONE_HOME_NUMBER_PREFIX, u"234"},
                                {PHONE_HOME_NUMBER_SUFFIX, u"5682"}},
            .default_country = u"US",
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillInferCountryCallingCode)
                    ? u"1 650-234-5682"
                    : u"(650) 234-5682",
            // .expected_values were already covered for this format above.
        },

        // If the phone number is submitted as a PHONE_HOME_CITY_AND_NUMBER in
        // international format (with or without a +), it is stored as a
        // formatted international number.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_AND_NUMBER, u"+16502345681"}},
            .default_country = u"US",
            .expected_stored_number = u"1 650-234-5681",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"1"},
                                {PHONE_HOME_CITY_CODE, u"650"},
                                {PHONE_HOME_NUMBER, u"2345681"},
                                {PHONE_HOME_WHOLE_NUMBER, u"16502345681"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"6502345681"}}},
        PhoneImportAndGetTestCase{
            // Same number as above but without a leading +.
            .observed_fields = {{PHONE_HOME_CITY_AND_NUMBER, u"16502345681"}},
            .default_country = u"US",
            .expected_stored_number = u"1 650-234-5681",
            // Same expectations as above.
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"1"},
                                {PHONE_HOME_CITY_CODE, u"650"},
                                {PHONE_HOME_NUMBER, u"2345681"},
                                {PHONE_HOME_WHOLE_NUMBER, u"16502345681"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"6502345681"}}},

        // If the phone number is submitted as a PHONE_HOME_WHOLE_NUMBER in
        // international format (with or without a +) and the user DID NOT
        // add formatting characters (space, parentheses), the values are
        // formatted.
        // The values filled on websites are the same as if the number was
        // stored as a PHONE_HOME_CITY_AND_NUMBER.
        PhoneImportAndGetTestCase{
            // This time using PHONE_HOME_WHOLE_NUMBER:
            .observed_fields = {{PHONE_HOME_WHOLE_NUMBER, u"+16502345681"}},
            .default_country = u"US",
            // The + from the input is reflected here:
            .expected_stored_number = u"1 650-234-5681",
            // Same expectations as above.
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"1"},
                                {PHONE_HOME_CITY_CODE, u"650"},
                                {PHONE_HOME_NUMBER, u"2345681"},
                                {PHONE_HOME_WHOLE_NUMBER, u"16502345681"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"6502345681"}}},
        PhoneImportAndGetTestCase{
            // Same number as above but without a leading +.
            .observed_fields = {{PHONE_HOME_WHOLE_NUMBER, u"16502345681"}},
            .default_country = u"US",
            // The lack of a + is refelected here:
            .expected_stored_number = u"1 650-234-5681",
            // Same expectations as above.
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"1"},
                                {PHONE_HOME_CITY_CODE, u"650"},
                                {PHONE_HOME_NUMBER, u"2345681"},
                                {PHONE_HOME_WHOLE_NUMBER, u"16502345681"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"6502345681"}}},

        // If, however, the user has provided formatting, this is preserved for
        // PHONE_HOME_WHOLE_NUMBER. Note the broken whitespacing in the number.
        // This is changed however when AutofillPreferParsedPhoneNumber is
        // enabled.
        PhoneImportAndGetTestCase{
            // This time using PHONE_HOME_WHOLE_NUMBER:
            .observed_fields = {{PHONE_HOME_WHOLE_NUMBER, u"+1 65 02 345681"}},
            .default_country = u"US",
            // The + from the input is reflected here:
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillPreferParsedPhoneNumber)
                    ? u"1 650-234-5681"
                    : u"+1 65 02 345681",
            // Same expectations as above.
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"1"},
                                {PHONE_HOME_CITY_CODE, u"650"},
                                {PHONE_HOME_NUMBER, u"2345681"},
                                {PHONE_HOME_WHOLE_NUMBER, u"16502345681"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"6502345681"}}},
        PhoneImportAndGetTestCase{
            // Same number as above but without a leading +.
            .observed_fields = {{PHONE_HOME_WHOLE_NUMBER, u"1 65 02 345681"}},
            .default_country = u"US",
            // The lack of a + is refelected here:
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillPreferParsedPhoneNumber)
                    ? u"1 650-234-5681"
                    : u"1 65 02 345681",
            // Same expectations as above.
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"1"},
                                {PHONE_HOME_CITY_CODE, u"650"},
                                {PHONE_HOME_NUMBER, u"2345681"},
                                {PHONE_HOME_WHOLE_NUMBER, u"16502345681"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"6502345681"}}},

        // The following tests document the behavior on a field that is
        // classified as a PHONE_HOME_CITY_AND_NUMBER field.

        // Phone number is correctly filled in national format -> data is
        // stored in national format. Whitespaces in the input are ignored.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_AND_NUMBER, u"089 12 34 567"}},
            .default_country = u"DE",
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillInferCountryCallingCode)
                    ? u"+49 89 1234567"
                    : u"089 1234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"49"
                                     : u""},
                                {PHONE_HOME_CITY_CODE, u"89"},
                                {PHONE_HOME_NUMBER, u"1234567"},
                                {PHONE_HOME_WHOLE_NUMBER,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"49891234567"
                                     : u"0891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}},
        // While we expected a PHONE_HOME_CITY_AND_NUMBER, the user entered an
        // international format (+49). The number is interpreted and stored in
        // international format.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_AND_NUMBER, u"+49891234567"}},
            .default_country = u"DE",
            .expected_stored_number = u"+49 89 1234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"49"},
                                {PHONE_HOME_CITY_CODE, u"89"},
                                {PHONE_HOME_NUMBER, u"1234567"},
                                // In Germany international numbers start with +
                                // (different from US where the + is dropped).
                                {PHONE_HOME_WHOLE_NUMBER, u"+49891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}},
        // While we expected a PHONE_HOME_CITY_AND_NUMBER, the user entered an
        // international format (+49) and made the mistake of not skipping the
        // 0. The number is interpreted and stored in international format.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_AND_NUMBER, u"+490891234567"}},
            .default_country = u"DE",
            .expected_stored_number = u"+49 89 1234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"49"},
                                {PHONE_HOME_CITY_CODE, u"89"},
                                {PHONE_HOME_NUMBER, u"1234567"},
                                // In Germany international numbers start with +
                                // (different from US where the + is dropped).
                                {PHONE_HOME_WHOLE_NUMBER, u"+49891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}},
        // Phone number is filled in international format with 00 CC -> number
        // is stored in international format.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_AND_NUMBER, u"0049891234567"}},
            .default_country = u"DE",
            .expected_stored_number = u"+49 89 1234567",
            // .expected_values were already covered for this format above.
        },
        // Phone number is filled in international format but the user did not
        // enter the + or 00 (e.g. because the website put the + before the
        // input field). -> The number is still stored in international format.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_AND_NUMBER, u"49891234567"}},
            .default_country = u"DE",
            .expected_stored_number = u"+49 89 1234567",
            // .expected_values were already covered for this format above.
        },
        // Phone number is filled in national format but the user did not enter
        // the leading 0 (e.g. because the field was preceded by a country code
        // selector that was not recognized). -> The number is still interpreted
        // correctly and stored in national format.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_AND_NUMBER, u"891234567"}},
            .default_country = u"DE",
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillInferCountryCallingCode)
                    ? u"+49 89 1234567"
                    : u"089 1234567",
            // .expected_values were already covered for this format above.
        },

        // The following tests document the behavior on a field that is
        // classified as a PHONE_HOME_WHOLE_NUMBER field (the input values are
        // the same as above). Note how in all cases the raw input value is
        // stored on disk. We still do a good job at guessing the correct values
        // at filling because the number is parsed at filling time.

        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_WHOLE_NUMBER, u"0891234567"}},
            .default_country = u"DE",
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillInferCountryCallingCode)
                    ? u"+49 89 1234567"
                    : u"089 1234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"49"
                                     : u""},
                                {PHONE_HOME_CITY_CODE, u"89"},
                                {PHONE_HOME_NUMBER, u"1234567"},
                                {PHONE_HOME_WHOLE_NUMBER,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"+49891234567"
                                     : u"0891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}},
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_WHOLE_NUMBER, u"+49891234567"}},
            .default_country = u"DE",
            .expected_stored_number = u"+49 89 1234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"49"},
                                {PHONE_HOME_CITY_CODE, u"89"},
                                {PHONE_HOME_NUMBER, u"1234567"},
                                {PHONE_HOME_WHOLE_NUMBER, u"+49891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}},
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_WHOLE_NUMBER, u"+490891234567"}},
            .default_country = u"DE",

            .expected_stored_number = u"+49 89 1234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"49"},
                                {PHONE_HOME_CITY_CODE, u"89"},
                                {PHONE_HOME_NUMBER, u"1234567"},
                                {PHONE_HOME_WHOLE_NUMBER, u"+49891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}},
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_WHOLE_NUMBER, u"0049891234567"}},
            .default_country = u"DE",
            .expected_stored_number = u"+49 89 1234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"49"},
                                {PHONE_HOME_CITY_CODE, u"89"},
                                {PHONE_HOME_NUMBER, u"1234567"},
                                {PHONE_HOME_WHOLE_NUMBER, u"+49891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}},
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_WHOLE_NUMBER, u"49891234567"}},
            .default_country = u"DE",
            .expected_stored_number = u"+49 89 1234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"49"},
                                {PHONE_HOME_CITY_CODE, u"89"},
                                {PHONE_HOME_NUMBER, u"1234567"},
                                {PHONE_HOME_WHOLE_NUMBER, u"+49891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}},
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_WHOLE_NUMBER, u"891234567"}},
            .default_country = u"DE",
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillInferCountryCallingCode)
                    ? u"+49 89 1234567"
                    : u"089 1234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"49"
                                     : u""},
                                {PHONE_HOME_CITY_CODE, u"89"},
                                {PHONE_HOME_NUMBER, u"1234567"},
                                {PHONE_HOME_WHOLE_NUMBER,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"+49891234567"
                                     : u"0891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}},

        // What happens in case a US profile has a DE number?

        // For a proper international phone number, everything works as
        // expected.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_AND_NUMBER, u"+49891234567"}},
            .default_country = u"US",
            .expected_stored_number = u"+49 89 1234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE, u"49"},
                                {PHONE_HOME_CITY_CODE, u"89"},
                                {PHONE_HOME_NUMBER, u"1234567"},
                                // Unlike an international US number, this one
                                // is prefixed by a +.
                                {PHONE_HOME_WHOLE_NUMBER, u"+49891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}},

        // The German national number "08912345678" entered in a
        // PHONE_HOME_CITY_AND_NUMBER or PHONE_HOME_WHOLE_NUMBER field would not
        // be stored because it consists of 11 digits, more than US phone
        // numbers allow. We don't have a test case because the number does not
        // get stored.

        // The national German number "0891234567" (10 digits) is also invalid
        // in the US, but can still be reproduced as a PHONE_HOME_NUMBER,
        // PHONE_HOME_WHOLE_NUMBER and PHONE_HOME_CITY_AND_NUMBER.
        // PHONE_HOME_CITY_CODE cannot be returned.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}},
            .default_country = u"US",
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillInferCountryCallingCode)
                    ? u"+1 0891234567"
                    : u"0891234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"1"
                                     : u""},
                                {PHONE_HOME_CITY_CODE, u""},
                                {PHONE_HOME_NUMBER, u"0891234567"},
                                {PHONE_HOME_WHOLE_NUMBER,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"+10891234567"
                                     : u"0891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}},

        // If an invalid 10 digit number is stored from a
        // PHONE_HOME_WHOLE_NUMBER, it behaves like a
        // PHONE_HOME_CITY_AND_NUMBER.
        PhoneImportAndGetTestCase{
            .observed_fields = {{PHONE_HOME_WHOLE_NUMBER, u"0891234567"}},
            .default_country = u"US",
            .expected_stored_number =
                base::FeatureList::IsEnabled(
                    features::kAutofillInferCountryCallingCode)
                    ? u"+1 0891234567"
                    : u"0891234567",
            .expected_values = {{PHONE_HOME_COUNTRY_CODE,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"1"
                                     : u""},
                                {PHONE_HOME_CITY_CODE, u""},
                                {PHONE_HOME_NUMBER, u"0891234567"},
                                {PHONE_HOME_WHOLE_NUMBER,
                                 base::FeatureList::IsEnabled(
                                     features::kAutofillInferCountryCallingCode)
                                     ? u"+10891234567"
                                     : u"0891234567"},
                                {PHONE_HOME_CITY_AND_NUMBER, u"0891234567"}}}));

}  // namespace autofill
