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
#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"
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
  std::string street_location;
  std::string street_name;
  std::string house_number;
  std::string subpremise;
  std::string overflow_and_landmark;
  std::string floor;
  std::string apartment;
  std::string apartment_type;
  std::string apartment_num;
  std::string overflow;
  std::string landmark;
  std::string between_streets;
  std::string admin_level_2;
  std::string cross_streets;
  std::string cross_streets_1;
  std::string cross_streets_2;
};

std::ostream& operator<<(std::ostream& out,
                         const AddressLineParsingTestCase& test_case) {
  out << "Country: " << test_case.country_code << std::endl;
  out << "Street address: " << test_case.street_address << std::endl;
  out << "Street location: " << test_case.street_location << std::endl;
  out << "Street name: " << test_case.street_name << std::endl;
  out << "House number: " << test_case.house_number << std::endl;
  out << "Floor: " << test_case.floor << std::endl;
  out << "Apartment: " << test_case.apartment << std::endl;
  out << "Overflow: " << test_case.overflow << std::endl;
  out << "Landmark: " << test_case.landmark << std::endl;
  out << "Between streets: " << test_case.between_streets << std::endl;
  out << "Admin level 2: " << test_case.admin_level_2 << std::endl;
  out << "Subpremise: " << test_case.subpremise << std::endl;
  out << "Cross streets: " << test_case.cross_streets << std::endl;
  out << "Cross streets 1: " << test_case.cross_streets_1 << std::endl;
  out << "Cross streets 2: " << test_case.cross_streets_2 << std::endl;
  return out;
}

class AutofillStructuredAddress : public testing::Test {
 public:
  AutofillStructuredAddress() {
    features_.InitWithFeatures(
        {features::kAutofillEnableSupportForAdminLevel2,
         features::kAutofillEnableSupportForApartmentNumbers},
        {});
  }

 private:
  base::test::ScopedFeatureList features_;
};

void TestAddressLineParsing(const AddressLineParsingTestCase& test_case) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel();
  const AddressComponentTestValues test_value = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = test_case.street_address,
       .status = VerificationStatus::kObserved}};

  SetTestValues(address.get(), test_value);

  SCOPED_TRACE(test_case);

  const AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_COUNTRY,
       .value = test_case.country_code,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = test_case.street_address,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_LOCATION,
       .value = test_case.street_location,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_STREET_NAME,
       .value = test_case.street_name,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_HOUSE_NUMBER,
       .value = test_case.house_number,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_APT,
       .value = test_case.apartment,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_APT_NUM,
       .value = test_case.apartment_num,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_APT_TYPE,
       .value = test_case.apartment_type,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_FLOOR,
       .value = test_case.floor,
       .status = VerificationStatus::kParsed}};
  VerifyTestValues(address.get(), expectation);
}

void TestAddressLineFormatting(const AddressLineParsingTestCase& test_case) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel();
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
       .value = test_case.apartment_num,
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

  SetTestValues(address.get(), test_value);

  SCOPED_TRACE(test_case);

  const AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_COUNTRY,
       .value = test_case.country_code,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = test_case.street_address,
       .status = VerificationStatus::kFormatted},
      {.type = ADDRESS_HOME_STREET_LOCATION,
       .value = test_case.street_location,
       .status = VerificationStatus::kFormatted},
      {.type = ADDRESS_HOME_STREET_NAME,
       .value = test_case.street_name,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_HOUSE_NUMBER,
       .value = test_case.house_number,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_APT_NUM,
       .value = test_case.apartment_num,
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
  VerifyTestValues(address.get(), expectation);
}

using AddressComponentTestValues = std::vector<AddressComponentTestValue>;

