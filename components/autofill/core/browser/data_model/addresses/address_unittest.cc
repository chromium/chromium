// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/address.h"

#include <stddef.h>

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::autofill::i18n_model_definition::kLegacyHierarchyCountryCode;
using ::base::ASCIIToUTF16;

class AddressTest : public testing::Test {
 private:
  base::test::ScopedFeatureList features_{features::kAutofillUseINAddressModel};
};

// Test that country data can be properly returned as either a country code or a
// localized country name.
TEST_F(AddressTest, GetCountry) {
  Address address(kLegacyHierarchyCountryCode);
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"");

  // Make sure that nothing breaks when the country code is missing.
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"");

  address.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"United States");
  EXPECT_EQ(address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US"),
            u"United States");
  EXPECT_EQ(address.GetInfo(
                AutofillType(ADDRESS_HOME_COUNTRY, /*is_country_code=*/true),
                "en-US"),
            u"US");

  address.SetRawInfo(ADDRESS_HOME_COUNTRY, u"CA");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"Canada");
  EXPECT_EQ(address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US"),
            u"Canada");
  EXPECT_EQ(address.GetInfo(
                AutofillType(ADDRESS_HOME_COUNTRY, /*is_country_code=*/true),
                "en-US"),
            u"CA");
}

// Test that country data can be properly returned as either a country code or a
// full country name that can even be localized.
TEST_F(AddressTest, SetHtmlCountryCodeTypeWithFullCountryName) {
  Address address(kLegacyHierarchyCountryCode);
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"");

  AutofillType autofill_type(ADDRESS_HOME_COUNTRY, /*is_country_code=*/true);

  // Test that the country value can be set and retrieved if it is not
  // a country code but a full country name.
  address.SetInfo(autofill_type, u"Germany", "en-US");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"Germany");
  EXPECT_EQ(address.GetInfo(
                AutofillType(ADDRESS_HOME_COUNTRY, /*is_country_code=*/true),
                "en-US"),
            u"DE");

  // Reset the country and verify that the reset works as expected.
  address.SetInfo(autofill_type, u"", "en-US");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"");
  EXPECT_EQ(address.GetInfo(
                AutofillType(ADDRESS_HOME_COUNTRY, /*is_country_code=*/true),
                "en-US"),
            u"");

  // Test that the country value can be set and retrieved if it is not
  // a country code but a full country name with a non-standard locale.
  address.SetInfo(autofill_type, u"deutschland", "de");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"Germany");
  EXPECT_EQ(address.GetInfo(
                AutofillType(ADDRESS_HOME_COUNTRY, /*is_country_code=*/true),
                "en-US"),
            u"DE");

  // Reset the country.
  address.SetInfo(autofill_type, u"", "en-US");

  // Test that the country is still stored correctly with a supplied
  // country code.
  address.SetInfo(autofill_type, u"DE", "en-US");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"Germany");
  EXPECT_EQ(address.GetInfo(
                AutofillType(ADDRESS_HOME_COUNTRY, /*is_country_code=*/true),
                "en-US"),
            u"DE");
}

// Test that we properly detect country codes appropriate for each country.
TEST_F(AddressTest, SetCountry) {
  Address address(kLegacyHierarchyCountryCode);
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"");

  // Test basic conversion.
  address.SetInfo(ADDRESS_HOME_COUNTRY, u"United States", "en-US");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"US");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"United States");

  // Test basic synonym detection.
  address.SetInfo(ADDRESS_HOME_COUNTRY, u"USA", "en-US");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"US");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"United States");

  // Test case-insensitivity.
  address.SetInfo(ADDRESS_HOME_COUNTRY, u"canADA", "en-US");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"CA");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"Canada");

  // Test country code detection.
  address.SetInfo(ADDRESS_HOME_COUNTRY, u"JP", "en-US");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"JP");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"Japan");

  // Test that we ignore unknown countries.
  address.SetInfo(ADDRESS_HOME_COUNTRY, u"Unknown", "en-US");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"");

  // Test setting the country based on an HTML field type.
  AutofillType html_type_country_code =
      AutofillType(ADDRESS_HOME_COUNTRY, /*is_country_code=*/true);
  address.SetInfo(html_type_country_code, u"US", "en-US");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"US");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"United States");

  // Test case-insensitivity when setting the country based on an HTML field
  // type.
  address.SetInfo(html_type_country_code, u"cA", "en-US");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"CA");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"Canada");

  // Test setting the country based on invalid data with an HTML field type.
  address.SetInfo(html_type_country_code, u"unknown", "en-US");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"");

  // Test incorrect use of country codes (when a country name is passed
  // as a country code).
  address.SetInfo(html_type_country_code, u"日本", "ja-JP");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"JP");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US"), u"Japan");
}

