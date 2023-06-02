// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address.h"

#include <stddef.h>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

using AddressComponentTestValues = std::vector<AddressComponentTestValue>;

struct AddressLineParsingTestCase {
  std::string country_code;
  std::string street_address;
  std::string street_name;
  std::string house_number;
  std::string floor;
  std::string apartment;
  std::string landmark;
  std::string between_streets;
  std::string admin_level_2;
};

std::ostream& operator<<(std::ostream& out,
                         const AddressLineParsingTestCase& test_case) {
  out << "Country: " << test_case.country_code << std::endl;
  out << "Street address: " << test_case.street_address << std::endl;
  out << "Street name: " << test_case.street_name << std::endl;
  out << "House number: " << test_case.house_number << std::endl;
  out << "Floor: " << test_case.floor << std::endl;
  out << "Apartment: " << test_case.apartment << std::endl;
  out << "Landmark: " << test_case.landmark << std::endl;
  out << "Between streets: " << test_case.between_streets << std::endl;
  out << "Admin level 2: " << test_case.admin_level_2 << std::endl;
  return out;
}

void TestAddressLineParsing(const AddressLineParsingTestCase& test_case) {
  AddressNode address(nullptr);
  const AddressComponentTestValues test_value = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = test_case.street_address,
       .status = VerificationStatus::kObserved}};

  SetTestValues(&address, test_value);

  SCOPED_TRACE(test_case);

  const AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_COUNTRY,
       .value = test_case.country_code,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = test_case.street_address,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_NAME,
       .value = test_case.street_name,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_HOUSE_NUMBER,
       .value = test_case.house_number,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_APT_NUM,
       .value = test_case.apartment,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_FLOOR,
       .value = test_case.floor,
       .status = VerificationStatus::kParsed}};
  VerifyTestValues(&address, expectation);
}

void TestAddressLineFormatting(const AddressLineParsingTestCase& test_case) {
  AddressNode address;
  const AddressComponentTestValues test_value = {
      {.type = ADDRESS_HOME_COUNTRY,
       .value = test_case.country_code,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_NAME,
       .value = test_case.street_name,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_HOUSE_NUMBER,
       .value = test_case.house_number,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_FLOOR,
       .value = test_case.floor,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_APT_NUM,
       .value = test_case.apartment,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LANDMARK,
       .value = test_case.landmark,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_BETWEEN_STREETS,
       .value = test_case.between_streets,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_ADMIN_LEVEL2,
       .value = test_case.admin_level_2,
       .status = VerificationStatus::kObserved}};

  SetTestValues(&address, test_value);

  SCOPED_TRACE(test_case);

  const AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_COUNTRY,
       .value = test_case.country_code,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = test_case.street_address,
       .status = VerificationStatus::kFormatted},
      {.type = ADDRESS_HOME_STREET_NAME,
       .value = test_case.street_name,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_HOUSE_NUMBER,
       .value = test_case.house_number,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_APT_NUM,
       .value = test_case.apartment,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_FLOOR,
       .value = test_case.floor,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LANDMARK,
       .value = test_case.landmark,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_BETWEEN_STREETS,
       .value = test_case.between_streets,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_ADMIN_LEVEL2,
       .value = test_case.admin_level_2,
       .status = VerificationStatus::kObserved}};
  VerifyTestValues(&address, expectation);
}

using AddressComponentTestValues = std::vector<AddressComponentTestValue>;