namespace {

TEST_F(AutofillStructuredAddress, ParseStreetAddress) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      {.street_address = "Erika-Mann-Str. 33",
       .street_location = "Erika-Mann-Str. 33",
       .street_name = "Erika-Mann-Str.",
       .house_number = "33"},
      {.street_address = "Implerstr. 73a",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "Implerstr. 73a Obergeschoss 2 Wohnung 3",
       .street_location = "Implerstr. 73a ",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2",
       .apartment_num = "3"},
      {.street_address = "Implerstr. 73a OG 2",
       .street_location = "Implerstr. 73a ",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2"},
      {.street_address = "Implerstr. 73a 2. OG",
       .street_location = "Implerstr. 73a ",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2"},
      {.street_address = "Implerstr. no 73a",
       .street_location = "Implerstr. no 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "Implerstr. °73a",
       .street_location = "Implerstr. °73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "Implerstr. number 73a",
       .street_location = "Implerstr. number 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "1600 Amphitheatre Parkway",
       .street_location = "1600 Amphitheatre Parkway",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600"},
      {.street_address = "1600 Amphitheatre Parkway, Floor 6 Apt 12",
       .street_location = "1600 Amphitheatre Parkway, ",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600",
       .floor = "6",
       .apartment_num = "12"},
      {.street_address = "Av. Paulista, 1098, 1º andar, apto. 101",
       .street_location = "Av. Paulista, 1098, ",
       .street_name = "Av. Paulista",
       .house_number = "1098",
       .floor = "1",
       .apartment_num = "101"},
      // Examples for Mexico.
      {.street_address = "Street Name 12 - Piso 13 - 14",
       .street_location = "Street Name 12 ",
       .street_name = "Street Name",
       .house_number = "12",
       .floor = "13",
       .apartment_num = "14"},
      {.street_address = "Street Name 12 - 14",
       .street_location = "Street Name 12 ",
       .street_name = "Street Name",
       .house_number = "12",
       .floor = "",
       .apartment_num = "14"},
      {.street_address = "Street Name 12 - Piso 13",
       .street_location = "Street Name 12 ",
       .street_name = "Street Name",
       .house_number = "12",
       .floor = "13",
       .apartment_num = ""},
      // Examples for Spain.
      {.street_address = "Street Name 1, 2º, 3ª",
       .street_location = "Street Name 1, ",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "2",
       .apartment_num = "3"},
      {.street_address = "Street Name 1, 2º",
       .street_location = "Street Name 1, ",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "2",
       .apartment_num = ""},
      {.street_address = "Street Name 1, 3ª",
       .street_location = "Street Name 1, ",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "",
       .apartment_num = "3"},
  };

  for (const auto& test_case : test_cases)
    TestAddressLineParsing(test_case);
}

TEST_F(AutofillStructuredAddress, ParseMultiLineStreetAddress) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      {.street_address = "Implerstr. 73a\nObergeschoss 2 Wohnung 3",
       .street_location = "Implerstr. 73a\n",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2",
       .apartment_num = "3"},
      {.street_address = "Implerstr. 73a\nSome Unparsable Text",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "1600 Amphitheatre Parkway\nFloor 6 Apt 12",
       .street_location = "1600 Amphitheatre Parkway\n",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600",
       .floor = "6",
       .apartment_num = "12"},
      {.street_address = "1600 Amphitheatre Parkway\nSome UnparsableText",
       .street_location = "1600 Amphitheatre Parkway",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600"},
      {.street_address = "Av. Paulista, 1098\n1º andar, apto. 101",
       .street_location = "Av. Paulista, 1098\n",
       .street_name = "Av. Paulista",
       .house_number = "1098",
       .floor = "1",
       .apartment_num = "101"}};

  for (const auto& test_case : test_cases)
    TestAddressLineParsing(test_case);
}

TEST_F(AutofillStructuredAddress, TestStreetAddressFormatting) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      {
          .country_code = "BR",
          .street_address = "Av. Brigadeiro Faria Lima, 3477, 1º andar, apto 2",
          .street_location = "Av. Brigadeiro Faria Lima 3477",
          .street_name = "Av. Brigadeiro Faria Lima",
          .house_number = "3477",
          .floor = "1",
          .apartment_num = "2",
      },
      {.country_code = "DE",
       .street_address = "Erika-Mann-Str. 33",
       .street_location = "Erika-Mann-Str. 33",
       .street_name = "Erika-Mann-Str.",
       .house_number = "33"},
      {.country_code = "DE",
       .street_address = "Erika-Mann-Str. 33, 2. Stock, 12. Wohnung",
       .street_location = "Erika-Mann-Str. 33",
       .street_name = "Erika-Mann-Str.",
       .house_number = "33",
       .floor = "2",
       .apartment_num = "12"},
      {.street_address = "1600 Amphitheatre Parkway FL 6 APT 12",
       .street_location = "Amphitheatre Parkway 1600",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600",
       .floor = "6",
       .apartment_num = "12"},
      // Examples for Mexico.
      {.country_code = "MX",
       .street_address = "StreetName 12 - Piso 13 - 14",
       .street_location = "StreetName 12",
       .street_name = "StreetName",
       .house_number = "12",
       .floor = "13",
       .apartment_num = "14",
       .admin_level_2 = "Guanajuato"},
      {.country_code = "MX",
       .street_address = "StreetName 12 - 14",
       .street_location = "StreetName 12",
       .street_name = "StreetName",
       .house_number = "12",
       .floor = "",
       .apartment_num = "14",
       .admin_level_2 = "Oaxaca"},
      {.country_code = "MX",
       .street_address = "StreetName 12 - Piso 13",
       .street_location = "StreetName 12",
       .street_name = "StreetName",
       .house_number = "12",
       .floor = "13",
       .apartment_num = "",
       .admin_level_2 = "Puebla"},
      // Examples for Spain.
      {.country_code = "ES",
       .street_address = "Street Name 1, 3ª",
       .street_location = "Street Name 1",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "",
       .apartment_num = "3"},
      {.country_code = "ES",
       .street_address = "Street Name 1, 2º",
       .street_location = "Street Name 1",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "2",
       .apartment_num = ""},
      {.country_code = "ES",
       .street_address = "Street Name 1, 2º, 3ª",
       .street_location = "Street Name 1",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "2",
       .apartment_num = "3"},
  };

  for (const auto& test_case : test_cases)
    TestAddressLineFormatting(test_case);
}