// Test setting and getting the new structured address tokens
TEST_F(AddressTest, StructuredAddressTokens) {
  Address address(kLegacyHierarchyCountryCode);

  // Set the address tokens.
  address.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"StreetName");
  address.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"HouseNumber");
  address.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"SubPremise");

  // Retrieve the tokens and verify that they are correct.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"StreetName");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"HouseNumber");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_SUBPREMISE), u"SubPremise");
}

// Test that we properly match typed values to stored country data.
TEST_F(AddressTest, IsCountry) {
  Address address(kLegacyHierarchyCountryCode);
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

  const char* const kValidMatches[] = {"United States", "USA", "US",
                                       "United states", "us"};
  for (const char* valid_match : kValidMatches) {
    SCOPED_TRACE(valid_match);
    FieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(valid_match), "US", &matching_types);
    ASSERT_EQ(1U, matching_types.size());
    EXPECT_EQ(ADDRESS_HOME_COUNTRY, *matching_types.begin());
  }

  const char* const kInvalidMatches[] = {"United", "Garbage"};
  for (const char* invalid_match : kInvalidMatches) {
    FieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(invalid_match), "US",
                             &matching_types);
    EXPECT_EQ(0U, matching_types.size());
  }

  // Make sure that garbage values don't match when the country code is empty.
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, std::u16string());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"");
  FieldTypeSet matching_types;
  address.GetMatchingTypes(u"Garbage", "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
}

// Verifies that Address::GetInfo() correctly combines address lines.
TEST_F(AddressTest, GetStreetAddress) {
  // Address has no address lines.
  Address address(kLegacyHierarchyCountryCode);
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_STREET_ADDRESS, "en-US"), u"");

  // Address has only line 1.
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_STREET_ADDRESS, "en-US"),
            u"123 Example Ave.");

  // Address has only line 2.
  address.SetRawInfo(ADDRESS_HOME_LINE1, std::u16string());
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt 42.");
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_STREET_ADDRESS, "en-US"),
            u"\nApt 42.");

  // Address has lines 1 and 2.
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"123 Example Ave.\n"
            u"Apt. 42");
  EXPECT_EQ(address.GetInfo(ADDRESS_HOME_STREET_ADDRESS, "en-US"),
            u"123 Example Ave.\n"
            u"Apt. 42");

  // A wild third line appears.
  address.SetRawInfo(ADDRESS_HOME_LINE3, u"Living room couch");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE3), u"Living room couch");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"123 Example Ave.\n"
            u"Apt. 42\n"
            u"Living room couch");

  // The second line vanishes.
  address.SetRawInfo(ADDRESS_HOME_LINE2, std::u16string());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"123 Example Ave.\n"
            u"\n"
            u"Living room couch");
}

// Verifies that overwriting an address with N lines with one that has fewer
// than N lines does not result in an address with blank lines at the end.
TEST_F(AddressTest, GetStreetAddressAfterOverwritingLongAddressWithShorterOne) {
  // Start with an address that has two lines.
  Address address(kLegacyHierarchyCountryCode);
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");

  // Now clear out the second address line.
  address.SetRawInfo(ADDRESS_HOME_LINE2, std::u16string());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"123 Example Ave.");

  // Now clear out the first address line as well.
  address.SetRawInfo(ADDRESS_HOME_LINE1, std::u16string());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS), u"");
}

