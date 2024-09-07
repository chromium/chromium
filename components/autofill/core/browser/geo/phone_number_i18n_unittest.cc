// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/phone_number_i18n.h"

#include <stddef.h>

#include <string>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libphonenumber/phonenumber_api.h"

namespace autofill {
namespace {

using i18n::ConstructPhoneNumber;
using i18n::NormalizePhoneNumber;
using i18n::ParsePhoneNumber;
using i18n::PhoneNumbersMatch;

TEST(PhoneNumberI18NTest, NormalizePhoneNumber) {
  // "Large" digits; these are not ASCII.
  std::u16string phone1(u"１６５０７４９８３２３");
  EXPECT_EQ(NormalizePhoneNumber(phone1, "US"), u"16507498323");

  // Devanagari script digits.
  std::u16string phone2(u"١٦٥٠٨٣٢٣٧٤٩");
  EXPECT_EQ(NormalizePhoneNumber(phone2, "US"), u"16508323749");

  std::u16string phone3(u"16503334２5٥");
  EXPECT_EQ(NormalizePhoneNumber(phone3, "US"), u"16503334255");

  std::u16string phone4(u"+1(650)2346789");
  EXPECT_EQ(NormalizePhoneNumber(phone4, "US"), u"16502346789");

  std::u16string phone5(u"6502346789");
  EXPECT_EQ(NormalizePhoneNumber(phone5, "US"), u"6502346789");
}

struct ParseNumberTestCase {
  // Expected parsing result.
  bool isPossibleNumber;
  // Inputs.
  std::u16string input;
  std::string assumed_region;
  // Further expectations.
  std::u16string number;
  std::u16string city_code;
  std::u16string country_code;
  std::string deduced_region;
};

// Returns a string which is too long to be considered a phone number.
std::u16string GenerateTooLongString() {
  return std::u16string(i18n::kMaxPhoneNumberSize + 1, u'7');
}

class ParseNumberTest : public testing::TestWithParam<ParseNumberTestCase> {};

TEST_P(ParseNumberTest, ParsePhoneNumber) {
  auto test_case = GetParam();
  SCOPED_TRACE(base::UTF16ToUTF8(test_case.input));

  std::u16string country_code, city_code, number;
  std::string deduced_region;
  ::i18n::phonenumbers::PhoneNumber unused_i18n_number;
  EXPECT_EQ(test_case.isPossibleNumber,
            ParsePhoneNumber(test_case.input, test_case.assumed_region,
                             &country_code, &city_code, &number,
                             &deduced_region, &unused_i18n_number));
  EXPECT_EQ(test_case.number, number);
  EXPECT_EQ(test_case.city_code, city_code);
  EXPECT_EQ(test_case.country_code, country_code);
  EXPECT_EQ(test_case.deduced_region, deduced_region);
}

INSTANTIATE_TEST_SUITE_P(
    PhoneNumberI18NTest,
    ParseNumberTest,
    testing::Values(
        // Test for empty string.  Should give back empty strings.
        ParseNumberTestCase{false, u"", "US"},
        // Test for string with less than 7 digits.  Should give back empty
        // strings.
        ParseNumberTestCase{false, u"1234", "US"},
        // Too long strings should not be parsed.
        ParseNumberTestCase{false, GenerateTooLongString(), "US"},
        // Test for string with exactly 7 digits. It is too short.
        // Should fail parsing in US.
        ParseNumberTestCase{false, u"17134567", "US"},
        // Does not have area code, but still a possible number with
        // unknown("ZZ") deduced region.
        ParseNumberTestCase{true, u"7134567", "US", u"7134567", u"", u"", "ZZ"},
        // Valid Canadian toll-free number.
        ParseNumberTestCase{true, u"3101234", "CA", u"1234", u"310", u"", "CA"},
        // Test for string with greater than 7 digits but less than 10 digits.
        // Should fail parsing in US.
        ParseNumberTestCase{false, u"123456789", "US"},
        // Test for string with greater than 7 digits but less than 10 digits
        // and
        // separators.
        // Should fail parsing in US.
        ParseNumberTestCase{false, u"12.345-6789", "US"},
        // Non-printable ASCII.
        ParseNumberTestCase{false, u"123", "US"},
        ParseNumberTestCase{false, u"123\u007f567", "US"},
        // Unicode noncharacters.
        ParseNumberTestCase{false, u"1\ufdef23", "US"},
        // Invalid UTF16.
        ParseNumberTestCase{false, u"1\xdfff", "US"},
        // Test for string with exactly 10 digits.
        // Should give back phone number and city code.
        // This one has an incorrect area code but could still be a possible
        // number with unknown("ZZ") deducted region.
        ParseNumberTestCase{true, u"1234567890", "US", u"1234567890", u"", u"",
                            "ZZ"},
        // This is actually not a valid number because the first number after
        // area code is 1. But it's still a possible number, just with deduced
        // country set to unknown("ZZ").
        ParseNumberTestCase{true, u"6501567890", "US", u"1567890", u"650", u"",
                            "ZZ"},
        ParseNumberTestCase{true, u"6504567890", "US", u"4567890", u"650", u"",
                            "US"},
        // Test for string with exactly 10 digits and separators.
        // Should give back phone number and city code.
        ParseNumberTestCase{true, u"(650) 456-7890", "US", u"4567890", u"650",
                            u"", "US"},
        // Tests for string with over 10 digits.
        // 01 is incorrect prefix in the USA, we interpret 011 as prefix, and
        // rest is parsed as a Singapore number(country code "SG").
        ParseNumberTestCase{true, u"0116504567890", "US", u"04567890", u"",
                            u"65", "SG"},
        // 011 is a correct "dial out" prefix in the USA - the parsing should
        // succeed.
        ParseNumberTestCase{true, u"01116504567890", "US", u"4567890", u"650",
                            u"1", "US"},
        // 011 is a correct "dial out" prefix in the USA but the rest of the
        // number
        // can't parse as a US number.
        ParseNumberTestCase{true, u"01178124567890", "US", u"4567890", u"812",
                            u"7", "RU"},
        // Test for string with over 10 digits with separator characters.
        // Should give back phone number, city code, and country code. "011" is
        // US "dial out" code, which is discarded.
        ParseNumberTestCase{true, u"(0111) 650-456.7890", "US", u"4567890",
                            u"650", u"1", "US"},
        // Now try phone from Czech republic - it has 00 dial out code, 420
        // country
        // code and variable length area codes.
        ParseNumberTestCase{true, u"+420 27-89.10.112", "US", u"910112", u"278",
                            u"420", "CZ"},
        ParseNumberTestCase{false, u"27-89.10.112", "US"},
        ParseNumberTestCase{true, u"27-89.10.112", "CZ", u"910112", u"278", u"",
                            "CZ"},
        ParseNumberTestCase{false, u"420 57-89.10.112", "US"},
        ParseNumberTestCase{true, u"420 57-89.10.112", "CZ", u"910112", u"578",
                            u"420", "CZ"},
        // Parses vanity numbers.
        ParseNumberTestCase{true, u"1-650-FLOWERS", "US", u"3569377", u"650",
                            u"1", "US"},
        // 800 is not an area code, but the destination code. In our library
        // these
        // codes should be treated the same as area codes.
        ParseNumberTestCase{true, u"1-800-FLOWERS", "US", u"3569377", u"800",
                            u"1", "US"},
        // Don't add a country code where there was none.
        ParseNumberTestCase{true, u"(08) 450 777 7777", "DE", u"7777777",
                            u"8450", u"", "DE"}));

TEST(PhoneNumberI18NTest, ConstructPhoneNumber) {
  std::u16string number;
  EXPECT_TRUE(ConstructPhoneNumber(u"16502345678", "US", &number));
  EXPECT_EQ(u"1 650-234-5678", number);
  EXPECT_TRUE(ConstructPhoneNumber(u"6502345678", "US", &number));
  EXPECT_EQ(u"(650) 234-5678", number);

  // Invalid number, too long.
  EXPECT_FALSE(ConstructPhoneNumber(u"650234567890", "US", &number));
  EXPECT_EQ(std::u16string(), number);
  // Italian number
  EXPECT_TRUE(ConstructPhoneNumber(base::StrCat({u"39", u"347", u"2345678"}),
                                   "IT", &number));
  EXPECT_EQ(u"+39 347 234 5678", number);
  EXPECT_TRUE(ConstructPhoneNumber(u"39 347 2345678", "IT", &number));
  EXPECT_EQ(u"+39 347 234 5678", number);
  EXPECT_TRUE(
      ConstructPhoneNumber(base::StrCat({u"347", u"2345678"}), "IT", &number));
  EXPECT_EQ(u"347 234 5678", number);
  // German number.
  // Not a strictly correct number, because the zero trunk prefix in 024 does
  // not belong there.
  EXPECT_TRUE(ConstructPhoneNumber(base::StrCat({u"49", u"024", u"2345678901"}),
                                   "DE", &number));
  EXPECT_EQ(u"+49 2423 45678901", number);
  EXPECT_TRUE(ConstructPhoneNumber(base::StrCat({u"024", u"2345678901"}), "DE",
                                   &number));
  EXPECT_EQ(u"02423 45678901", number);
}

TEST(PhoneNumberI18NTest, PhoneNumbersMatch) {
  // Same numbers, defined country code.
  EXPECT_TRUE(PhoneNumbersMatch(u"4158889999", u"4158889999", "US", "en-US"));
  // Same numbers, undefined country code.
  EXPECT_TRUE(
      PhoneNumbersMatch(u"4158889999", u"4158889999", std::string(), "en-US"));

  // Numbers differ by country code only.
  EXPECT_TRUE(PhoneNumbersMatch(u"14158889999", u"4158889999", "US", "en-US"));

  // Same numbers, different formats.
  EXPECT_TRUE(PhoneNumbersMatch(u"4158889999", u"415-888-9999", "US", "en-US"));
  EXPECT_TRUE(
      PhoneNumbersMatch(u"4158889999", u"(415)888-9999", "US", "en-US"));
  EXPECT_TRUE(PhoneNumbersMatch(u"4158889999", u"415 888 9999", "US", "en-US"));
  EXPECT_TRUE(PhoneNumbersMatch(u"4158889999", u"415 TUV WXYZ", "US", "en-US"));
  EXPECT_TRUE(
      PhoneNumbersMatch(u"1(415)888-99-99", u"+14158889999", "US", "en-US"));

  // Partial matches don't count.
  EXPECT_FALSE(PhoneNumbersMatch(u"14158889999", u"8889999", "US", "en-US"));

  // Different numbers don't match.
  EXPECT_FALSE(PhoneNumbersMatch(u"14158889999", u"1415888", "US", "en-US"));

  // Two empty numbers match.
  EXPECT_TRUE(
      PhoneNumbersMatch(std::u16string(), std::u16string(), "US", "en-US"));

  // An empty and a non-empty number do not match.
  EXPECT_FALSE(
      PhoneNumbersMatch(std::u16string(), u"5088585123", "US", "en-US"));
}

// Tests that the phone numbers are correctly formatted for the Payment
// Response.
TEST(PhoneNumberUtilTest, FormatPhoneForResponse) {
  EXPECT_EQ("+15152231234",
            i18n::FormatPhoneForResponse("(515) 223-1234", "US"));
  EXPECT_EQ("+15152231234",
            i18n::FormatPhoneForResponse("(1) 515-223-1234", "US"));
  EXPECT_EQ("+33142685300",
            i18n::FormatPhoneForResponse("1 42 68 53 00", "FR"));

  // Invalid numbers are not formatted.
  EXPECT_EQ("(515) 123-1234",
            i18n::FormatPhoneForResponse("(515) 123-1234", "US"));
  EXPECT_EQ("(1) 515-123-1234",
            i18n::FormatPhoneForResponse("(1) 515-123-1234", "US"));
}

// Tests that phone numbers are correctly formatted in a national format.
TEST(PhoneNumberUtilTest, FormatPhoneNationallyForDisplay) {
  // Invalid US and Brazilian numbers are not formatted.
  EXPECT_EQ("1234567890",
            i18n::FormatPhoneNationallyForDisplay("1234567890", "US"));
  EXPECT_EQ("(11) 13333-4444",
            i18n::FormatPhoneNationallyForDisplay("(11) 13333-4444", "BR"));
  EXPECT_EQ("(11) 13333-4444",
            i18n::FormatPhoneNationallyForDisplay("(11) 13333-4444", "IN"));

  // Valid US, Canadian, UK, and Brazilian numbers are nationally formatted.
  EXPECT_EQ("(202) 444-0000",
            i18n::FormatPhoneNationallyForDisplay("2024440000", "US"));
  EXPECT_EQ("(202) 444-0000",
            i18n::FormatPhoneNationallyForDisplay("+1(202)4440000", "US"));
  EXPECT_EQ("(202) 444-0000",
            i18n::FormatPhoneNationallyForDisplay("12024440000", "US"));
  EXPECT_EQ("(202) 444-0000",
            i18n::FormatPhoneNationallyForDisplay("(202)4440000", "US"));
  EXPECT_EQ("(202) 444-0000",
            i18n::FormatPhoneNationallyForDisplay("202-444-0000", "US"));
  EXPECT_EQ("(819) 555-9999",
            i18n::FormatPhoneNationallyForDisplay("+1(819)555 9999", "CA"));
  EXPECT_EQ("(819) 555-9999",
            i18n::FormatPhoneNationallyForDisplay("18195559999", "CA"));
  EXPECT_EQ("020 7601 4444",
            i18n::FormatPhoneNationallyForDisplay("+4402076014444", "UK"));
  EXPECT_EQ("(21) 3883-5600",
            i18n::FormatPhoneNationallyForDisplay("2138835600", "BR"));
}

// Tests that the phone numbers are correctly formatted to display to the user.
TEST(PhoneNumberUtilTest, FormatPhoneForDisplay) {
  // Invalid number is not formatted.
  EXPECT_EQ("5151231234", i18n::FormatPhoneForDisplay("5151231234", "US"));
  // Valid number is formatted.
  EXPECT_EQ("+1 515-223-1234", i18n::FormatPhoneForDisplay("5152231234", "US"));
  EXPECT_EQ("+33 1 42 68 53 00",
            i18n::FormatPhoneForDisplay("142685300", "FR"));
}

// Test for the GetFormattedPhoneNumberForDisplay method.
struct PhoneNumberFormatCase {
  PhoneNumberFormatCase(const char16_t* phone,
                        const char16_t* country,
                        const char16_t* expected_format,
                        const char* locale = "")
      : phone(phone),
        country(country),
        expected_format(expected_format),
        locale(locale) {}