// Test setting the first address line.
TEST_F(AutofillStructuredAddress, TestSettingsAddressLine1) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel();

  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_LINE1,
       .value = "line1",
       .status = VerificationStatus::kObserved}};

  SetTestValues(address.get(), test_values);

  AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_LINE1,
       .value = "line1",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1",
       .status = VerificationStatus::kObserved}};

  VerifyTestValues(address.get(), expectation);
}

// Test settings all three address lines.
TEST_F(AutofillStructuredAddress, TestSettingsAddressLines) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel();
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

  SetTestValues(address.get(), test_values);

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

  VerifyTestValues(address.get(), expectation);
}

// Test setting the home street address and retrieving the address lines.
TEST_F(AutofillStructuredAddress, TestGettingAddressLines) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1\nline2\nline3",
       .status = VerificationStatus::kObserved}};

  SetTestValues(address.get(), test_values);

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

  VerifyTestValues(address.get(), expectation);
}

// Test setting the home street address and retrieving the address lines.
TEST_F(AutofillStructuredAddress,
       TestGettingAddressLines_JoinedAdditionalLines) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1\nline2\nline3\nline4",
       .status = VerificationStatus::kObserved}};

  SetTestValues(address.get(), test_values);

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

  VerifyTestValues(address.get(), expectation);
}

// Tests that a structured address gets successfully migrated and subsequently
// completed.
TEST_F(AutofillStructuredAddress, TestMigrationAndFinalization) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "123 Street name",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_COUNTRY,
       .value = "US",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_STATE,
       .value = "CA",
       .status = VerificationStatus::kNoStatus}};

  SetTestValues(address.get(), test_values, /*finalize=*/false);

  // Invoke the migration. This should only change the verification statuses of
  // the set values.
  address->MigrateLegacyStructure();

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
      {.type = ADDRESS_HOME_ADDRESS,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_CITY,
       .value = "",
       .status = VerificationStatus::kNoStatus},
  };

  VerifyTestValues(address.get(), expectation_after_migration);

  // Complete the address tree and check the expectations.
  address->CompleteFullTree();

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
      {.type = ADDRESS_HOME_ADDRESS,
       .value = "123 Street name CA US",
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

  VerifyTestValues(address.get(), expectation_after_completion);
}

// Tests that the migration does not happen of the root node
// (ADDRESS_HOME_ADDRESS) already has a verification status.
TEST_F(AutofillStructuredAddress,
       TestMigrationAndFinalization_AlreadyMigrated) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel();
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

  SetTestValues(address.get(), test_values, /*finalize=*/false);

  // Invoke the migration. Since the ADDRESS_HOME_ADDRESS node already has a
  // verification status, the address is considered as already migrated.
  address->MigrateLegacyStructure();

  // Verify that the address was not changed by the migration.
  VerifyTestValues(address.get(), test_values);
}

// Tests that a valid address structure is not wiped.
TEST_F(AutofillStructuredAddress,
       TestWipingAnInvalidSubstructure_ValidStructure) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel();
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

  SetTestValues(address.get(), address_with_valid_structure,
                /*finalize=*/false);

  EXPECT_FALSE(address->WipeInvalidStructure());
  VerifyTestValues(address.get(), address_with_valid_structure);
}

// Tests that an invalid address structure is wiped.
TEST_F(AutofillStructuredAddress,
       TestWipingAnInvalidSubstructure_InValidStructure) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponentTestValues address_with_valid_structure = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "Some other name",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_LOCATION,
       .value = "Street name 123",
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_STREET_NAME,
       .value = "Street name",
       .status = VerificationStatus::kParsed},
      // The structure is invalid, because the house number is not contained in
      // the unstructured street address.
      {.type = ADDRESS_HOME_HOUSE_NUMBER,
       .value = "123",
       .status = VerificationStatus::kParsed},
  };

  SetTestValues(address.get(), address_with_valid_structure,
                /*finalize=*/false);

  EXPECT_TRUE(address->WipeInvalidStructure());

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
  VerifyTestValues(address.get(), address_with_wiped_structure);
}

