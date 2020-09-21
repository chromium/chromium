// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/address.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

class AddressTest : public testing::Test,
                    public testing::WithParamInterface<bool> {
 public:
  AddressTest() { CountryNames::SetLocaleString("en-US"); }

  bool StructuredAddresses() const { return structured_addresses_enabled_; }

 private:
  void SetUp() override { InitializeFeatures(); }

  void InitializeFeatures() {
    structured_addresses_enabled_ = GetParam();
    if (structured_addresses_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kAutofillEnableSupportForMoreStructureInAddresses);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kAutofillEnableSupportForMoreStructureInAddresses);
    }
  }

  bool structured_addresses_enabled_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that country data can be properly returned as either a country code or a
// localized country name.
TEST_P(AddressTest, GetCountry) {
  Address address;
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));

  // Make sure that nothing breaks when the country code is missing.
  base::string16 country =
      address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(base::string16(), country);

  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("US"),
                  "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("United States"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_NAME, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("United States"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), country);

  address.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("CA"));
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_NAME, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("CA"), country);
}

// Test that country data can be properly returned as either a country code or a
// full country name that can even be localized.
TEST_P(AddressTest, SetHtmlCountryCodeTypeWithFullCountryName) {
  Address address;
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));

  // Enable the feature that allows for full country names although the
  // field type explicitly set to HTML_TYPE_COUNTRY_CODE.
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(
      features::kAutofillAllowHtmlTypeCountryCodesWithFullNames);

  // Create an autofill type from HTML_TYPE_COUNTRY_CODE.
  AutofillType autofill_type(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);

  // Test that the country value can be set and retrieved if it is not
  // a country code but a full country name.
  address.SetInfo(autofill_type, ASCIIToUTF16("Germany"), "en-US");
  base::string16 actual_country =
      address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  base::string16 actual_country_code = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("Germany"), actual_country);
  EXPECT_EQ(ASCIIToUTF16("DE"), actual_country_code);

  // Reset the country and verify that the reset works as expected.
  address.SetInfo(autofill_type, ASCIIToUTF16(""), "en-US");
  actual_country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  actual_country_code = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16(""), actual_country);
  EXPECT_EQ(ASCIIToUTF16(""), actual_country_code);

  // Test that the country value can be set and retrieved if it is not
  // a country code but a full country name with a non-standard locale.
  address.SetInfo(autofill_type, ASCIIToUTF16("deutschland"), "de");
  actual_country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  actual_country_code = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("Germany"), actual_country);
  EXPECT_EQ(ASCIIToUTF16("DE"), actual_country_code);

  // Reset the country.
  address.SetInfo(autofill_type, ASCIIToUTF16(""), "en-US");

  // Test that the country is still stored correctly with a supplied
  // country code.
  address.SetInfo(autofill_type, ASCIIToUTF16("DE"), "en-US");
  actual_country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  actual_country_code = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("DE"), actual_country_code);
  EXPECT_EQ(ASCIIToUTF16("Germany"), actual_country);

  // By disabling the feature, test that the country name deduction actually
  // uses the path for HTML_TYPE_COUNTRY_CODE.
  feature.Reset();
  feature.InitAndDisableFeature(
      features::kAutofillAllowHtmlTypeCountryCodesWithFullNames);
  address.SetInfo(autofill_type, ASCIIToUTF16("Germany"), "en-US");
  actual_country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  actual_country_code = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16(""), actual_country);
  EXPECT_EQ(ASCIIToUTF16(""), actual_country_code);
}