// Verifies that Address::SetRawInfo() is able to split address lines correctly.
TEST_F(AddressTest, SetRawStreetAddress) {
  const std::u16string empty_street_address;
  const std::u16string short_street_address = u"456 Nowhere Ln.";
  const std::u16string long_street_address =
      u"123 Example Ave.\n"
      u"Apt. 42\n"
      u"(The one with the blue door)";

  Address address(kLegacyHierarchyCountryCode);
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1), u"");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2), u"");

  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, long_street_address);
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1), u"123 Example Ave.");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2), u"Apt. 42");
  EXPECT_EQ(long_street_address,
            address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));

  // A short address should clear out unused address lines.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, short_street_address);
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1), u"456 Nowhere Ln.");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2), u"");

  // An empty address should clear out all address lines.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, long_street_address);
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, empty_street_address);
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1), u"");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2), u"");
}

// Street addresses should be set properly.
TEST_F(AddressTest, SetStreetAddress) {
  const std::u16string empty_street_address;
  const std::u16string multi_line_street_address =
      u"789 Fancy Pkwy.\n"
      u"Unit 3.14\n"
      u"Box 9";
  const std::u16string single_line_street_address = u"123 Main, Apt 7";

  // Start with a non-empty address.
  Address address(kLegacyHierarchyCountryCode);
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");
  address.SetRawInfo(ADDRESS_HOME_LINE3, u"and a half");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());

  // Attempting to set a multi-line address should succeed.
  EXPECT_TRUE(address.SetInfo(ADDRESS_HOME_STREET_ADDRESS,
                              multi_line_street_address, "en-US"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1), u"789 Fancy Pkwy.");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2), u"Unit 3.14");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE3), u"Box 9");

  // Setting a single line street address should clear out subsequent lines.
  EXPECT_TRUE(address.SetInfo(ADDRESS_HOME_STREET_ADDRESS,
                              single_line_street_address, "en-US"));
  EXPECT_EQ(single_line_street_address, address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2), u"");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE3), u"");

  // Attempting to set an empty address should also succeed, and clear out the
  // previously stored data.
  EXPECT_TRUE(address.SetInfo(ADDRESS_HOME_STREET_ADDRESS,
                              multi_line_street_address, "en-US"));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_TRUE(address.SetInfo(ADDRESS_HOME_STREET_ADDRESS, empty_street_address,
                              "en-US"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1), u"");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2), u"");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE3), u"");
}

// Verifies that Address::SetInfio() rejects setting data for
// ADDRESS_HOME_STREET_ADDRESS if the data has any interior blank lines.
TEST_F(AddressTest, SetStreetAddressRejectsAddressesWithInteriorBlankLines) {
  // Start with a non-empty address.
  Address address(kLegacyHierarchyCountryCode);
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS).empty());

  // Attempting to set an address with interior blank lines should fail, and
  // clear out the previously stored address.
  EXPECT_FALSE(address.SetInfo(ADDRESS_HOME_STREET_ADDRESS,
                               u"Address line 1\n"
                               u"\n"
                               u"Address line 3",
                               "en-US"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1), u"");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2), u"");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS), u"");
}

// Verifies that Address::SetInfio() rejects setting data for
// ADDRESS_HOME_STREET_ADDRESS if the data has any leading blank lines.
TEST_F(AddressTest, SetStreetAddressRejectsAddressesWithLeadingBlankLines) {
  // Start with a non-empty address.
  Address address(kLegacyHierarchyCountryCode);
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS).empty());

  // Attempting to set an address with leading blank lines should fail, and
  // clear out the previously stored address.
  EXPECT_FALSE(address.SetInfo(ADDRESS_HOME_STREET_ADDRESS,
                               u"\n"
                               u"Address line 2"
                               u"Address line 3",
                               "en-US"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1), u"");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2), u"");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS), u"");
}

// Verifies that Address::SetInfio() rejects setting data for
// ADDRESS_HOME_STREET_ADDRESS if the data has any trailing blank lines.
TEST_F(AddressTest, SetStreetAddressRejectsAddressesWithTrailingBlankLines) {
  // Start with a non-empty address.
  Address address(kLegacyHierarchyCountryCode);
  address.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example Ave.");
  address.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS).empty());

  // Attempting to set an address with leading blank lines should fail, and
  // clear out the previously stored address.
  EXPECT_FALSE(address.SetInfo(ADDRESS_HOME_STREET_ADDRESS,
                               u"Address line 1"
                               u"Address line 2"
                               u"\n",
                               "en-US"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1), u"");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2), u"");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS), u"");
}