// Test that the correct common country between structured addresses is
// computed.
TEST_F(AutofillStructuredAddress, TestGetCommonCountry) {
  std::unique_ptr<AddressComponent> address1 =
      i18n_model_definition::CreateAddressComponentModel();
  std::unique_ptr<AddressComponent> address2 =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponent* country1 =
      address1->GetNodeForTypeForTesting(ADDRESS_HOME_COUNTRY);
  AddressComponent* country2 =
      address2->GetNodeForTypeForTesting(ADDRESS_HOME_COUNTRY);

  // No countries set.
  EXPECT_EQ(country1->GetCommonCountry(*country2), u"");
  EXPECT_EQ(country2->GetCommonCountry(*country1), u"");

  // If exactly one country is set, use it as their common one.
  country1->SetValue(u"AT", VerificationStatus::kObserved);
  EXPECT_EQ(country1->GetCommonCountry(*country2), u"AT");
  EXPECT_EQ(country2->GetCommonCountry(*country1), u"AT");

  // If both are set to the same value, use it as their common one.
  country2->SetValue(u"AT", VerificationStatus::kObserved);
  EXPECT_EQ(country1->GetCommonCountry(*country2), u"AT");
  EXPECT_EQ(country2->GetCommonCountry(*country1), u"AT");

  // If both have a different value, there is no common one.
  country2->SetValue(u"DE", VerificationStatus::kObserved);
  EXPECT_EQ(country1->GetCommonCountry(*country2), u"");
  EXPECT_EQ(country2->GetCommonCountry(*country1), u"");
}

// Tests retrieving a value for comparison for a field type.
TEST_F(AutofillStructuredAddress, TestGetValueForComparisonForType) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponent* country_code =
      address->GetNodeForTypeForTesting(ADDRESS_HOME_COUNTRY);
  country_code->SetValue(u"US", VerificationStatus::kObserved);

  AddressComponent* street_address =
      address->GetNodeForTypeForTesting(ADDRESS_HOME_STREET_ADDRESS);
  EXPECT_TRUE(street_address->SetValueForType(ADDRESS_HOME_STREET_ADDRESS,
                                              u"Main Street\nOther Street",
                                              VerificationStatus::kObserved));
  EXPECT_EQ(street_address->GetValueForComparisonForType(
                ADDRESS_HOME_STREET_ADDRESS, *street_address),
            u"main st other st");
  EXPECT_EQ(street_address->GetValueForComparisonForType(ADDRESS_HOME_LINE1,
                                                         *street_address),
            u"main st");
  EXPECT_EQ(street_address->GetValueForComparisonForType(ADDRESS_HOME_LINE2,
                                                         *street_address),
            u"other st");
  EXPECT_TRUE(
      street_address
          ->GetValueForComparisonForType(ADDRESS_HOME_LINE3, *street_address)
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
    root_old_node_ = i18n_model_definition::CreateAddressComponentModel();
    root_old_node_->SetValueForType(ADDRESS_HOME_COUNTRY, u"US",
                                    VerificationStatus::kParsed);
    root_new_node_ = i18n_model_definition::CreateAddressComponentModel();
    root_new_node_->SetValueForType(ADDRESS_HOME_COUNTRY, u"US",
                                    VerificationStatus::kParsed);
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
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AddressComponent> root_old_node_;
  std::unique_ptr<AddressComponent> root_new_node_;
};