namespace {

TEST(AutofillStructuredAddress, ParseStreetAddress) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      {.street_address = "Erika-Mann-Str. 33",
       .street_name = "Erika-Mann-Str.",
       .house_number = "33"},
      {.street_address = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "Implerstr. 73a Obergeschoss 2 Wohnung 3",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2",
       .apartment = "3"},
      {.street_address = "Implerstr. 73a OG 2",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2"},
      {.street_address = "Implerstr. 73a 2. OG",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2"},
      {.street_address = "Implerstr. no 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "Implerstr. °73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "Implerstr. number 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "1600 Amphitheatre Parkway",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600"},
      {.street_address = "1600 Amphitheatre Parkway, Floor 6 Apt 12",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600",
       .floor = "6",
       .apartment = "12"},
      {.street_address = "Av. Paulista, 1098, 1º andar, apto. 101",
       .street_name = "Av. Paulista",
       .house_number = "1098",
       .floor = "1",
       .apartment = "101"},
      // Examples for Mexico.
      {.street_address = "Street Name 12 - Piso 13 - 14",
       .street_name = "Street Name",
       .house_number = "12",
       .floor = "13",
       .apartment = "14"},
      {.street_address = "Street Name 12 - 14",
       .street_name = "Street Name",
       .house_number = "12",
       .floor = "",
       .apartment = "14"},
      {.street_address = "Street Name 12 - Piso 13",
       .street_name = "Street Name",
       .house_number = "12",
       .floor = "13",
       .apartment = ""},
      // Examples for Spain.
      {.street_address = "Street Name 1, 2º, 3ª",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "2",
       .apartment = "3"},
      {.street_address = "Street Name 1, 2º",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "2",
       .apartment = ""},
      {.street_address = "Street Name 1, 3ª",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "",
       .apartment = "3"},
  };

  for (const auto& test_case : test_cases)
    TestAddressLineParsing(test_case);
}

TEST(AutofillStructuredAddress, ParseMultiLineStreetAddress) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      {.street_address = "Implerstr. 73a\nObergeschoss 2 Wohnung 3",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2",
       .apartment = "3"},
      {.street_address = "Implerstr. 73a\nSome Unparsable Text",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "1600 Amphitheatre Parkway\nFloor 6 Apt 12",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600",
       .floor = "6",
       .apartment = "12"},
      {.street_address = "1600 Amphitheatre Parkway\nSome UnparsableText",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600"},
      {.street_address = "Av. Paulista, 1098\n1º andar, apto. 101",
       .street_name = "Av. Paulista",
       .house_number = "1098",
       .floor = "1",
       .apartment = "101"}};

  for (const auto& test_case : test_cases)
    TestAddressLineParsing(test_case);
}

TEST(AutofillStructuredAddress, TestStreetAddressFormatting) {
  AddressNode address;

  std::vector<AddressLineParsingTestCase> test_cases = {
      {
          .country_code = "BR",
          .street_address = "Av. Brigadeiro Faria Lima, 3477, 1º andar, apto 2",
          .street_name = "Av. Brigadeiro Faria Lima",
          .house_number = "3477",
          .floor = "1",
          .apartment = "2",
      },
      {.country_code = "DE",
       .street_address = "Erika-Mann-Str. 33",
       .street_name = "Erika-Mann-Str.",
       .house_number = "33"},
      {.country_code = "DE",
       .street_address = "Erika-Mann-Str. 33, 2. Stock, 12. Wohnung",
       .street_name = "Erika-Mann-Str.",
       .house_number = "33",
       .floor = "2",
       .apartment = "12"},
      {.street_address = "1600 Amphitheatre Parkway FL 6 APT 12",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600",
       .floor = "6",
       .apartment = "12"},
      // Examples for Mexico.
      {.country_code = "MX",
       .street_address = "StreetName 12 - Piso 13 - 14",
       .street_name = "StreetName",
       .house_number = "12",
       .floor = "13",
       .apartment = "14",
       .landmark = "Red tree",
       .between_streets = "Via Blanca y Rotaria",
       .admin_level_2 = "Guanajuato"},
      {.country_code = "MX",
       .street_address = "StreetName 12 - 14",
       .street_name = "StreetName",
       .house_number = "12",
       .floor = "",
       .apartment = "14",
       .landmark = "Old house",
       .between_streets = "Marcos y Oliva",
       .admin_level_2 = "Oaxaca"},
      {.country_code = "MX",
       .street_address = "StreetName 12 - Piso 13",
       .street_name = "StreetName",
       .house_number = "12",
       .floor = "13",
       .apartment = "",
       .landmark = "Pine in the corner",
       .between_streets = "Rosario y Alfonso",
       .admin_level_2 = "Puebla"},
      // Examples for Spain.
      {.country_code = "ES",
       .street_address = "Street Name 1, 3ª",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "",
       .apartment = "3"},
      {.country_code = "ES",
       .street_address = "Street Name 1, 2º",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "2",
       .apartment = ""},
      {.country_code = "ES",
       .street_address = "Street Name 1, 2º, 3ª",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "2",
       .apartment = "3"},
  };

  for (const auto& test_case : test_cases)
    TestAddressLineFormatting(test_case);
}