// Test that we properly detect country codes appropriate for each country.
TEST_P(AddressTest, SetCountry) {
  Address address;
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));

  // Test basic conversion.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY),
                  ASCIIToUTF16("United States"), "en-US");
  base::string16 country =
      address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test basic synonym detection.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("USA"),
                  "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test case-insensitivity.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("canADA"),
                  "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("CA"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);

  // Test country code detection.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("JP"),
                  "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("JP"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("Japan"), country);

  // Test that we ignore unknown countries.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("Unknown"),
                  "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(base::string16(), country);

  // Test setting the country based on an HTML field type.
  AutofillType html_type_country_code =
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  address.SetInfo(html_type_country_code, ASCIIToUTF16("US"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test case-insensitivity when setting the country based on an HTML field
  // type.
  address.SetInfo(html_type_country_code, ASCIIToUTF16("cA"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("CA"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);

  // Test setting the country based on invalid data with an HTML field type.
  address.SetInfo(html_type_country_code, ASCIIToUTF16("unknown"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(base::string16(), country);
}

// Test setting and getting the new structured address tokens
TEST_P(AddressTest, StructuredAddressTokens) {
  // Activate the feature to support the new structured address tokens.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillAddressEnhancementVotes);
  Address address;

  // Set the address tokens.
  address.SetRawInfo(ADDRESS_HOME_STREET_NAME,
                     base::ASCIIToUTF16("StreetName"));
  address.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER,
                     base::ASCIIToUTF16("HouseNumber"));
  address.SetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME,
                     base::ASCIIToUTF16("DependentStreetName"));
  address.SetRawInfo(ADDRESS_HOME_PREMISE_NAME,
                     base::ASCIIToUTF16("PremiseNmae"));
  address.SetRawInfo(ADDRESS_HOME_SUBPREMISE, base::ASCIIToUTF16("SubPremise"));

  // Retrieve the tokens and verify that they are correct.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME),
            base::ASCIIToUTF16("StreetName"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER),
            base::ASCIIToUTF16("HouseNumber"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME),
            base::ASCIIToUTF16("DependentStreetName"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_PREMISE_NAME),
            base::ASCIIToUTF16("PremiseNmae"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_SUBPREMISE),
            base::ASCIIToUTF16("SubPremise"));
}

// Test setting and getting the new structured address tokens
TEST_P(AddressTest,
       StructuredAddressTokens_ResetOnChangedUnstructuredInformation) {
  // Activate the feature to support the new structured address tokens for
  // voting. The feature to actively support structured addresses must be turned
  // of.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kAutofillAddressEnhancementVotes},
      {features::kAutofillEnableSupportForMoreStructureInAddresses});
  Address address;

  // Set the address tokens.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     base::ASCIIToUTF16("Line1\nLine2"));
  address.SetRawInfo(ADDRESS_HOME_STREET_NAME,
                     base::ASCIIToUTF16("StreetName"));
  address.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER,
                     base::ASCIIToUTF16("HouseNumber"));
  address.SetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME,
                     base::ASCIIToUTF16("DependentStreetName"));
  address.SetRawInfo(ADDRESS_HOME_PREMISE_NAME,
                     base::ASCIIToUTF16("PremiseNmae"));
  address.SetRawInfo(ADDRESS_HOME_SUBPREMISE, base::ASCIIToUTF16("SubPremise"));

  // Retrieve the tokens and verify that they are correct.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1),
            base::ASCIIToUTF16("Line1"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2),
            base::ASCIIToUTF16("Line2"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            base::ASCIIToUTF16("Line1\nLine2"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME),
            base::ASCIIToUTF16("StreetName"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER),
            base::ASCIIToUTF16("HouseNumber"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME),
            base::ASCIIToUTF16("DependentStreetName"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_PREMISE_NAME),
            base::ASCIIToUTF16("PremiseNmae"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_SUBPREMISE),
            base::ASCIIToUTF16("SubPremise"));

  // Set the unstructured address information to the same values as they already
  // are.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     base::ASCIIToUTF16("Line1\nLine2"));
  address.SetRawInfo(ADDRESS_HOME_LINE1, base::ASCIIToUTF16("Line1"));
  address.SetRawInfo(ADDRESS_HOME_LINE2, base::ASCIIToUTF16("Line2"));

  // Verify that the structured tokens are still set.
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME),
            base::ASCIIToUTF16("StreetName"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER),
            base::ASCIIToUTF16("HouseNumber"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME),
            base::ASCIIToUTF16("DependentStreetName"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_PREMISE_NAME),
            base::ASCIIToUTF16("PremiseNmae"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_SUBPREMISE),
            base::ASCIIToUTF16("SubPremise"));

  // Now, change the address by changing HOME_ADDRESS_LINE1 and verify that the
  // structured tokens are reset.
  address.SetRawInfo(ADDRESS_HOME_LINE1, base::ASCIIToUTF16("NewLine1"));

  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1),
            base::ASCIIToUTF16("NewLine1"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2),
            base::ASCIIToUTF16("Line2"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            base::ASCIIToUTF16("NewLine1\nLine2"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME),
            base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_PREMISE_NAME), base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_SUBPREMISE), base::string16());

  // Reset the structured tokens and perform the same step for
  // HOME_ADDRESS_LINE2.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     base::ASCIIToUTF16("Line1\nLine2"));
  address.SetRawInfo(ADDRESS_HOME_STREET_NAME,
                     base::ASCIIToUTF16("StreetName"));
  address.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER,
                     base::ASCIIToUTF16("HouseNumber"));
  address.SetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME,
                     base::ASCIIToUTF16("DependentStreetName"));
  address.SetRawInfo(ADDRESS_HOME_PREMISE_NAME,
                     base::ASCIIToUTF16("PremiseNmae"));
  address.SetRawInfo(ADDRESS_HOME_SUBPREMISE, base::ASCIIToUTF16("SubPremise"));

  address.SetRawInfo(ADDRESS_HOME_LINE2, base::ASCIIToUTF16("NewLine2"));

  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1),
            base::ASCIIToUTF16("Line1"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2),
            base::ASCIIToUTF16("NewLine2"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            base::ASCIIToUTF16("Line1\nNewLine2"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME),
            base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_PREMISE_NAME), base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_SUBPREMISE), base::string16());

  // And once again for ADDRESS_HOME_STREET_ADDRESS.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     base::ASCIIToUTF16("Line1\nLine2"));
  address.SetRawInfo(ADDRESS_HOME_STREET_NAME,
                     base::ASCIIToUTF16("StreetName"));
  address.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER,
                     base::ASCIIToUTF16("HouseNumber"));
  address.SetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME,
                     base::ASCIIToUTF16("DependentStreetName"));
  address.SetRawInfo(ADDRESS_HOME_PREMISE_NAME,
                     base::ASCIIToUTF16("PremiseNmae"));
  address.SetRawInfo(ADDRESS_HOME_SUBPREMISE, base::ASCIIToUTF16("SubPremise"));

  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     base::ASCIIToUTF16("NewLine1\nNewLine2"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE1),
            base::ASCIIToUTF16("NewLine1"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_LINE2),
            base::ASCIIToUTF16("NewLine2"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            base::ASCIIToUTF16("NewLine1\nNewLine2"));
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_STREET_NAME), base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME),
            base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_PREMISE_NAME), base::string16());
  EXPECT_EQ(address.GetRawInfo(ADDRESS_HOME_SUBPREMISE), base::string16());
}