// Tests the logging of which street name (old or new) was chosen during merging
// when the feature `kAutofillConvergeToExtremeLengthStreetAddress` is enabled.
TEST_P(HasNewerStreetAddressPrecedenceInMergingTest,
       HasNewerStreetAddressPrecedenceInMergingTestCase) {
  HasNewerStreetAddressPrecedenceInMergingTestCase test_case = GetParam();
  auto* old_street =
      root_old_node_->GetNodeForTypeForTesting(ADDRESS_HOME_STREET_ADDRESS);
  auto* new_street =
      root_new_node_->GetNodeForTypeForTesting(ADDRESS_HOME_STREET_ADDRESS);
  old_street->SetValue(test_case.old_street_address_name,
                       test_case.old_street_address_status);
  new_street->SetValue(test_case.new_street_address_name,
                       test_case.new_street_address_status);

  old_street->MergeWithComponent(*new_street);
  EXPECT_EQ(old_street->GetValue() == new_street->GetValue(),
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

  std::unique_ptr<AddressComponent> older_address =
      i18n_model_definition::CreateAddressComponentModel();
  SetTestValues(older_address.get(), older_values);

  std::unique_ptr<AddressComponent> newer_address =
      i18n_model_definition::CreateAddressComponentModel();
  SetTestValues(newer_address.get(), newer_values);

  EXPECT_EQ(test_case.is_mergeable,
            older_address->IsMergeableWithComponent(*newer_address));

  std::unique_ptr<AddressComponent> expectation_address =
      i18n_model_definition::CreateAddressComponentModel();
  SetTestValues(expectation_address.get(), expectation_values);

  older_address->MergeWithComponent(*newer_address);
  EXPECT_TRUE(older_address->SameAs(*expectation_address));
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

class AutofillI18nStructuredAddress : public testing::Test {
 public:
  AutofillI18nStructuredAddress() {
    features_.InitWithFeatures(
        {
            features::kAutofillEnableSupportForLandmark,
            features::kAutofillEnableSupportForBetweenStreets,
            features::kAutofillEnableSupportForAdminLevel2,
            features::kAutofillEnableSupportForApartmentNumbers,
            features::kAutofillEnableSupportForAddressOverflow,
            features::kAutofillEnableSupportForBetweenStreetsOrLandmark,
            features::kAutofillEnableSupportForAddressOverflowAndLandmark,
            features::kAutofillUseI18nAddressModel,
        },
        {});
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(AutofillI18nStructuredAddress, ParseStreetAddressLegacy) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      {.country_code = "",
       .street_address = "Erika-Mann-Str. 33",
       .street_location = "Erika-Mann-Str. 33",
       .street_name = "Erika-Mann-Str.",
       .house_number = "33"},
      {.country_code = "",
       .street_address = "Implerstr. 73a",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.country_code = "",
       .street_address = "Implerstr. 73a Obergeschoss 2 Wohnung 3",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .subpremise = "Obergeschoss 2 Wohnung 3",
       .floor = "2",
       .apartment_num = "3"},
      {.country_code = "",
       .street_address = "Implerstr. 73a OG 2",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .subpremise = "OG 2",
       .floor = "2"},
      {.country_code = "",
       .street_address = "Implerstr. 73a 2. OG",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .subpremise = "2. OG",
       .floor = "2"},
      {.country_code = "",
       .street_address = "Implerstr. no 73a",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.country_code = "",
       .street_address = "1600 Amphitheatre Parkway",
       .street_location = "Amphitheatre Parkway 1600",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600"},
      {.country_code = "",
       .street_address = "1600 Amphitheatre Parkway, Floor 6 Apt 12",
       .street_location = "Amphitheatre Parkway 1600",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600",
       .subpremise = "Floor 6 Apt 12",
       .floor = "6",
       .apartment_num = "12"},
      {.country_code = "",
       .street_address = "Av. Paulista, 1098, 1º andar, apto. 101",
       .street_location = "Av. Paulista 1098",
       .street_name = "Av. Paulista",
       .house_number = "1098",
       .subpremise = "1º andar, apto. 101",
       .floor = "1",
       .apartment_num = "101"},
      {.country_code = "",
       .street_address = "Street Name 12 - Piso 13 - 14",
       .street_location = "Street Name 12",
       .street_name = "Street Name",
       .house_number = "12",
       .subpremise = "- Piso 13 - 14",
       .floor = "13",
       .apartment_num = "14"},
      {.country_code = "",
       .street_address = "Street Name 12 - 14",
       .street_location = "Street Name 12",
       .street_name = "Street Name",
       .house_number = "12",
       .subpremise = "- 14",
       .apartment_num = "14"},
      {.country_code = "",
       .street_address = "Street Name 12 - Piso 13",
       .street_location = "Street Name 12",
       .street_name = "Street Name",
       .house_number = "12",
       .subpremise = "- Piso 13",
       .floor = "13"},
      {.country_code = "",
       .street_address = "Street Name 1, 2º, 3ª",
       .street_location = "Street Name 1",
       .street_name = "Street Name",
       .house_number = "1",
       .subpremise = "2º, 3ª",
       .floor = "2",
       .apartment_num = "3"},
      {.country_code = "",
       .street_address = "Street Name 1, 2º",
       .street_location = "Street Name 1",
       .street_name = "Street Name",
       .house_number = "1",
       .subpremise = "2º",
       .floor = "2"},
      {.country_code = "",
       .street_address = "Street Name 1, 3ª",
       .street_location = "Street Name 1",
       .street_name = "Street Name",
       .house_number = "1",
       .subpremise = "3ª",
       .apartment_num = "3"},
  };

  for (const auto &test_case : test_cases) {
    std::unique_ptr<AddressComponent> address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.get(), test_value);

    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kFormatted},
        {.type = ADDRESS_HOME_STREET_NAME,
         .value = test_case.street_name,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_HOUSE_NUMBER,
         .value = test_case.house_number,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_SUBPREMISE,
         .value = test_case.subpremise,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_APT_NUM,
         .value = test_case.apartment_num,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_FLOOR,
         .value = test_case.floor,
         .status = VerificationStatus::kParsed}};
    VerifyTestValues(address.get(), expectation);
  }
}