// Test setting the first address line.
TEST(AutofillStructuredAddress, TestSettingsAddressLine1) {
  AddressNode address;
  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_LINE1,
       .value = "line1",
       .status = VerificationStatus::kObserved}};

  SetTestValues(&address, test_values);

  AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_LINE1,
       .value = "line1",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1",
       .status = VerificationStatus::kObserved}};

  VerifyTestValues(&address, expectation);
}

// Test settings all three address lines.
TEST(AutofillStructuredAddress, TestSettingsAddressLines) {
  AddressNode address;
  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_LINE1,
       .value = "line1",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LINE2,
       .value = "line2",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LINE3,
       .value = "line3",
       .status = VerificationStatus::kObserved}};

  SetTestValues(&address, test_values);

  AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_LINE1,
       .value = "line1",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LINE2,
       .value = "line2",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LINE3,
       .value = "line3",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1\nline2\nline3",
       .status = VerificationStatus::kObserved}};

  VerifyTestValues(&address, expectation);
}

// Test setting the home street address and retrieving the address lines.
TEST(AutofillStructuredAddress, TestGettingAddressLines) {
  AddressNode address;
  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1\nline2\nline3",
       .status = VerificationStatus::kObserved}};

  SetTestValues(&address, test_values);

  AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_LINE1,
       .value = "line1",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LINE2,
       .value = "line2",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LINE3,
       .value = "line3",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1\nline2\nline3",
       .status = VerificationStatus::kObserved}};

  VerifyTestValues(&address, expectation);
}

// Test setting the home street address and retrieving the address lines.
TEST(AutofillStructuredAddress, TestGettingAddressLines_JoinedAdditionalLines) {
  AddressNode address;
  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1\nline2\nline3\nline4",
       .status = VerificationStatus::kObserved}};

  SetTestValues(&address, test_values);

  AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_LINE1,
       .value = "line1",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LINE2,
       .value = "line2",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LINE3,
       .value = "line3 line4",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1\nline2\nline3\nline4",
       .status = VerificationStatus::kObserved}};

  VerifyTestValues(&address, expectation);
}

// Tests that a structured address gets successfully migrated and subsequently
// completed.
TEST(AutofillStructuredAddress, TestMigrationAndFinalization) {
  AddressNode address;
  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "123 Street name",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_COUNTRY,
       .value = "US",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_STATE,
       .value = "CA",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_LANDMARK,
       .value = "Red tree",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_BETWEEN_STREETS,
       .value = "Rosario y Alfonso",
       .status = VerificationStatus::kNoStatus}};

  SetTestValues(&address, test_values, /*finalize=*/false);

  // Invoke the migration. This should only change the verification statuses of
  // the set values.
  address.MigrateLegacyStructure();

  AddressComponentTestValues expectation_after_migration = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "123 Street name",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_COUNTRY,
       .value = "US",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STATE,
       .value = "CA",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LANDMARK,
       .value = "Red tree",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_BETWEEN_STREETS,
       .value = "Rosario y Alfonso",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_ADDRESS,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_CITY,
       .value = "",
       .status = VerificationStatus::kNoStatus},
  };

  VerifyTestValues(&address, expectation_after_migration);

  // Complete the address tree and check the expectations.
  address.CompleteFullTree();

  AddressComponentTestValues expectation_after_completion = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "123 Street name",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_COUNTRY,
       .value = "US",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STATE,
       .value = "CA",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LANDMARK,
       .value = "Red tree",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_BETWEEN_STREETS,
       .value = "Rosario y Alfonso",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_ADDRESS,
       .value = "123 Street name CA US Rosario y Alfonso Red tree",
       .status = VerificationStatus::kFormatted},
      {.type = ADDRESS_HOME_CITY,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_STREET_NAME,
       .value = "Street name",
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_HOUSE_NUMBER,
       .value = "123",
       .status = VerificationStatus::kParsed},
  };

  VerifyTestValues(&address, expectation_after_completion);
}