// Test that we properly match typed values to stored country data.
TEST_P(AddressTest, IsCountry) {
  Address address;
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));

  const char* const kValidMatches[] = {"United States", "USA", "US",
                                       "United states", "us"};
  for (const char* valid_match : kValidMatches) {
    SCOPED_TRACE(valid_match);
    ServerFieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(valid_match), "US", &matching_types);
    ASSERT_EQ(1U, matching_types.size());
    EXPECT_EQ(ADDRESS_HOME_COUNTRY, *matching_types.begin());
  }

  const char* const kInvalidMatches[] = {"United", "Garbage"};
  for (const char* invalid_match : kInvalidMatches) {
    ServerFieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(invalid_match), "US",
                             &matching_types);
    EXPECT_EQ(0U, matching_types.size());
  }

  // Make sure that garbage values don't match when the country code is empty.
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, base::string16());
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  ServerFieldTypeSet matching_types;
  address.GetMatchingTypes(ASCIIToUTF16("Garbage"), "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
}

// Verifies that Address::GetInfo() correctly combines address lines.
TEST_P(AddressTest, GetStreetAddress) {
  const AutofillType type = AutofillType(ADDRESS_HOME_STREET_ADDRESS);

  // Address has no address lines.
  Address address;
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(base::string16(), address.GetInfo(type, "en-US"));

  // Address has only line 1.
  address.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Example Ave."));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(ASCIIToUTF16("123 Example Ave."), address.GetInfo(type, "en-US"));

  // Address has only line 2.
  address.SetRawInfo(ADDRESS_HOME_LINE1, base::string16());
  address.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Apt 42."));
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(ASCIIToUTF16("\nApt 42."), address.GetInfo(type, "en-US"));

  // Address has lines 1 and 2.
  address.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Example Ave."));
  address.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Apt. 42"));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(ASCIIToUTF16("123 Example Ave.\n"
                         "Apt. 42"),
            address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_EQ(ASCIIToUTF16("123 Example Ave.\n"
                         "Apt. 42"),
            address.GetInfo(type, "en-US"));

  // A wild third line appears.
  address.SetRawInfo(ADDRESS_HOME_LINE3, ASCIIToUTF16("Living room couch"));
  EXPECT_EQ(ASCIIToUTF16("Living room couch"),
            address.GetRawInfo(ADDRESS_HOME_LINE3));
  EXPECT_EQ(ASCIIToUTF16("123 Example Ave.\n"
                         "Apt. 42\n"
                         "Living room couch"),
            address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));

  // The second line vanishes.
  address.SetRawInfo(ADDRESS_HOME_LINE2, base::string16());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_EQ(ASCIIToUTF16("123 Example Ave.\n"
                         "\n"
                         "Living room couch"),
            address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

// Verifies that overwriting an address with N lines with one that has fewer
// than N lines does not result in an address with blank lines at the end.
TEST_P(AddressTest, GetStreetAddressAfterOverwritingLongAddressWithShorterOne) {
  // Start with an address that has two lines.
  Address address;
  address.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Example Ave."));
  address.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Apt. 42"));

  // Now clear out the second address line.
  address.SetRawInfo(ADDRESS_HOME_LINE2, base::string16());
  EXPECT_EQ(ASCIIToUTF16("123 Example Ave."),
            address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));

  // Now clear out the first address line as well.
  address.SetRawInfo(ADDRESS_HOME_LINE1, base::string16());
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

// Verifies that Address::SetRawInfo() is able to split address lines correctly.
TEST_P(AddressTest, SetRawStreetAddress) {
  const base::string16 empty_street_address;
  const base::string16 short_street_address = ASCIIToUTF16("456 Nowhere Ln.");
  const base::string16 long_street_address = ASCIIToUTF16(
      "123 Example Ave.\n"
      "Apt. 42\n"
      "(The one with the blue door)");

  Address address;
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));

  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, long_street_address);
  EXPECT_EQ(ASCIIToUTF16("123 Example Ave."),
            address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Apt. 42"), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(long_street_address,
            address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));

  // A short address should clear out unused address lines.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, short_street_address);
  EXPECT_EQ(ASCIIToUTF16("456 Nowhere Ln."),
            address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));

  // An empty address should clear out all address lines.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, long_street_address);
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, empty_street_address);
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));
}