TEST_F(AutofillI18nStructuredAddress, ParseStreetAddressMX) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples for Mexico.
      {.country_code = "MX",
       // Street and house number, default case: separated by space
       .street_address = "Avenida Álvaro Obregón 1234",
       .street_location = "Avenida Álvaro Obregón 1234",
       .street_name = "Avenida Álvaro Obregón",
       .house_number = "1234"},
      {.country_code = "MX",
       // Street and house number, separated with #
       .street_address = "Avenida Álvaro Obregón #1234",
       .street_location = "Avenida Álvaro Obregón #1234",
       .street_name = "Avenida Álvaro Obregón",
       .house_number = "1234"},
      {.country_code = "MX",
       // Street and house number, separated with No.
       .street_address = "Avenida Álvaro Obregón No. 1234",
       .street_location = "Avenida Álvaro Obregón No. 1234",
       .street_name = "Avenida Álvaro Obregón",
       .house_number = "1234"},
      {.country_code = "MX",
       // Street and house number, with KM position
       .street_address = "Avenida Álvaro Obregón KM 1234",
       .street_location = "Avenida Álvaro Obregón KM 1234",
       .street_name = "Avenida Álvaro Obregón",
       .house_number = "KM 1234"},
      {.country_code = "MX",
       // Street and house number, without a number
       .street_address = "Avenida Álvaro Obregón S/N",
       .street_location = "Avenida Álvaro Obregón S/N",
       .street_name = "Avenida Álvaro Obregón",
       .house_number = "S/N"},
      {.country_code = "MX",
       .street_address = "Avenida Álvaro Obregón 1234, Apartamento 5A, Piso 10 "
                         "Entre Calles Tonalá y Monterrey",
       .street_location = "Avenida Álvaro Obregón 1234",
       .street_name = "Avenida Álvaro Obregón",
       .house_number = "1234",
       .subpremise = "Apartamento 5A, Piso 10",
       .floor = "10",
       .apartment = "Apartamento 5A",
       .apartment_type = "Apartamento",
       .apartment_num = "5A",
       .overflow = "Entre Calles Tonalá y Monterrey",
       .cross_streets = "Tonalá y Monterrey",
       .cross_streets_1 = "Tonalá",
       .cross_streets_2 = "Monterrey"},
      {.country_code = "MX",
       .street_address = "Avenida Paseo de la Reforma 505 interior 201, piso "
                         "2, entre Río Sena y Río Neva",
       .street_location = "Avenida Paseo de la Reforma 505",
       .street_name = "Avenida Paseo de la Reforma",
       .house_number = "505",
       .subpremise = "interior 201, Piso 2",
       .floor = "2",
       .apartment = "interior 201",
       .apartment_type = "interior",
       .apartment_num = "201",
       .overflow = "Entre Calles Río Sena y Río Neva",
       .cross_streets = "Río Sena y Río Neva",
       .cross_streets_1 = "Río Sena",
       .cross_streets_2 = "Río Neva"},
      {.country_code = "MX",
       .street_address = "Calle 60 Norte, número 262, departamento 3, cerca "
                         "del Rio Bravo, planta baja, entre 35 y 37",
       .street_location = "Calle 60 Norte, número 262",
       .street_name = "Calle 60 Norte",
       .house_number = "262",
       .subpremise = "departamento 3",
       .apartment = "departamento 3",
       .apartment_type = "departamento",
       .apartment_num = "3",
       .overflow = "Entre Calles 35 y 37 Rio Bravo",
       .landmark = " Rio Bravo",
       .cross_streets = "35 y 37",
       .cross_streets_1 = "35",
       .cross_streets_2 = "37"},
  };

  for (const auto& test_case : test_cases) {
    std::unique_ptr<AddressComponent> address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.get(), test_value);

    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_STREET_NAME,
         .value = test_case.street_name,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_HOUSE_NUMBER,
         .value = test_case.house_number,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_SUBPREMISE,
         .value = test_case.subpremise,
         .status = VerificationStatus::kFormatted},
        {.type = ADDRESS_HOME_APT,
         .value = test_case.apartment,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_APT_TYPE,
         .value = test_case.apartment_type,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_APT_NUM,
         .value = test_case.apartment_num,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_FLOOR,
         .value = test_case.floor,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_OVERFLOW,
         .value = test_case.overflow,
         .status = VerificationStatus::kFormatted},
        {.type = ADDRESS_HOME_BETWEEN_STREETS,
         .value = test_case.cross_streets,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_BETWEEN_STREETS_1,
         .value = test_case.cross_streets_1,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_BETWEEN_STREETS_2,
         .value = test_case.cross_streets_2,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_LANDMARK,
         .value = test_case.landmark,
         .status = VerificationStatus::kParsed},
    };
    VerifyTestValues(address.get(), expectation);
  }
}