// Tests that the migration does not happen of the root node
// (ADDRESS_HOME_ADDRESS) already has a verification status.
TEST(AutofillStructuredAddress, TestMigrationAndFinalization_AlreadyMigrated) {
  AddressNode address;
  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "123 Street name",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_COUNTRY,
       .value = "US",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_STATE,
       .value = "CA",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_ADDRESS,
       .value = "the address",
       .status = VerificationStatus::kFormatted}};

  SetTestValues(&address, test_values, /*finalize=*/false);

  // Invoke the migration. Since the ADDRESS_HOME_ADDRESS node already has a
  // verification status, the address is considered as already migrated.
  address.MigrateLegacyStructure();

  // Verify that the address was not changed by the migration.
  VerifyTestValues(&address, test_values);
}

// Tests that a valid address structure is not wiped.
TEST(AutofillStructuredAddress,
     TestWipingAnInvalidSubstructure_ValidStructure) {
  AddressNode address;
  AddressComponentTestValues address_with_valid_structure = {
      // This structure is valid because all structured components are contained
      // in the unstructured representation.
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "123 Street name",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_NAME,
       .value = "Street name",
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_HOUSE_NUMBER,
       .value = "123",
       .status = VerificationStatus::kParsed},
  };

  SetTestValues(&address, address_with_valid_structure, /*finalize=*/false);

  EXPECT_FALSE(address.WipeInvalidStructure());
  VerifyTestValues(&address, address_with_valid_structure);
}

// Tests that an invalid address structure is wiped.
TEST(AutofillStructuredAddress,
     TestWipingAnInvalidSubstructure_InValidStructure) {
  AddressNode address;
  AddressComponentTestValues address_with_valid_structure = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "Some other name",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_NAME,
       .value = "Street name",
       .status = VerificationStatus::kParsed},
      // The structure is invalid, because the house number is not contained in
      // the unstructured street address.
      {.type = ADDRESS_HOME_HOUSE_NUMBER,
       .value = "123",
       .status = VerificationStatus::kParsed},
  };

  SetTestValues(&address, address_with_valid_structure, /*finalize=*/false);

  EXPECT_TRUE(address.WipeInvalidStructure());

  AddressComponentTestValues address_with_wiped_structure = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "Some other name",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_NAME,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_HOUSE_NUMBER,
       .value = "",
       .status = VerificationStatus::kNoStatus},
  };
  VerifyTestValues(&address, address_with_wiped_structure);
}

// Test that the correct common country between structured addresses is
// computed.
TEST(AutofillStructuredAddress, TestGetCommonCountry) {
  CountryCodeNode country1(nullptr);
  CountryCodeNode country2(nullptr);

  // No countries set.
  EXPECT_EQ(country1.GetCommonCountry(country2), u"");
  EXPECT_EQ(country2.GetCommonCountry(country1), u"");

  // If exactly one country is set, use it as their common one.
  country1.SetValue(u"AT", VerificationStatus::kObserved);
  EXPECT_EQ(country1.GetCommonCountry(country2), u"AT");
  EXPECT_EQ(country2.GetCommonCountry(country1), u"AT");

  // If both are set to the same value, use it as their common one.
  country2.SetValue(u"AT", VerificationStatus::kObserved);
  EXPECT_EQ(country1.GetCommonCountry(country2), u"AT");
  EXPECT_EQ(country2.GetCommonCountry(country1), u"AT");

  // If both have a different value, there is no common one.
  country2.SetValue(u"DE", VerificationStatus::kObserved);
  EXPECT_EQ(country1.GetCommonCountry(country2), u"");
  EXPECT_EQ(country2.GetCommonCountry(country1), u"");
}

// Tests retrieving a value for comparison for a field type.
TEST(AutofillStructuredAddress, TestGetValueForComparisonForType) {
  CountryCodeNode country_code(nullptr);
  country_code.SetValue(u"US", VerificationStatus::kObserved);
  StreetAddressNode street_address(&country_code);
  EXPECT_TRUE(street_address.SetValueForType(ADDRESS_HOME_STREET_ADDRESS,
                                             u"Main Street\nOther Street",
                                             VerificationStatus::kObserved));
  EXPECT_EQ(street_address.GetValueForComparisonForType(
                ADDRESS_HOME_STREET_ADDRESS, street_address),
            u"main st other st");
  EXPECT_EQ(street_address.GetValueForComparisonForType(ADDRESS_HOME_LINE1,
                                                        street_address),
            u"main st");
  EXPECT_EQ(street_address.GetValueForComparisonForType(ADDRESS_HOME_LINE2,
                                                        street_address),
            u"other st");
  EXPECT_TRUE(
      street_address
          .GetValueForComparisonForType(ADDRESS_HOME_LINE3, street_address)
          .empty());
}