// Street addresses should be set properly.
TEST_P(AddressTest, SetStreetAddress) {
  const base::string16 empty_street_address;
  const base::string16 multi_line_street_address = ASCIIToUTF16(
      "789 Fancy Pkwy.\n"
      "Unit 3.14\n"
      "Box 9");
  const base::string16 single_line_street_address =
      ASCIIToUTF16("123 Main, Apt 7");
  const AutofillType type = AutofillType(ADDRESS_HOME_STREET_ADDRESS);

  // Start with a non-empty address.
  Address address;
  address.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Example Ave."));
  address.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Apt. 42"));
  address.SetRawInfo(ADDRESS_HOME_LINE3, ASCIIToUTF16("and a half"));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());

  // Attempting to set a multi-line address should succeed.
  EXPECT_TRUE(address.SetInfo(type, multi_line_street_address, "en-US"));
  EXPECT_EQ(ASCIIToUTF16("789 Fancy Pkwy."),
            address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Unit 3.14"), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(ASCIIToUTF16("Box 9"), address.GetRawInfo(ADDRESS_HOME_LINE3));

  // Setting a single line street address should clear out subsequent lines.
  EXPECT_TRUE(address.SetInfo(type, single_line_street_address, "en-US"));
  EXPECT_EQ(single_line_street_address, address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE3));

  // Attempting to set an empty address should also succeed, and clear out the
  // previously stored data.
  EXPECT_TRUE(address.SetInfo(type, multi_line_street_address, "en-US"));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE3).empty());
  EXPECT_TRUE(address.SetInfo(type, empty_street_address, "en-US"));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE3));
}