TEST_F(AutofillI18nStructuredAddress, ParseSubpremiseMX) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel(
          AddressCountryCode("MX"));

  AddressLineParsingTestCase test_case = {.subpremise = "apto 12, piso 1",
                                          .floor = "1",
                                          .apartment = "apto 12",
                                          .apartment_type = "apto",
                                          .apartment_num = "12"};

  const AddressComponentTestValues test_value = {
      {.type = ADDRESS_HOME_SUBPREMISE,
       .value = test_case.subpremise,
       .status = VerificationStatus::kObserved}};

  SetTestValues(address.get(), test_value);

  const AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_SUBPREMISE,
       .value = test_case.subpremise,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_APT,
       .value = test_case.apartment,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_APT_TYPE,
       .value = test_case.apartment_type,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_APT_NUM,
       .value = test_case.apartment_num,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_FLOOR,
       .value = test_case.floor,
       .status = VerificationStatus::kParsed}};
  VerifyTestValues(address.get(), expectation);
}

TEST_F(AutofillI18nStructuredAddress, ParseStreetAddressBR) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples for Brasil.
      {.country_code = "BR",
       // Street and house number, default case: separated by comma.
       .street_address = "Avenida Mem de Sá, 1234",
       .street_location = "Avenida Mem de Sá, 1234",
       .street_name = "Avenida Mem de Sá",
       .house_number = "1234"},
      {.country_code = "BR",
       // Street and house number, default case: separated by -.
       .street_address = "Avenida Mem de Sá - 1234",
       .street_location = "Avenida Mem de Sá - 1234",
       .street_name = "Avenida Mem de Sá",
       .house_number = "1234"},
      {.country_code = "BR",
       // Street and house number, default case: separated by comma with nº
       // prefix.
       .street_address = "Avenida Mem de Sá, nº 1234",
       .street_location = "Avenida Mem de Sá, nº 1234",
       .street_name = "Avenida Mem de Sá",
       .house_number = "1234"},
      {.country_code = "BR",
       // Street and house number, default case: separated by comma with KM
       // position.
       .street_address = "Avenida Mem de Sá, KM 1234",
       .street_location = "Avenida Mem de Sá, KM 1234",
       .street_name = "Avenida Mem de Sá",
       .house_number = "KM 1234"},
      {.country_code = "BR",
       // A full street address.
       .street_address =
           "Avenida Mem de Sá, 1234 apto 12, andar 1\n referência: "
           "foo\n something else",
       .street_location = "Avenida Mem de Sá, 1234",
       .street_name = "Avenida Mem de Sá",
       .house_number = "1234",
       .subpremise = "Andar 1, apto 12",
       .overflow_and_landmark = "Andar 1, apto 12\nPonto de referência: foo",
       .floor = "1",
       .apartment = "apto 12",
       .apartment_type = "apto",
       .apartment_num = "12",
       .overflow = "Andar 1, apto 12",
       .landmark = "foo"},
      {.country_code = "BR",
       // A full street address, v2 (floor in separate row).
       .street_address =
           "Avenida Mem de Sá, 1234\n apto 12\n andar 1\n referência: "
           "foo\n something else",
       .street_location = "Avenida Mem de Sá, 1234",
       .street_name = "Avenida Mem de Sá",
       .house_number = "1234",
       .subpremise = "Andar 1, apto 12",
       .overflow_and_landmark = "Andar 1, apto 12\nPonto de referência: foo",
       .floor = "1",
       .apartment = "apto 12",
       .apartment_type = "apto",
       .apartment_num = "12",
       .overflow = "Andar 1, apto 12",
       .landmark = "foo"},
      {.country_code = "BR",
       // A full street address, v3 (in-building-loation in line 1).
       .street_address = "Avenida Mem de Sá, 1234, andar 1, apto "
                         "12\nreferência: foo\nsomething else",
       .street_location = "Avenida Mem de Sá, 1234",
       .street_name = "Avenida Mem de Sá",
       .house_number = "1234",
       .subpremise = "Andar 1, apto 12",
       .overflow_and_landmark = "Andar 1, apto 12\nPonto de referência: foo",
       .floor = "1",
       .apartment = "apto 12",
       .apartment_type = "apto",
       .apartment_num = "12",
       .overflow = "Andar 1, apto 12",
       .landmark = "foo"},
      {.country_code = "BR",
       // A full street address, v4 (don't discover a street-location from line
       // 2).
       .street_address = "Something else\nAvenida Mem de Sá, 1234, andar 1, "
                         "apto 12\nreferência: foo\nsomething else",
       .subpremise = "Andar 1, apto 12",
       .overflow_and_landmark = "Andar 1, apto 12\nPonto de referência: foo",
       .floor = "1",
       .apartment = "apto 12",
       .apartment_type = "apto",
       .apartment_num = "12",
       .overflow = "Andar 1, apto 12",
       .landmark = "foo"},
  };

  for (const auto& test_case : test_cases) {
    std::unique_ptr<AddressComponent> address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.get(), test_value);

    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_STREET_NAME,
         .value = test_case.street_name,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_HOUSE_NUMBER,
         .value = test_case.house_number,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_APT,
         .value = test_case.apartment,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_APT_TYPE,
         .value = test_case.apartment_type,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_APT_NUM,
         .value = test_case.apartment_num,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_FLOOR,
         .value = test_case.floor,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_LANDMARK,
         .value = test_case.landmark,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_SUBPREMISE,
         .value = test_case.subpremise,
         .status = VerificationStatus::kFormatted},
        {.type = ADDRESS_HOME_OVERFLOW,
         .value = test_case.overflow,
         .status = VerificationStatus::kFormatted},
        {.type = ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
         .value = test_case.overflow_and_landmark,
         .status = VerificationStatus::kFormatted},
    };
    VerifyTestValues(address.get(), expectation);
  }
}