struct HasNewerStreetAddressPrecedenceInMergingTestCase {
  // State and parameterization of feature
  // `kAutofillConvergeToExtremeLengthStreetAddress`.
  enum class FeatureState {
    kDisabled = 0,
    kShorter = 1,
    kLonger = 2
  } feature_state;
  std::u16string old_street_address_name, new_street_address_name;
  VerificationStatus old_street_address_status, new_street_address_status;
  bool expect_newer_precedence;
};

class HasNewerStreetAddressPrecedenceInMergingTest
    : public testing::TestWithParam<
          HasNewerStreetAddressPrecedenceInMergingTestCase> {
 public:
  HasNewerStreetAddressPrecedenceInMergingTest() {
    using TestFeatureState =
        HasNewerStreetAddressPrecedenceInMergingTestCase::FeatureState;
    HasNewerStreetAddressPrecedenceInMergingTestCase test_case = GetParam();
    if (test_case.feature_state != TestFeatureState::kDisabled) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kAutofillConvergeToExtremeLengthStreetAddress,
          {{features::kAutofillConvergeToLonger.name,
            test_case.feature_state == TestFeatureState::kLonger ? "true"
                                                                 : "false"}});
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kAutofillConvergeToExtremeLengthStreetAddress);
    }
    country_code_ = std::make_unique<CountryCodeNode>(nullptr);
    country_code_->SetValue(u"US", VerificationStatus::kParsed);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<CountryCodeNode> country_code_;
};

// Tests the logging of which street name (old or new) was chosen during merging
// when the feature `kAutofillConvergeToExtremeLengthStreetAddress` is enabled.
TEST_P(HasNewerStreetAddressPrecedenceInMergingTest,
       HasNewerStreetAddressPrecedenceInMergingTestCase) {
  HasNewerStreetAddressPrecedenceInMergingTestCase test_case = GetParam();
  StreetAddressNode old_street(country_code_.get());
  StreetAddressNode new_street(country_code_.get());
  old_street.SetValue(test_case.old_street_address_name,
                      test_case.old_street_address_status);
  new_street.SetValue(test_case.new_street_address_name,
                      test_case.new_street_address_status);

  old_street.MergeWithComponent(new_street);
  EXPECT_EQ(old_street.GetValue() == new_street.GetValue(),
            test_case.expect_newer_precedence);
}