// Verifies that the merging-related methods for structured addresses are
// implemented correctly. This is not a test of the merging logic itself.
TEST_F(AddressTest, TestMergeStructuredAddresses) {
  autofill::AutofillProfileComparator profile_comparator("en-US");

  // The two zip codes have a is-substring relation and are mergeable.
  AutofillProfile profile1("1", AutofillProfile::RecordType::kAccount,
                           AddressCountryCode(kLegacyHierarchyCountryCode));
  AutofillProfile profile2("2", AutofillProfile::RecordType::kAccount,
                           AddressCountryCode(kLegacyHierarchyCountryCode));
  // Two empty profiles are mergeable by default.
  EXPECT_TRUE(profile_comparator.AreMergeable(profile1, profile2));
  // We use SetProfileInfo instead of SetRawInfo as it calls
  // FinalizeAfterImport() making it more similar to how the tree is handled in
  // prod - which is recommended as we're using tree's interfaces for merging.
  test::SetProfileInfo(&profile1, "", "", "", "", "", "", "", "", "", "",
                       /*zipcode=*/"12345", "", "");
  test::SetProfileInfo(&profile2, "", "", "", "", "", "", "", "", "", "",
                       /*zipcode=*/"1234", "", "");

  EXPECT_TRUE(profile_comparator.AreMergeable(profile1, profile2));

  base::Time old_time;
  ASSERT_TRUE(
      base::Time::FromString("Tue, 15 Nov 1994 12:45:26 GMT", &old_time));
  base::Time new_time;
  ASSERT_TRUE(
      base::Time::FromString("Tue, 15 Nov 1996 12:45:26 GMT", &new_time));

  profile1.usage_history().set_use_date(old_time);
  profile2.usage_history().set_use_date(old_time);
  // The merging should maintain the value because profile2 is not more
  // recently used.
  profile1.MergeDataFrom(profile2, "en-US");
  EXPECT_EQ(profile1.GetRawInfo(ADDRESS_HOME_ZIP), u"12345");
  // Once it is more recently used, the value from profile2 should be copied
  // into profile1.
  profile2.usage_history().set_use_date(new_time);
  profile1.MergeDataFrom(profile2, "en-US");
  EXPECT_EQ(profile1.GetRawInfo(ADDRESS_HOME_ZIP), u"1234");

  // With a second incompatible ZIP code the addresses are not mergeable
  // anymore.
  AutofillProfile profile3("3", AutofillProfile::RecordType::kAccount,
                           AddressCountryCode(kLegacyHierarchyCountryCode));

  test::SetProfileInfo(&profile3, "", "", "", "", "", "", "", "", "", "",
                       "67890", "", "");
  EXPECT_FALSE(profile_comparator.AreMergeable(profile1, profile3));
}

// Tests that if only one of the structured addresses in a merge operation has
// country information, it is used as their common country during comparison and
// for rewriting rules.
TEST_F(AddressTest, TestMergeStructuredAddressesMissingCountry) {
  Address address1(kLegacyHierarchyCountryCode);
  Address address2(kLegacyHierarchyCountryCode);

  address1.SetRawInfo(ADDRESS_HOME_COUNTRY, u"GB");
  address1.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                            u"1 Trafalgar Square",
                                            VerificationStatus::kUserVerified);

  address2.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                            u"1 Trafalgar Square",
                                            VerificationStatus::kObserved);

  // |address1| and |address2|'s street address are not trivially the same,
  // because their verification status differs. But they should still be
  // mergeable, regardless if one of them has a country or not. This is not
  // trivially given, because country-specific rewriting rules are applied
  // during the merge process.
  EXPECT_TRUE(address1.IsStructuredAddressMergeable(address2));
  EXPECT_TRUE(address2.IsStructuredAddressMergeable(address1));
}