TEST_F(AutofillI18nStructuredAddress, ParseOverflowAndLandmarkBR) {
  std::unique_ptr<AddressComponent> address =
      i18n_model_definition::CreateAddressComponentModel(
          AddressCountryCode("BR"));

  AddressLineParsingTestCase test_case = {
      .overflow_and_landmark =
          "apto 12, 1 andar, referência: foo, something else",
      .floor = "1",
      .apartment = "apto 12",
      .apartment_type = "apto",
      .apartment_num = "12",
      .landmark = "foo"};

  const AddressComponentTestValues test_value = {
      {.type = ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
       .value = test_case.overflow_and_landmark,
       .status = VerificationStatus::kObserved}};

  SetTestValues(address.get(), test_value);

  const AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
       .value = test_case.overflow_and_landmark,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_APT,
       .value = test_case.apartment,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_APT_TYPE,
       .value = test_case.apartment_type,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_APT_NUM,
       .value = test_case.apartment_num,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_FLOOR,
       .value = test_case.floor,
       .status = VerificationStatus::kParsed},
      {.type = ADDRESS_HOME_LANDMARK,
       .value = test_case.landmark,
       .status = VerificationStatus::kParsed}};
  VerifyTestValues(address.get(), expectation);
}

TEST_F(AutofillI18nStructuredAddress, ParseSubpremiseBR) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      {.subpremise = "apto 12, 1 andar",
       .floor = "1",
       .apartment = "apto 12",
       .apartment_type = "apto",
       .apartment_num = "12"},
      {.subpremise = "apto 12, andar 1",
       .floor = "1",
       .apartment = "apto 12",
       .apartment_type = "apto",
       .apartment_num = "12"},
  };

  for (const auto& test_case : test_cases) {
    std::unique_ptr<AddressComponent> address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode("BR"));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_SUBPREMISE,
         .value = test_case.subpremise,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.get(), test_value);

    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_SUBPREMISE,
         .value = test_case.subpremise,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_APT,
         .value = test_case.apartment,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_APT_NUM,
         .value = test_case.apartment_num,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_APT_TYPE,
         .value = test_case.apartment_type,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_FLOOR,
         .value = test_case.floor,
         .status = VerificationStatus::kParsed}};
    VerifyTestValues(address.get(), expectation);
  }
}

}  // namespace

}  // namespace autofill