INSTANTIATE_TEST_SUITE_P(
    StreetAddressConvergenceTest,
    HasNewerStreetAddressPrecedenceInMergingTest,
    testing::Values(
        // When the feature is disabled, always prefer the newer value.
        HasNewerStreetAddressPrecedenceInMergingTestCase{
            .feature_state = HasNewerStreetAddressPrecedenceInMergingTestCase::
                FeatureState::kDisabled,
            .old_street_address_name = u"205 Main Street",
            .new_street_address_name = u"205 Main St",
            .old_street_address_status = VerificationStatus::kParsed,
            .new_street_address_status = VerificationStatus::kParsed,
            .expect_newer_precedence = true},
        HasNewerStreetAddressPrecedenceInMergingTestCase{
            .feature_state = HasNewerStreetAddressPrecedenceInMergingTestCase::
                FeatureState::kDisabled,
            .old_street_address_name = u"205 Main St",
            .new_street_address_name = u"205 Main Street",
            .old_street_address_status = VerificationStatus::kParsed,
            .new_street_address_status = VerificationStatus::kParsed,
            .expect_newer_precedence = true},
        HasNewerStreetAddressPrecedenceInMergingTestCase{
            .feature_state = HasNewerStreetAddressPrecedenceInMergingTestCase::
                FeatureState::kDisabled,
            .old_street_address_name = u"205 Main St",
            .new_street_address_name = u"205 Main Street",
            .old_street_address_status = VerificationStatus::kUserVerified,
            .new_street_address_status = VerificationStatus::kParsed,
            .expect_newer_precedence = false},
        // Converge to longer --> prefer the new one.
        HasNewerStreetAddressPrecedenceInMergingTestCase{
            .feature_state = HasNewerStreetAddressPrecedenceInMergingTestCase::
                FeatureState::kLonger,
            .old_street_address_name = u"205 Main St",
            .new_street_address_name = u"205 Main Street",
            .old_street_address_status = VerificationStatus::kParsed,
            .new_street_address_status = VerificationStatus::kParsed,
            .expect_newer_precedence = true},
        // Converge to longer --> prefer the old one.
        HasNewerStreetAddressPrecedenceInMergingTestCase{
            .feature_state = HasNewerStreetAddressPrecedenceInMergingTestCase::
                FeatureState::kLonger,
            .old_street_address_name = u"205 Main Street",
            .new_street_address_name = u"205 Main St",
            .old_street_address_status = VerificationStatus::kParsed,
            .new_street_address_status = VerificationStatus::kParsed,
            .expect_newer_precedence = false},
        // Converge to longer, but prefer the new one, having better status.
        HasNewerStreetAddressPrecedenceInMergingTestCase{
            .feature_state = HasNewerStreetAddressPrecedenceInMergingTestCase::
                FeatureState::kLonger,
            .old_street_address_name = u"205 Main Street",
            .new_street_address_name = u"205 Main St",
            .old_street_address_status = VerificationStatus::kParsed,
            .new_street_address_status = VerificationStatus::kUserVerified,
            .expect_newer_precedence = true},
        // Converge to shorter --> prefer the new one.
        HasNewerStreetAddressPrecedenceInMergingTestCase{
            .feature_state = HasNewerStreetAddressPrecedenceInMergingTestCase::
                FeatureState::kShorter,
            .old_street_address_name = u"205 Main Street",
            .new_street_address_name = u"205 Main St",
            .old_street_address_status = VerificationStatus::kParsed,
            .new_street_address_status = VerificationStatus::kParsed,
            .expect_newer_precedence = true},
        // Converge to shorter --> prefer the old one.
        HasNewerStreetAddressPrecedenceInMergingTestCase{
            .feature_state = HasNewerStreetAddressPrecedenceInMergingTestCase::
                FeatureState::kShorter,
            .old_street_address_name = u"205 Main St",
            .new_street_address_name = u"205 Main Street",
            .old_street_address_status = VerificationStatus::kParsed,
            .new_street_address_status = VerificationStatus::kParsed,
            .expect_newer_precedence = false},
        // Converge to shorter, but prefer the old one, having better status.
        HasNewerStreetAddressPrecedenceInMergingTestCase{
            .feature_state = HasNewerStreetAddressPrecedenceInMergingTestCase::
                FeatureState::kShorter,
            .old_street_address_name = u"205 Main Street",
            .new_street_address_name = u"205 Main St",
            .old_street_address_status = VerificationStatus::kUserVerified,
            .new_street_address_status = VerificationStatus::kParsed,
            .expect_newer_precedence = false},
        // Equivalent post rewriting, same status, same length --> prefer the
        // old one.
        HasNewerStreetAddressPrecedenceInMergingTestCase{
            .feature_state = HasNewerStreetAddressPrecedenceInMergingTestCase::
                FeatureState::kShorter,
            .old_street_address_name = u"205 Main Street Av",
            .new_street_address_name = u"205 Main St Avenue",
            .old_street_address_status = VerificationStatus::kParsed,
            .new_street_address_status = VerificationStatus::kParsed,
            .expect_newer_precedence = false}));

struct MergeStatesWithCanonicalNamesTestCase {
  std::string older_state;
  VerificationStatus older_status;
  std::string newer_state;
  VerificationStatus newer_status;
  std::string expectation;
  bool is_mergeable;
};