// Tests the retrieval of the structured address.
TEST_F(AddressTest, TestGettingTheStructuredAddress) {
  // Create the address and set a test value.
  Address address1(kLegacyHierarchyCountryCode);
  address1.SetRawInfo(ADDRESS_HOME_ZIP, u"12345");

  // Get the structured address and verify that it has the same test value set.
  const AddressComponent& structured_address = address1.GetRoot();
  EXPECT_EQ(structured_address.GetValueForType(ADDRESS_HOME_ZIP), u"12345");
}

// For structured address, test that the structured information is wiped
// correctly when the unstructured street address changes.
TEST_F(AddressTest, ResetStructuredTokens) {
  Address address(kLegacyHierarchyCountryCode);
  // Set a structured address line and call the finalization routine.
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"Erika-Mann-Str 12",
                                           VerificationStatus::kUserVerified);
  address.FinalizeAfterImport();

  // Verify that structured tokens have been assigned correctly.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Erika-Mann-Str");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kParsed);
  ASSERT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"12");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kParsed);

  // Lift the verification status of the house number to be |kObserved|.
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"12",
                                           VerificationStatus::kObserved);
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kObserved);

  // Now, set a new unstructured street address that has the same tokens in a
  // different order.
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"12 Erika-Mann-Str",
                                           VerificationStatus::kUserVerified);

  // After this operation, the structure should be maintained including the
  // observed status of the house number.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Erika-Mann-Str");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kParsed);
  ASSERT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"12");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kObserved);

  // Now set a different street address.
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"Marienplatz",
                                           VerificationStatus::kUserVerified);

  // The set address is not parsable and the this should unset both the street
  // name and the house number.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kNoStatus);
  ASSERT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"");
  EXPECT_EQ(address.GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kNoStatus);
}

TEST_F(AddressTest, IsLegacyAddress) {
  Address address(kLegacyHierarchyCountryCode);
  EXPECT_TRUE(address.IsLegacyAddress());

  Address address_br(AddressCountryCode("BR"));
  EXPECT_FALSE(address_br.IsLegacyAddress());

  Address address_mx(AddressCountryCode("MX"));
  EXPECT_FALSE(address_mx.IsLegacyAddress());

  Address i18n_copy(kLegacyHierarchyCountryCode);
  // The legacy address should adopt the non-legacy one.
  i18n_copy = address_mx;
  EXPECT_FALSE(i18n_copy.IsLegacyAddress());
  // The non-legacy address should adopt the legacy one.
  address_br = address;
  EXPECT_TRUE(address_br.IsLegacyAddress());
  // Assignment between legacy addresses should stay legacy.
  Address legacy_copy(kLegacyHierarchyCountryCode);
  legacy_copy = address;
  EXPECT_TRUE(legacy_copy.IsLegacyAddress());
}

TEST_F(AddressTest, IsLegacyAddressUpdateCountry) {
  Address address(kLegacyHierarchyCountryCode);
  EXPECT_TRUE(address.IsLegacyAddress());

  address.SetRawInfo(ADDRESS_HOME_COUNTRY, u"BR");
  EXPECT_FALSE(address.IsLegacyAddress());

  address.SetRawInfo(ADDRESS_HOME_COUNTRY, u"AZ");
  EXPECT_TRUE(address.IsLegacyAddress());

  address.SetRawInfo(ADDRESS_HOME_COUNTRY, u"MX");
  EXPECT_FALSE(address.IsLegacyAddress());

  address.SetRawInfo(ADDRESS_HOME_COUNTRY, u"");
  EXPECT_TRUE(address.IsLegacyAddress());
}