// Verifies that Address::SetInfio() rejects setting data for
// ADDRESS_HOME_STREET_ADDRESS if the data has any interior blank lines.
TEST_P(AddressTest, SetStreetAddressRejectsAddressesWithInteriorBlankLines) {
  // Start with a non-empty address.
  Address address;
  address.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Example Ave."));
  address.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Apt. 42"));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS).empty());

  // Attempting to set an address with interior blank lines should fail, and
  // clear out the previously stored address.
  EXPECT_FALSE(address.SetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS),
                               ASCIIToUTF16("Address line 1\n"
                                            "\n"
                                            "Address line 3"),
                               "en-US"));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

// Verifies that Address::SetInfio() rejects setting data for
// ADDRESS_HOME_STREET_ADDRESS if the data has any leading blank lines.
TEST_P(AddressTest, SetStreetAddressRejectsAddressesWithLeadingBlankLines) {
  // Start with a non-empty address.
  Address address;
  address.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Example Ave."));
  address.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Apt. 42"));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS).empty());

  // Attempting to set an address with leading blank lines should fail, and
  // clear out the previously stored address.
  EXPECT_FALSE(address.SetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS),
                               ASCIIToUTF16("\n"
                                            "Address line 2"
                                            "Address line 3"),
                               "en-US"));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

// Verifies that Address::SetInfio() rejects setting data for
// ADDRESS_HOME_STREET_ADDRESS if the data has any trailing blank lines.
TEST_P(AddressTest, SetStreetAddressRejectsAddressesWithTrailingBlankLines) {
  // Start with a non-empty address.
  Address address;
  address.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Example Ave."));
  address.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Apt. 42"));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS).empty());

  // Attempting to set an address with leading blank lines should fail, and
  // clear out the previously stored address.
  EXPECT_FALSE(address.SetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS),
                               ASCIIToUTF16("Address line 1"
                                            "Address line 2"
                                            "\n"),
                               "en-US"));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

// Verifies that the merging-related methods for structured addresses are
// implemented correctly. This is not a test of the merging logic itself.
TEST_P(AddressTest, TestMergeStructuredAddresses) {
  // This test is only applicable for structured addresses.
  if (!StructuredAddresses())
    return;

  Address address1;
  Address address2;

  // Two empty addresses are mergeable by default.
  EXPECT_TRUE(address1.IsStructuredAddressMergeable(address2));

  // The two zip codes have a is-substring relation and are mergeable.
  address1.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("12345"));
  address2.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("1234"));
  EXPECT_TRUE(address2.IsStructuredAddressMergeable(address1));
  EXPECT_TRUE(address1.IsStructuredAddressMergeable(address2));

  // The merging should maintain the value because address2 is not more
  // recently used.
  address1.MergeStructuredAddress(address2,
                                  /*newer_use_more_recently_used=*/false);
  EXPECT_EQ(address1.GetRawInfo(ADDRESS_HOME_ZIP), base::UTF8ToUTF16("12345"));

  // Once it is more recently used, the value from address2 should be copied
  // into address1.
  address1.MergeStructuredAddress(address2,
                                  /*newer_use_more_recently_used=*/true);
  EXPECT_EQ(address1.GetRawInfo(ADDRESS_HOME_ZIP), base::UTF8ToUTF16("1234"));

  // With a second incompatible ZIP code the addresses are not mergeable
  // anymore.
  Address address3;
  address3.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("67890"));
  EXPECT_FALSE(address1.IsStructuredAddressMergeable(address3));
}

// Tests the retrieval of the structured address.
TEST_P(AddressTest, TestGettingTheStructuredAddress) {
  // This test is only applicable for structured addresses.
  if (!StructuredAddresses())
    return;

  // Create the address and set a test value.
  Address address1;
  address1.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("12345"));

  // Get the structured address and verify that it has the same test value set.
  structured_address::Address structured_address =
      address1.GetStructuredAddress();
  EXPECT_EQ(structured_address.GetValueForType(ADDRESS_HOME_ZIP),
            base::UTF8ToUTF16("12345"));
}

// Runs the suite with the feature
// |kAutofillSupportForStructuredStructuredNames| enabled and disabled.
// TODO(crbug.com/1130194): Remove parameterized test once structured addresses
// are fully launched.
INSTANTIATE_TEST_SUITE_P(, AddressTest, testing::Bool());

}  // namespace autofill