class MergeStatesWithCanonicalNamesTest
    : public testing::Test,
      public testing::WithParamInterface<
          MergeStatesWithCanonicalNamesTestCase> {
 private:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillUseAlternativeStateNameMap);

    AlternativeStateNameMap::GetInstance()
        ->ClearAlternativeStateNameMapForTesting();

    autofill::test::PopulateAlternativeStateNameMapForTesting(
        "XX", "CS",
        {{.canonical_name = "CanonicalState",
          .abbreviations = {"AS"},
          .alternative_names = {"CoolState"}}});
    autofill::test::PopulateAlternativeStateNameMapForTesting(
        "XX", "OS",
        {{.canonical_name = "OtherState",
          .abbreviations = {"OS"},
          .alternative_names = {""}}});
  }

  base::test::ScopedFeatureList feature_list_;
};

// Test that the correct country for merging structured addresses is computed.
TEST_P(MergeStatesWithCanonicalNamesTest, MergeTest) {
  MergeStatesWithCanonicalNamesTestCase test_case = GetParam();

  AddressComponentTestValues older_values = {
      {.type = ADDRESS_HOME_COUNTRY,
       .value = "XX",
       .status = VerificationStatus::kUserVerified},
      {.type = ADDRESS_HOME_STATE,
       .value = test_case.older_state,
       .status = test_case.older_status},
  };

  AddressComponentTestValues newer_values = {
      {.type = ADDRESS_HOME_COUNTRY,
       .value = "XX",
       .status = VerificationStatus::kUserVerified},
      {.type = ADDRESS_HOME_STATE,
       .value = test_case.newer_state,
       .status = test_case.newer_status},
  };

  // In the expectations it is already assumed that the higher
  // verification status should always win.
  AddressComponentTestValues expectation_values = {
      {.type = ADDRESS_HOME_COUNTRY,
       .value = "XX",
       .status = VerificationStatus::kUserVerified},
      {.type = ADDRESS_HOME_STATE,
       .value = test_case.expectation,
       .status = IsLessSignificantVerificationStatus(test_case.older_status,
                                                     test_case.newer_status)
                     ? test_case.newer_status
                     : test_case.older_status},
  };

  AddressNode older_address;
  SetTestValues(&older_address, older_values);

  AddressNode newer_address;
  SetTestValues(&newer_address, newer_values);

  EXPECT_EQ(test_case.is_mergeable,
            older_address.IsMergeableWithComponent(newer_address));

  AddressNode expectation_address;
  SetTestValues(&expectation_address, expectation_values);

  older_address.MergeWithComponent(newer_address);
  EXPECT_TRUE(older_address.SameAs(expectation_address));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillStructuredAddress,
    MergeStatesWithCanonicalNamesTest,
    ::testing::Values(

        // Both have the same canonical name but the older one has the better
        // status and should win in the merge.
        MergeStatesWithCanonicalNamesTestCase{
            "CanonicalState", VerificationStatus::kUserVerified, "CoolState",
            VerificationStatus::kParsed, "CanonicalState", true},

        // Both have the same canonical name but the newer one has the better
        // status and should win in the merge.
        MergeStatesWithCanonicalNamesTestCase{
            "CanonicalState", VerificationStatus::kObserved, "CoolState",
            VerificationStatus::kUserVerified, "CoolState", true},

        // The newer one has no canonical name but the value is a substring of
        // the older one. The older has a higher status and should win.
        MergeStatesWithCanonicalNamesTestCase{
            "CanonicalState", VerificationStatus::kUserVerified, "state",
            VerificationStatus::kParsed, "CanonicalState", true},

        // The other way round: Now the old one remains because it is a
        // substring and has the better status.
        MergeStatesWithCanonicalNamesTestCase{
            "state", VerificationStatus::kUserVerified, "CanonicalState",
            VerificationStatus::kParsed, "state", true},

        // Those two are not mergeable but both have a canonical name.
        MergeStatesWithCanonicalNamesTestCase{
            "CanonicalState", VerificationStatus::kUserVerified, "OtherState",
            VerificationStatus::kParsed, "CanonicalState", false},

        // Here the newer one does not have a canonical test.
        MergeStatesWithCanonicalNamesTestCase{
            "CanonicalState", VerificationStatus::kUserVerified, "Random",
            VerificationStatus::kParsed, "CanonicalState", false}));

}  // namespace

}  // namespace autofill