TEST_F(AddressTest, TestUpdateLegacyToCustomHierarchy) {
  Address address(kLegacyHierarchyCountryCode);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Munich",
                                           VerificationStatus::kObserved);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"Bayern",
                                           VerificationStatus::kObserved);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"111",
                                           VerificationStatus::kObserved);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                           u"Municipality",
                                           VerificationStatus::kObserved);

  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"12 Erika-Mann-Str Floor 15 Apt 13",
                                           VerificationStatus::kUserVerified);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_LOCATION,
                                           u"12 Erika-Mann-Str",
                                           VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Erika-Mann-Str", VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"12",
                                           VerificationStatus::kParsed);

  address.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_SUBPREMISE, u"Floor 15 Apt 13", VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"15",
                                           VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"13",
                                           VerificationStatus::kParsed);

  // Updates the internal hierarchy and copies the data into the new model.
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, u"BR");
  // Completes all gaps in the new model.
  address.FinalizeAfterImport();

  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"BR");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_CITY), u"Munich");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STATE), u"Bayern");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_ZIP), u"111");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY),
            u"Municipality");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"12 Erika-Mann-Str Floor 15 Apt 13");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_LOCATION),
            u"12 Erika-Mann-Str");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Erika-Mann-Str");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"12");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_SUBPREMISE), u"Floor 15 Apt 13");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_FLOOR), u"15");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_APT_NUM), u"13");

  // BR specific nodes.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_OVERFLOW_AND_LANDMARK),
            u"Floor 15 Apt 13");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_OVERFLOW), u"Floor 15 Apt 13");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_APT), u"13");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LANDMARK), u"");
}

TEST_F(AddressTest, TestUpdateCustomHierarchyToLegacy) {
  Address address(AddressCountryCode("BR"));

  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Munich",
                                           VerificationStatus::kObserved);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"Bayern",
                                           VerificationStatus::kObserved);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"111",
                                           VerificationStatus::kObserved);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                           u"Municipality",
                                           VerificationStatus::kObserved);

  address.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS,
      u"12 Erika-Mann-Str Floor 15 Apt 13, Near red tower",
      VerificationStatus::kUserVerified);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_LOCATION,
                                           u"12 Erika-Mann-Str",
                                           VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Erika-Mann-Str", VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"12",
                                           VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
                                           u"Floor 15 Apt 13, Near red tower",
                                           VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_OVERFLOW, u"Floor 15 Apt 13", VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_LANDMARK, u"Near red tower", VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_SUBPREMISE, u"Floor 15 Apt 13", VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"15",
                                           VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT, u"Apt 13",
                                           VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_TYPE, u"Apt",
                                           VerificationStatus::kParsed);
  address.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"13",
                                           VerificationStatus::kParsed);

  // Updates the internal hierarchy and copies the data into the legacy model.
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, u"AZ");
  address.FinalizeAfterImport();

  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_COUNTRY), u"AZ");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_CITY), u"Munich");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STATE), u"Bayern");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_ZIP), u"111");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY),
            u"Municipality");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"12 Erika-Mann-Str Floor 15 Apt 13, Near red tower");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_LOCATION),
            u"12 Erika-Mann-Str");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Erika-Mann-Str");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"12");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_SUBPREMISE), u"Floor 15 Apt 13");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_FLOOR), u"15");
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_APT_NUM), u"13");
}

TEST_F(AddressTest, TestSynthesizedNodesGeneration) {
  Address address1(AddressCountryCode("IN"));
  address1.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                            u"Kondapur",
                                            VerificationStatus::kObserved);
  address1.FinalizeAfterImport();
  EXPECT_EQ(address1.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK),
            u"Kondapur");

  Address address2(AddressCountryCode("IN"));
  address2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_LOCATION, u"12/110, Flat no. 504, Raja Apartments",
      VerificationStatus::kObserved);
  address2.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                            u"Kondapur",
                                            VerificationStatus::kObserved);
  address2.SetRawInfoWithVerificationStatus(ADDRESS_HOME_LANDMARK,
                                            u"Opp to Ayyappa Swamy temple",
                                            VerificationStatus::kObserved);
  // Finalize after import generates values for synthesized nodes.
  address2.FinalizeAfterImport();
  // Check values for synthesized nodes.
  EXPECT_EQ(address2.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK),
            u"Kondapur, Opp to Ayyappa Swamy temple");
  EXPECT_EQ(
      address2.GetRawInfo(ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK),
      u"12/110, Flat no. 504, Raja Apartments, Opp to Ayyappa Swamy temple");
  // Check values for internal nodes.
  EXPECT_EQ(address2.GetRawInfo(ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY),
            u"12/110, Flat no. 504, Raja Apartments, Kondapur");
  EXPECT_EQ(address2.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"12/110, Flat no. 504, Raja Apartments, Kondapur\n"
            u"Opp to Ayyappa Swamy temple");
}

}  // namespace
}  // namespace autofill