  const char16_t* phone;
  const char16_t* country;
  const char16_t* expected_format;
  const char* locale;
};

class GetFormattedPhoneNumberForDisplayTest
    : public testing::TestWithParam<PhoneNumberFormatCase> {};

TEST_P(GetFormattedPhoneNumberForDisplayTest,
       GetFormattedPhoneNumberForDisplay) {
  AutofillProfile profile(
      AddressCountryCode(base::UTF16ToUTF8(GetParam().country)));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, GetParam().phone);
  EXPECT_EQ(GetParam().expected_format, i18n::GetFormattedPhoneNumberForDisplay(
                                            profile, GetParam().locale));
}

INSTANTIATE_TEST_SUITE_P(
    GetFormattedPhoneNumberForDisplay,
    GetFormattedPhoneNumberForDisplayTest,
    testing::Values(
        //////////////////////////
        // US phone in US.
        //////////////////////////
        // Formatted phone numbers.
        PhoneNumberFormatCase(u"+1 415-555-5555", u"US", u"+1 415-555-5555"),
        PhoneNumberFormatCase(u"1 415-555-5555", u"US", u"+1 415-555-5555"),
        PhoneNumberFormatCase(u"415-555-5555", u"US", u"+1 415-555-5555"),
        // Raw phone numbers.
        PhoneNumberFormatCase(u"+14155555555", u"US", u"+1 415-555-5555"),
        PhoneNumberFormatCase(u"14155555555", u"US", u"+1 415-555-5555"),
        PhoneNumberFormatCase(u"4155555555", u"US", u"+1 415-555-5555"),

        //////////////////////////
        // US phone in CA.
        //////////////////////////
        // Formatted phone numbers.
        PhoneNumberFormatCase(u"+1 415-555-5555", u"CA", u"+1 415-555-5555"),
        PhoneNumberFormatCase(u"1 415-555-5555", u"CA", u"+1 415-555-5555"),
        PhoneNumberFormatCase(u"415-555-5555", u"CA", u"+1 415-555-5555"),
        // Raw phone numbers.
        PhoneNumberFormatCase(u"+14155555555", u"CA", u"+1 415-555-5555"),
        PhoneNumberFormatCase(u"14155555555", u"CA", u"+1 415-555-5555"),
        PhoneNumberFormatCase(u"4155555555", u"CA", u"+1 415-555-5555"),

        //////////////////////////
        // US phone in AU.
        //////////////////////////
        // A US phone with the country code is correctly formatted as an US
        // number.
        PhoneNumberFormatCase(u"+1 415-555-5555", u"AU", u"+1 415-555-5555"),
        PhoneNumberFormatCase(u"1 415-555-5555", u"AU", u"+1 415-555-5555"),
        // Without a country code, the phone is formatted for the profile's
        // country, if it's valid.
        PhoneNumberFormatCase(u"2 9374 4000", u"AU", u"+61 2 9374 4000"),
        // Without a country code, formatting returns the number as entered by
        // user, if it's invalid, unless AutofillInferCountryCallingCode is
        // enabled.
        PhoneNumberFormatCase(u"415-555-5555",
                              u"AU",
                              base::FeatureList::IsEnabled(
                                  features::kAutofillInferCountryCallingCode)
                                  ? u"+614155555555"
                                  : u"4155555555"),

        //////////////////////////
        // US phone in MX.
        //////////////////////////
        // A US phone with the country code is correctly formatted as an US
        // number.
        PhoneNumberFormatCase(u"+1 415-555-5555", u"MX", u"+1 415-555-5555"),
        // "+52 415 555 5555" is a valid number for Mexico,
        PhoneNumberFormatCase(u"1 415-555-5555", u"MX", u"+52 415 555 5555"),
        // Without a country code, the phone is formatted for the profile's
        // country.
        PhoneNumberFormatCase(u"415-555-5555", u"MX", u"+52 415 555 5555"),

        //////////////////////////
        // AU phone in AU.
        //////////////////////////
        // Formatted phone numbers.
        PhoneNumberFormatCase(u"+61 2 9374 4000", u"AU", u"+61 2 9374 4000"),
        PhoneNumberFormatCase(u"61 2 9374 4000", u"AU", u"+61 2 9374 4000"),
        PhoneNumberFormatCase(u"02 9374 4000", u"AU", u"+61 2 9374 4000"),
        PhoneNumberFormatCase(u"2 9374 4000", u"AU", u"+61 2 9374 4000"),
        // Raw phone numbers.
        PhoneNumberFormatCase(u"+61293744000", u"AU", u"+61 2 9374 4000"),
        PhoneNumberFormatCase(u"61293744000", u"AU", u"+61 2 9374 4000"),
        PhoneNumberFormatCase(u"0293744000", u"AU", u"+61 2 9374 4000"),
        PhoneNumberFormatCase(u"293744000", u"AU", u"+61 2 9374 4000"),

        //////////////////////////
        // AU phone in US.
        //////////////////////////
        // An AU phone with the country code is correctly formatted as an AU
        // number.
        PhoneNumberFormatCase(u"+61 2 9374 4000", u"US", u"+61 2 9374 4000"),
        PhoneNumberFormatCase(u"61 2 9374 4000", u"US", u"+61 2 9374 4000"),
        // Without a country code, the phone is formatted for the profile's
        // country.
        // This local AU number is associated with US profile, the number is
        // not a valid US number, therefore formatting will just return what
        // user entered, unless AutofillInferCountryCallingCode is.
        PhoneNumberFormatCase(u"02 9374 4000",
                              u"US",
                              base::FeatureList::IsEnabled(
                                  features::kAutofillInferCountryCallingCode)
                                  ? u"+10293744000"
                                  : u"0293744000"),
        // This local GR(Greece) number is formatted as an US number, if it's
        // valid US number.
        PhoneNumberFormatCase(u"22 6800 0090", u"US", u"+1 226-800-0090"),

        //////////////////////////
        // MX phone in MX.
        //////////////////////////
        // Formatted phone numbers.
        PhoneNumberFormatCase(u"+52 55 5342 8400", u"MX", u"+52 55 5342 8400"),
        PhoneNumberFormatCase(u"52 55 5342 8400", u"MX", u"+52 55 5342 8400"),
        PhoneNumberFormatCase(u"55 5342 8400", u"MX", u"+52 55 5342 8400"),
        // Raw phone numbers.
        PhoneNumberFormatCase(u"+525553428400", u"MX", u"+52 55 5342 8400"),
        PhoneNumberFormatCase(u"525553428400", u"MX", u"+52 55 5342 8400"),
        PhoneNumberFormatCase(u"5553428400", u"MX", u"+52 55 5342 8400"),

        //////////////////////////
        // MX phone in US.
        //////////////////////////
        // A MX phone with the country code is correctly formatted as a MX
        // number.
        PhoneNumberFormatCase(u"+52 55 5342 8400", u"US", u"+52 55 5342 8400"),
        PhoneNumberFormatCase(u"52 55 5342 8400", u"US", u"+52 55 5342 8400"),
        // This number is not a valid US number, we won't try to format. If
        // AutofillInferCountryCallingCode is enabled, we just add the inferred
        // code.
        PhoneNumberFormatCase(u"55 5342 8400",
                              u"US",
                              base::FeatureList::IsEnabled(
                                  features::kAutofillInferCountryCallingCode)
                                  ? u"+15553428400"
                                  : u"5553428400")));

INSTANTIATE_TEST_SUITE_P(
    GetFormattedPhoneNumberForDisplay_EdgeCases,
    GetFormattedPhoneNumberForDisplayTest,
    testing::Values(
        //////////////////////////
        // No country.
        //////////////////////////
        // Fallback to locale if no country is set.
        PhoneNumberFormatCase(u"52 55 5342 8400",
                              u"",
                              u"+52 55 5342 8400",
                              "es_MX"),
        PhoneNumberFormatCase(u"55 5342 8400",
                              u"",
                              u"+52 55 5342 8400",
                              "es_MX"),
        PhoneNumberFormatCase(u"61 2 9374 4000",
                              u"",
                              u"+61 2 9374 4000",
                              "en_AU"),
        PhoneNumberFormatCase(u"02 9374 4000",
                              u"",
                              u"+61 2 9374 4000",
                              "en_AU"),

        // Numbers in local format yet are invalid with user locale, user might
        // be trying to enter a foreign number, calling formatting will just
        // return what the user entered.
        PhoneNumberFormatCase(u"55 5342 8400", u"", u"5553428400", "en_US"),
        PhoneNumberFormatCase(u"55 5342 8400", u"", u"5553428400"),
        PhoneNumberFormatCase(u"226 123 1234", u"", u"2261231234", "en_US"),
        PhoneNumberFormatCase(u"293744000", u"", u"293744000"),
        PhoneNumberFormatCase(u"02 9374 4000", u"", u"0293744000"),

        //////////////////////////
        // No country or locale.
        //////////////////////////
        // Format according to the country code.
        PhoneNumberFormatCase(u"61 2 9374 4000", u"", u"+61 2 9374 4000"),
        PhoneNumberFormatCase(u"52 55 5342 8400", u"", u"+52 55 5342 8400"),
        PhoneNumberFormatCase(u"1 415 555 5555", u"", u"+1 415-555-5555"),
        // If no country code is found, formats for US.
        PhoneNumberFormatCase(u"415-555-5555", u"", u"+1 415-555-5555")));

}  // namespace
}  // namespace autofill
