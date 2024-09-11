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
#include "components/autofill/core/browser/data_model/autofill_structured_address_component_test_api.h"
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
  std::string building_and_unit;
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
  out << "House number and apartment number: " << test_case.building_and_unit
      << std::endl;
  return out;
}

class AutofillStructuredAddress : public testing::Test {
 public:
  AutofillStructuredAddress() {
    features_.InitWithFeatures({features::kAutofillUseAUAddressModel,
                                features::kAutofillUseCAAddressModel,
                                features::kAutofillUseDEAddressModel,
                                features::kAutofillUseINAddressModel,
                                features::kAutofillUseITAddressModel,
                                features::kAutofillUsePLAddressModel},
                               {});
  }

 private:
  base::test::ScopedFeatureList features_;
};

void TestAddressLineParsing(const AddressLineParsingTestCase& test_case) {
  AddressComponentsStore address =
      i18n_model_definition::CreateAddressComponentModel(
          AddressCountryCode(test_case.country_code));
  const AddressComponentTestValues test_value = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = test_case.street_address,
       .status = VerificationStatus::kObserved}};

  SetTestValues(address.Root(), test_value);

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
       .status = VerificationStatus::kFormatted},
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
  VerifyTestValues(address.Root(), expectation);
}

void TestAddressLineFormatting(const AddressLineParsingTestCase& test_case) {
  AddressComponentsStore store =
      i18n_model_definition::CreateAddressComponentModel(
          AddressCountryCode(test_case.country_code));
  AddressComponent* root = store.Root();

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
      {.type = ADDRESS_HOME_APT_TYPE,
       .value = test_case.apartment_type,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_LANDMARK,
       .value = test_case.landmark,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_BETWEEN_STREETS,
       .value = test_case.between_streets,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_ADMIN_LEVEL2,
       .value = test_case.admin_level_2,
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_OVERFLOW,
       .value = test_case.overflow,
       .status = VerificationStatus::kObserved}};

  SetTestValues(root, test_value);

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
      {.type = ADDRESS_HOME_APT_TYPE,
       .value = test_case.apartment_type,
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
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_OVERFLOW,
       .value = test_case.overflow,
       .status = VerificationStatus::kObserved}};
  VerifyTestValues(root, expectation);
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
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2",
       .apartment_num = "3"},
      {.street_address = "Implerstr. 73a OG 2",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2"},
      {.street_address = "Implerstr. 73a 2. OG",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2"},
      {.street_address = "Implerstr. no 73a",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "Implerstr. °73a",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.street_address = "Implerstr. number 73a",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.country_code = "GB",
       .street_address = "1600 Amphitheatre Parkway",
       .street_location = "1600 Amphitheatre Parkway",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600"},
      {.country_code = "GB",
       .street_address = "1600 Amphitheatre Parkway, Floor 6 Apt 12",
       .street_location = "1600 Amphitheatre Parkway",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600",
       .floor = "6",
       .apartment_num = "12"},
      {.country_code = "ES",
       .street_address = "Av. Paulista 1098, 1º andar, apto. 101",
       .street_location = "Av. Paulista 1098",
       .street_name = "Av. Paulista",
       .house_number = "1098",
       .floor = "1",
       .apartment_num = "101"},
      // Examples for Mexico.
      {.street_address = "Street Name 12 - Piso 13 - 14",
       .street_location = "Street Name 12",
       .street_name = "Street Name",
       .house_number = "12",
       .floor = "13",
       .apartment_num = "14"},
      {.street_address = "Street Name 12 - 14",
       .street_location = "Street Name 12",
       .street_name = "Street Name",
       .house_number = "12",
       .floor = "",
       .apartment_num = "14"},
      {.street_address = "Street Name 12 - Piso 13",
       .street_location = "Street Name 12",
       .street_name = "Street Name",
       .house_number = "12",
       .floor = "13",
       .apartment_num = ""},
      // Examples for Spain.
      {.street_address = "Street Name 1, 2º, 3ª",
       .street_location = "Street Name 1",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "2",
       .apartment_num = "3"},
      {.street_address = "Street Name 1, 2º",
       .street_location = "Street Name 1",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "2",
       .apartment_num = ""},
      {.street_address = "Street Name 1, 3ª",
       .street_location = "Street Name 1",
       .street_name = "Street Name",
       .house_number = "1",
       .floor = "",
       .apartment_num = "3"},
      // Regression test for crbug.com/365252089 (the trailing \n is important
      // because it means that the AddressLinesDecomposition part of the
      // street-address-alternative-1 cascade failed and the legacy fallback
      // regular expressions tried to bind an apartment number which is not part
      // of the DE custom hierarchy).
      {.country_code = "DE", .street_address = "2 Foo, Apt 2\n"},
  };

  for (const auto& test_case : test_cases) {
    TestAddressLineParsing(test_case);
  }
}

TEST_F(AutofillStructuredAddress, ParseMultiLineStreetAddress) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      {.street_address = "Implerstr. 73a\nObergeschoss 2 Wohnung 3",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .floor = "2",
       .apartment_num = "3"},
      {.street_address = "Implerstr. 73a\nSome Unparsable Text",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.country_code = "ZA",
       .street_address = "1600 Amphitheatre Parkway\nFloor 6 Apt 12",
       .street_location = "1600 Amphitheatre Parkway",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600",
       .floor = "6",
       .apartment_num = "12"},
      {.country_code = "ZA",
       .street_address = "1600 Amphitheatre Parkway\nSome UnparsableText",
       .street_location = "1600 Amphitheatre Parkway",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600"},
      {.country_code = "ES",
       .street_address = "Av. Paulista 1098\n1º andar, apto. 101",
       .street_location = "Av. Paulista 1098",
       .street_name = "Av. Paulista",
       .house_number = "1098",
       .floor = "1",
       .apartment_num = "101"}};

  for (const auto& test_case : test_cases) {
    TestAddressLineParsing(test_case);
  }
}

TEST_F(AutofillStructuredAddress, TestStreetAddressFormatting) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      {.country_code = "BR",
       .street_address = "Av. Brigadeiro Faria Lima, 3477\nAndar 1, Apto 2",
       .street_location = "Av. Brigadeiro Faria Lima, 3477",
       .street_name = "Av. Brigadeiro Faria Lima",
       .house_number = "3477",
       .floor = "1",
       .apartment_type = "Apto",
       .apartment_num = "2",
       .overflow = "Andar 1, Apto 2"},
      // TODO(crbug.com/40275657): Build conditional address formatting to
      // support cases where the apt_type is not present.
      {.country_code = "BR",
       .street_address = "Av. Brigadeiro Faria Lima, 3477\nAndar 1, 2",
       .street_location = "Av. Brigadeiro Faria Lima, 3477",
       .street_name = "Av. Brigadeiro Faria Lima",
       .house_number = "3477",
       .floor = "1",
       .apartment_num = "2",
       .overflow = "Andar 1, 2"},
      {.country_code = "DE",
       .street_address = "Erika-Mann-Str. 33",
       .street_location = "Erika-Mann-Str. 33",
       .street_name = "Erika-Mann-Str.",
       .house_number = "33"},
      {.country_code = "DE",
       .street_address = "Erika-Mann-Str. 33\n2. Stock, 12. Wohnung",
       .street_location = "Erika-Mann-Str. 33",
       .street_name = "Erika-Mann-Str.",
       .house_number = "33",
       .overflow = "2. Stock, 12. Wohnung"},
      {.country_code = "XX",
       .street_address = "Amphitheatre Parkway 1600\nApt. 12, Floor 6",
       .street_location = "Amphitheatre Parkway 1600",
       .street_name = "Amphitheatre Parkway",
       .house_number = "1600",
       .floor = "6",
       .apartment_num = "12"},
      // Examples for Mexico.
      {.country_code = "MX",
       .street_address = "StreetName 12, Piso 13, Apto 14",
       .street_location = "StreetName 12",
       .street_name = "StreetName",
       .house_number = "12",
       .floor = "13",
       .apartment_type = "Apto",
       .apartment_num = "14",
       .admin_level_2 = "Guanajuato"},
      {.country_code = "MX",
       .street_address = "StreetName 12, 14",
       .street_location = "StreetName 12",
       .street_name = "StreetName",
       .house_number = "12",
       .floor = "",
       .apartment_num = "14",
       .admin_level_2 = "Oaxaca"},
      {.country_code = "MX",
       .street_address = "StreetName 12, Piso 13",
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

  for (const auto& test_case : test_cases) {
    TestAddressLineFormatting(test_case);
  }
}

// Test setting the first address line.
TEST_F(AutofillStructuredAddress, TestSettingsAddressLine1) {
  AddressComponentsStore store =
      i18n_model_definition::CreateAddressComponentModel();

  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_LINE1,
       .value = "line1",
       .status = VerificationStatus::kObserved}};

  SetTestValues(store.Root(), test_values);

  AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_LINE1,
       .value = "line1",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1",
       .status = VerificationStatus::kObserved}};

  VerifyTestValues(store.Root(), expectation);
}

// Test settings all three address lines.
TEST_F(AutofillStructuredAddress, TestSettingsAddressLines) {
  AddressComponentsStore store =
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

  SetTestValues(store.Root(), test_values);

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

  VerifyTestValues(store.Root(), expectation);
}

// Test setting the home street address and retrieving the address lines.
TEST_F(AutofillStructuredAddress, TestGettingAddressLines) {
  AddressComponentsStore store =
      i18n_model_definition::CreateAddressComponentModel();

  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1\nline2\nline3",
       .status = VerificationStatus::kObserved}};

  SetTestValues(store.Root(), test_values);

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

  VerifyTestValues(store.Root(), expectation);
}

// Test setting the home street address and retrieving the address lines.
TEST_F(AutofillStructuredAddress,
       TestGettingAddressLines_JoinedAdditionalLines) {
  AddressComponentsStore store =
      i18n_model_definition::CreateAddressComponentModel();

  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "line1\nline2\nline3\nline4",
       .status = VerificationStatus::kObserved}};

  SetTestValues(store.Root(), test_values);

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

  VerifyTestValues(store.Root(), expectation);
}

// Tests that a structured address gets successfully migrated and subsequently
// completed.
TEST_F(AutofillStructuredAddress, TestMigrationAndFinalization) {
  AddressComponentsStore store =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponent* root = store.Root();

  // The test uses Great Britain as its address model contains all the address
  // tokens described below.
  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "123 Street name",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_COUNTRY,
       .value = "GB",
       .status = VerificationStatus::kNoStatus},
      {.type = ADDRESS_HOME_STATE,
       .value = "CA",
       .status = VerificationStatus::kNoStatus}};

  SetTestValues(root, test_values, /*finalize=*/false);

  // Invoke the migration. This should only change the verification statuses of
  // the set values.
  root->MigrateLegacyStructure();

  AddressComponentTestValues expectation_after_migration = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "123 Street name",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_COUNTRY,
       .value = "GB",
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

  VerifyTestValues(root, expectation_after_migration);

  // Complete the address tree and check the expectations.
  root->CompleteFullTree();

  AddressComponentTestValues expectation_after_completion = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "123 Street name",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_COUNTRY,
       .value = "GB",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_STATE,
       .value = "CA",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_ADDRESS,
       .value = "123 Street name\nCA",
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

  VerifyTestValues(root, expectation_after_completion);
}

// Tests that the migration does not happen of the root node
// (ADDRESS_HOME_ADDRESS) already has a verification status.
TEST_F(AutofillStructuredAddress,
       TestMigrationAndFinalization_AlreadyMigrated) {
  AddressComponentsStore store =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponent* root = store.Root();

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

  SetTestValues(root, test_values, /*finalize=*/false);

  // Invoke the migration. Since the ADDRESS_HOME_ADDRESS node already has a
  // verification status, the address is considered as already migrated.
  root->MigrateLegacyStructure();

  // Verify that the address was not changed by the migration.
  VerifyTestValues(root, test_values);
}

// Tests that a valid address structure is not wiped.
TEST_F(AutofillStructuredAddress,
       TestWipingAnInvalidSubstructure_ValidStructure) {
  AddressComponentsStore store =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponent* root = store.Root();
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

  SetTestValues(root, address_with_valid_structure,
                /*finalize=*/false);

  EXPECT_FALSE(root->WipeInvalidStructure());
  VerifyTestValues(root, address_with_valid_structure);
}

// Tests that an invalid address structure is wiped.
TEST_F(AutofillStructuredAddress,
       TestWipingAnInvalidSubstructure_InValidStructure) {
  AddressComponentsStore store =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponent* root = store.Root();
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

  SetTestValues(root, address_with_valid_structure,
                /*finalize=*/false);

  EXPECT_TRUE(root->WipeInvalidStructure());

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
  VerifyTestValues(root, address_with_wiped_structure);
}

// Test that the correct common country between structured addresses is
// computed.
TEST_F(AutofillStructuredAddress, TestGetCommonCountry) {
  AddressComponentsStore address1 =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponentsStore address2 =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponent* country1 =
      test_api(*address1.Root()).GetNodeForType(ADDRESS_HOME_COUNTRY);
  AddressComponent* country2 =
      test_api(*address2.Root()).GetNodeForType(ADDRESS_HOME_COUNTRY);

  // No countries set.
  EXPECT_EQ(country1->GetCommonCountry(*country2), AddressCountryCode(""));
  EXPECT_EQ(country2->GetCommonCountry(*country1), AddressCountryCode(""));

  // If exactly one country is set, use it as their common one.
  country1->SetValue(u"AT", VerificationStatus::kObserved);
  EXPECT_EQ(country1->GetCommonCountry(*country2), AddressCountryCode("AT"));
  EXPECT_EQ(country2->GetCommonCountry(*country1), AddressCountryCode("AT"));

  // If both are set to the same value, use it as their common one.
  country2->SetValue(u"AT", VerificationStatus::kObserved);
  EXPECT_EQ(country1->GetCommonCountry(*country2), AddressCountryCode("AT"));
  EXPECT_EQ(country2->GetCommonCountry(*country1), AddressCountryCode("AT"));

  // If both have a different value, there is no common one.
  country2->SetValue(u"DE", VerificationStatus::kObserved);
  EXPECT_EQ(country1->GetCommonCountry(*country2), AddressCountryCode(""));
  EXPECT_EQ(country2->GetCommonCountry(*country1), AddressCountryCode(""));
}

// Tests retrieving a value for comparison for a field type.
TEST_F(AutofillStructuredAddress, TestGetValueForComparisonForType) {
  AddressComponentsStore store =
      i18n_model_definition::CreateAddressComponentModel();

  AddressComponent* country_code =
      test_api(*store.Root()).GetNodeForType(ADDRESS_HOME_COUNTRY);
  country_code->SetValue(u"US", VerificationStatus::kObserved);

  AddressComponent* street_address =
      test_api(*store.Root()).GetNodeForType(ADDRESS_HOME_STREET_ADDRESS);
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

// Tests that when merging two equivalent street addresses, the longer one is
// preferred in merging.
TEST_F(AutofillStructuredAddress,
       LongerEquivalentStreetAddressHasPrecedenceInMerging) {
  AddressComponentsStore old_address_1 =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponentsStore old_address_2 =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponentsStore new_longer_address =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponentsStore new_shorter_address =
      i18n_model_definition::CreateAddressComponentModel();
  auto* old_street_1 = test_api(*old_address_1.Root())
                           .GetNodeForType(ADDRESS_HOME_STREET_ADDRESS);
  auto* old_street_2 = test_api(*old_address_2.Root())
                           .GetNodeForType(ADDRESS_HOME_STREET_ADDRESS);
  auto* new_longer_street = test_api(*new_longer_address.Root())
                                .GetNodeForType(ADDRESS_HOME_STREET_ADDRESS);
  auto* new_shorter_street = test_api(*new_shorter_address.Root())
                                 .GetNodeForType(ADDRESS_HOME_STREET_ADDRESS);

  old_street_1->SetValue(u"123 Main Street Av", VerificationStatus::kParsed);
  old_street_2->SetValue(u"123 Main Street Av", VerificationStatus::kParsed);
  new_longer_street->SetValue(u"123 Main Street Avenue",
                              VerificationStatus::kParsed);
  new_shorter_street->SetValue(u"123 Main St Av", VerificationStatus::kParsed);

  old_street_1->MergeWithComponent(*new_longer_street);
  EXPECT_EQ(old_street_1->GetValue(), new_longer_street->GetValue());

  old_street_2->MergeWithComponent(*new_shorter_street);
  EXPECT_NE(old_street_2->GetValue(), new_shorter_street->GetValue());
}

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

  AddressComponentsStore older_address =
      i18n_model_definition::CreateAddressComponentModel();
  SetTestValues(older_address.Root(), older_values);

  AddressComponentsStore newer_address =
      i18n_model_definition::CreateAddressComponentModel();
  SetTestValues(newer_address.Root(), newer_values);

  EXPECT_EQ(
      test_case.is_mergeable,
      older_address.Root()->IsMergeableWithComponent(*newer_address.Root()));

  AddressComponentsStore expectation_address =
      i18n_model_definition::CreateAddressComponentModel();
  SetTestValues(expectation_address.Root(), expectation_values);

  older_address.Root()->MergeWithComponent(*newer_address.Root());
  EXPECT_TRUE(older_address.Root()->SameAs(*expectation_address.Root()));
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

TEST_F(AutofillStructuredAddress, ParseStreetAddressLegacy) {
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

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);

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
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseStreetAddressMX) {
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
       .street_address = "Avenida Álvaro Obregón 1234, Piso 10, Apartamento 5A "
                         "Entre Calles Tonalá y Monterrey",
       .street_location = "Avenida Álvaro Obregón 1234",
       .street_name = "Avenida Álvaro Obregón",
       .house_number = "1234",
       .subpremise = "Piso 10, Apartamento 5A",
       .floor = "10",
       .apartment = "Apartamento 5A",
       .apartment_type = "Apartamento",
       .apartment_num = "5A",
       .overflow = "Entre Calles Tonalá y Monterrey",
       .cross_streets = "Tonalá y Monterrey",
       .cross_streets_1 = "Tonalá",
       .cross_streets_2 = "Monterrey"},
      {.country_code = "MX",
       .street_address = "Avenida Paseo de la Reforma 505 piso 2, interior 201"
                         ", entre Río Sena y Río Neva",
       .street_location = "Avenida Paseo de la Reforma 505",
       .street_name = "Avenida Paseo de la Reforma",
       .house_number = "505",
       .subpremise = "Piso 2, interior 201",
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
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);

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
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseSubpremiseMX) {
  AddressComponentsStore address =
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

  SetTestValues(address.Root(), test_value);

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
  VerifyTestValues(address.Root(), expectation);
}

TEST_F(AutofillStructuredAddress, ParseStreetAddressBR) {
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
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);

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
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseOverflowAndLandmarkBR) {
  AddressComponentsStore address =
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

  SetTestValues(address.Root(), test_value);

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
  VerifyTestValues(address.Root(), expectation);
}

TEST_F(AutofillStructuredAddress, ParseSubpremiseBR) {
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
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode("BR"));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_SUBPREMISE,
         .value = test_case.subpremise,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);

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
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseStreetAddressDE) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples for Germany.
      {.country_code = "DE",
       .street_address = "Implerstr. 73a Obergeschoss 2 Wohnung 3",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a",
       .overflow = "Obergeschoss 2 Wohnung 3"},
      {.country_code = "DE",
       .street_address = "Implerstr. 73 OG 2",
       .street_location = "Implerstr. 73",
       .street_name = "Implerstr.",
       .house_number = "73",
       .overflow = "OG 2"},
      {.country_code = "DE",
       .street_address = "Implerstr. nummer 73 2. OG",
       .street_location = "Implerstr. nummer 73",
       .street_name = "Implerstr.",
       .house_number = "73",
       .overflow = "2. OG"},
      {.country_code = "DE",
       .street_address = "Implerstr. 73 abcdefg",
       .street_location = "Implerstr. 73",
       .street_name = "Implerstr.",
       .house_number = "73",
       .overflow = "abcdefg"},
      {.country_code = "DE",
       .street_address = "Implerstr. nummer 73\nRückgebäude",
       .street_location = "Implerstr. nummer 73",
       .street_name = "Implerstr.",
       .house_number = "73",
       .overflow = "Rückgebäude"},
      {.country_code = "DE",
       .street_address = "Implerstr. nummer 73\nRückgebäude\nExtra info",
       .street_location = "Implerstr. nummer 73",
       .street_name = "Implerstr.",
       .house_number = "73",
       .overflow = "Rückgebäude\nExtra info"},
  };

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);

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
        {.type = ADDRESS_HOME_OVERFLOW,
         .value = test_case.overflow,
         .status = VerificationStatus::kParsed},
    };
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseStreetLocationDE) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples for Germany.
      {.country_code = "DE",
       .street_location = "Erika-Mann-Str. 3",
       .street_name = "Erika-Mann-Str.",
       .house_number = "3"},
      {.country_code = "DE",
       .street_location = "Implerstr. 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.country_code = "DE",
       .street_location = "Implerstr. no 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.country_code = "DE",
       .street_location = "Implerstr. °73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.country_code = "DE",
       .street_location = "Implerstr. Nummer 73a",
       .street_name = "Implerstr.",
       .house_number = "73a"},
      {.country_code = "DE",
       .street_location = "Implerstr. 10/12",
       .street_name = "Implerstr.",
       .house_number = "10/12"},
      {.country_code = "DE",
       .street_location = "Implerstr. Nummer 10 - 12",
       .street_name = "Implerstr.",
       .house_number = "10 - 12"},
      {.country_code = "DE",
       .street_location = "Implerstr. 73 a",
       .street_name = "Implerstr.",
       .house_number = "73 a"},
      {.country_code = "DE",
       .street_location = "Implerstr Nr 8",
       .street_name = "Implerstr",
       .house_number = "8"},
  };

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);

    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_NAME,
         .value = test_case.street_name,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_HOUSE_NUMBER,
         .value = test_case.house_number,
         .status = VerificationStatus::kParsed},
    };
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseSubpremiseAU) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples of subpremise(in-building-location) for Australia.
      {.country_code = "AU",
       .subpremise = "Apartment 75",
       .apartment = "Apartment 75",
       .apartment_type = "Apartment",
       .apartment_num = "75"},
      {.country_code = "AU",
       .subpremise = "Apt. 75 Floor 7",
       .floor = "7",
       .apartment = "Apt. 75",
       .apartment_type = "Apt.",
       .apartment_num = "75"},
      {.country_code = "AU",
       .subpremise = "Unit 7 Level 8",
       .floor = "8",
       .apartment = "Unit 7",
       .apartment_type = "Unit",
       .apartment_num = "7"},
      {.country_code = "AU",
       .subpremise = "suite 5 fl 10",
       .floor = "10",
       .apartment = "suite 5",
       .apartment_type = "suite",
       .apartment_num = "5"},
      {.country_code = "AU",
       .subpremise = "ste 6 ug",
       .floor = "ug",
       .apartment = "ste 6",
       .apartment_type = "ste",
       .apartment_num = "6"},
      {.country_code = "AU", .subpremise = "level 8", .floor = "8"},
      {.country_code = "AU", .subpremise = "ug", .floor = "ug"},
      {.country_code = "AU",
       .subpremise = "suite 75 ug",
       .floor = "ug",
       .apartment = "suite 75",
       .apartment_type = "suite",
       .apartment_num = "75"},
  };

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_SUBPREMISE,
         .value = test_case.subpremise,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);
    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_SUBPREMISE,
         .value = test_case.subpremise,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_APT_TYPE,
         .value = test_case.apartment_type,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_APT_NUM,
         .value = test_case.apartment_num,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_FLOOR,
         .value = test_case.floor,
         .status = VerificationStatus::kParsed},
    };
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseStreetLocationAU) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples of street locations (building-location) for Australia.
      {.country_code = "AU",
       .street_location = "16 Main Street",
       .street_name = "Main Street",
       .house_number = "16"},
      {.country_code = "AU",
       .street_location = "16A Main Street",
       .street_name = "Main Street",
       .house_number = "16A"},
      {.country_code = "AU",
       .street_location = "17-19 Main Street",
       .street_name = "Main Street",
       .house_number = "17-19"},
  };

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);
    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_NAME,
         .value = test_case.street_name,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_HOUSE_NUMBER,
         .value = test_case.house_number,
         .status = VerificationStatus::kParsed},
    };
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseStreetAddressAU) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples of street addresses for Australia.
      {.country_code = "AU",
       .street_address = "16 Main Street",
       .street_location = "16 Main Street",
       .street_name = "Main Street",
       .house_number = "16"},
      {.country_code = "AU",
       .street_address = "16A Main Street",
       .street_location = "16A Main Street",
       .street_name = "Main Street",
       .house_number = "16A"},
      {.country_code = "AU",
       .street_address = "Unit 7 Level 8  189 Great Eastern Highway",
       .street_location = "189 Great Eastern Highway",
       .street_name = "Great Eastern Highway",
       .house_number = "189",
       .floor = "8",
       .apartment = "Unit 7",
       .apartment_type = "Unit",
       .apartment_num = "7"},
      {.country_code = "AU",
       .street_address = "ste 5 ug 16A Main Street",
       .street_location = "16A Main Street",
       .street_name = "Main Street",
       .house_number = "16A",
       .floor = "ug",
       .apartment = "ste 5",
       .apartment_type = "ste",
       .apartment_num = "5"},
      {.country_code = "AU",
       .street_address = "u 5 17-19 Main Street",
       .street_location = "17-19 Main Street",
       .street_name = "Main Street",
       .house_number = "17-19",
       .apartment = "u 5",
       .apartment_type = "u",
       .apartment_num = "5"},
      {.country_code = "AU",
       .street_address = "u 5 level 7 17-19 Main Street",
       .street_location = "17-19 Main Street",
       .street_name = "Main Street",
       .house_number = "17-19",
       .floor = "7",
       .apartment = "u 5",
       .apartment_type = "u",
       .apartment_num = "5"},
      {.country_code = "AU",
       .street_address = "floor 5 17-19 Main Street",
       .street_location = "17-19 Main Street",
       .street_name = "Main Street",
       .house_number = "17-19",
       .floor = "5"},
      {.country_code = "AU",
       .street_address = "ug 17-19 Main Street",
       .street_location = "17-19 Main Street",
       .street_name = "Main Street",
       .house_number = "17-19",
       .floor = "ug"},
      {.country_code = "AU",
       .street_address = "17-19 Main Street",
       .street_location = "17-19 Main Street",
       .street_name = "Main Street",
       .house_number = "17-19"},
      {.country_code = "AU",
       .street_address = "suite 5 fl 10  189 Great Eastern Highway",
       .street_location = "189 Great Eastern Highway",
       .street_name = "Great Eastern Highway",
       .house_number = "189",
       .floor = "10",
       .apartment = "suite 5",
       .apartment_type = "suite",
       .apartment_num = "5"},
      {.country_code = "AU",
       .street_address = "17/189 Great Eastern Highway",
       .street_location = "189 Great Eastern Highway",
       .street_name = "Great Eastern Highway",
       .house_number = "189",
       .apartment = "17",
       .apartment_num = "17"},
      {.country_code = "AU",
       .street_address = "17 / 189 Great Eastern Highway",
       .street_location = "189 Great Eastern Highway",
       .street_name = "Great Eastern Highway",
       .house_number = "189",
       .apartment = "17",
       .apartment_num = "17"},
      {.country_code = "AU",
       .street_address = "U 17  189 Great Eastern Highway",
       .street_location = "189 Great Eastern Highway",
       .street_name = "Great Eastern Highway",
       .house_number = "189",
       .apartment = "U 17",
       .apartment_type = "U",
       .apartment_num = "17"},
      {.country_code = "AU",
       .street_address = "Unit 17  189 Great Eastern Highway",
       .street_location = "189 Great Eastern Highway",
       .street_name = "Great Eastern Highway",
       .house_number = "189",
       .apartment = "Unit 17",
       .apartment_type = "Unit",
       .apartment_num = "17"},
      {.country_code = "AU",
       .street_address = "Apt 17  189 Great Eastern Highway",
       .street_location = "189 Great Eastern Highway",
       .street_name = "Great Eastern Highway",
       .house_number = "189",
       .apartment = "Apt 17",
       .apartment_type = "Apt",
       .apartment_num = "17"},
      {.country_code = "AU",
       .street_address = "Floor 10  189 Great Eastern Highway",
       .street_location = "189 Great Eastern Highway",
       .street_name = "Great Eastern Highway",
       .house_number = "189",
       .floor = "10"},
      {.country_code = "AU",
       .street_address = "suite 3\n fl. 7 189 Great Eastern Highway",
       .street_location = "189 Great Eastern Highway",
       .street_name = "Great Eastern Highway",
       .house_number = "189",
       .floor = "7",
       .apartment = "suite 3",
       .apartment_type = "suite",
       .apartment_num = "3"},
      {.country_code = "AU",
       .street_address = "suite 3\n fl. 7 189-195 Great Eastern Highway",
       .street_location = "189-195 Great Eastern Highway",
       .street_name = "Great Eastern Highway",
       .house_number = "189-195",
       .floor = "7",
       .apartment = "suite 3",
       .apartment_type = "suite",
       .apartment_num = "3"},
      {.country_code = "AU",
       .street_address = "unit 7 189-195 Great Eastern Highway",
       .street_location = "189-195 Great Eastern Highway",
       .street_name = "Great Eastern Highway",
       .house_number = "189-195",
       .apartment = "unit 7",
       .apartment_type = "unit",
       .apartment_num = "7"},
  };

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);
    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = (ADDRESS_HOME_STREET_ADDRESS),
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
        {.type = ADDRESS_HOME_APT_TYPE,
         .value = test_case.apartment_type,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_APT_NUM,
         .value = test_case.apartment_num,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_FLOOR,
         .value = test_case.floor,
         .status = VerificationStatus::kParsed},
    };
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, TestFormattingPL) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      {.country_code = "PL",
       .street_address = "Jan Warsaw 9/10",
       .street_location = "Jan Warsaw 9/10",
       .street_name = "Jan Warsaw",
       .building_and_unit = "9/10",
       .house_number = "9",
       .apartment = "10",
       .apartment_num = "10"},
      {.country_code = "PL",
       .street_address = "Warsaw 9/m. 10",
       .street_location = "Warsaw 9/m. 10",
       .street_name = "Warsaw",
       .building_and_unit = "9/m. 10",
       .house_number = "9",
       .apartment = "m. 10",
       .apartment_type = "m.",
       .apartment_num = "10"},
      {.country_code = "PL",
       .street_address = "Warsaw 9",
       .street_location = "Warsaw 9",
       .street_name = "Warsaw",
       .building_and_unit = "9",
       .house_number = "9"},
      {.country_code = "PL",
       .street_address = "Warsaw 9A/10",
       .street_location = "Warsaw 9A/10",
       .street_name = "Warsaw",
       .building_and_unit = "9A/10",
       .house_number = "9A",
       .apartment = "10",
       .apartment_num = "10"}};

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

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
        {.type = ADDRESS_HOME_APT_TYPE,
         .value = test_case.apartment_type,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_APT_NUM,
         .value = test_case.apartment_num,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);

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
        {.type = ADDRESS_HOME_APT_TYPE,
         .value = test_case.apartment_type,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_APT_NUM,
         .value = test_case.apartment_num,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
         .value = test_case.building_and_unit,
         .status = VerificationStatus::kFormatted}};
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseBuildingAndUnitPL) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples of house number and apartment numbers for Poland.
      {.country_code = "PL",
       .building_and_unit = "9/10",
       .house_number = "9",
       .apartment = "10",
       .apartment_num = "10"},
      {.country_code = "PL", .building_and_unit = "9", .house_number = "9"},
      {.country_code = "PL",
       .building_and_unit = "9A/10",
       .house_number = "9A",
       .apartment = "10",
       .apartment_num = "10"},
      {.country_code = "PL", .building_and_unit = "9A", .house_number = "9A"},
      {.country_code = "PL",
       .building_and_unit = "9A m. 10",
       .house_number = "9A",
       .apartment = "m. 10",
       .apartment_type = "m.",
       .apartment_num = "10"},
      {.country_code = "PL",
       .building_and_unit = "9A/m.10",
       .house_number = "9A",
       .apartment = "m.10",
       .apartment_type = "m.",
       .apartment_num = "10"}};

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
         .value = test_case.building_and_unit,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);
    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
         .value = test_case.building_and_unit,
         .status = VerificationStatus::kObserved},
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
    };
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseStreetAddressPL) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples of street addresses for Poland.
      {.country_code = "PL",
       .street_address = "ul. Jan Warsaw 9/10",
       .street_location = "ul. Jan Warsaw 9/10",
       .street_name = "Jan Warsaw",
       .building_and_unit = "9/10",
       .house_number = "9",
       .apartment = "10",
       .apartment_num = "10"},
      {.country_code = "PL",
       .street_address = "al. Warsaw 9/10",
       .street_location = "al. Warsaw 9/10",
       .street_name = "Warsaw",
       .building_and_unit = "9/10",
       .house_number = "9",
       .apartment = "10",
       .apartment_num = "10"},
      {.country_code = "PL",
       .street_address = "Warsaw 9/10",
       .street_location = "Warsaw 9/10",
       .street_name = "Warsaw",
       .building_and_unit = "9/10",
       .house_number = "9",
       .apartment = "10",
       .apartment_num = "10"},
      {.country_code = "PL",
       .street_address = "Warsaw 9",
       .street_location = "Warsaw 9",
       .street_name = "Warsaw",
       .building_and_unit = "9",
       .house_number = "9"},
      {.country_code = "PL",
       .street_address = "Warsaw 9A/10",
       .street_location = "Warsaw 9A/10",
       .street_name = "Warsaw",
       .building_and_unit = "9A/10",
       .house_number = "9A",
       .apartment = "10",
       .apartment_num = "10"},
      {.country_code = "PL",
       .street_address = "pl Warsaw 9",
       .street_location = "pl Warsaw 9",
       .street_name = "Warsaw",
       .building_and_unit = "9",
       .house_number = "9"},
      {.country_code = "PL",
       .street_address = "pl Warsaw 9A",
       .street_location = "pl Warsaw 9A",
       .street_name = "Warsaw",
       .building_and_unit = "9A",
       .house_number = "9A"},
      {.country_code = "PL",
       .street_address = "aleja Warsaw 9A",
       .street_location = "aleja Warsaw 9A",
       .street_name = "Warsaw",
       .building_and_unit = "9A",
       .house_number = "9A"},
      {.country_code = "PL",
       .street_address = "ul. Warsaw 9A m. 10",
       .street_location = "ul. Warsaw 9A m. 10",
       .street_name = "Warsaw",
       .building_and_unit = "9A m. 10",
       .house_number = "9A",
       .apartment = "m. 10",
       .apartment_type = "m.",
       .apartment_num = "10"},
      {.country_code = "PL",
       .street_address = "ul. Warsaw 9A/m.10",
       .street_location = "ul. Warsaw 9A/m.10",
       .street_name = "Warsaw",
       .building_and_unit = "9A/m.10",
       .house_number = "9A",
       .apartment = "m.10",
       .apartment_type = "m.",
       .apartment_num = "10"},
  };

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);
    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = (ADDRESS_HOME_STREET_ADDRESS),
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_STREET_NAME,
         .value = test_case.street_name,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
         .value = test_case.building_and_unit,
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
    };
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, TestFormattingIT) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      {.country_code = "IT",
       .street_address = "Corso Vittorio Emanuele II 30",
       .street_location = "Corso Vittorio Emanuele II 30",
       .street_name = "Corso Vittorio Emanuele II",
       .house_number = "30",
       .overflow = ""},
      {.country_code = "IT",
       .street_address = "Corso Vittorio Emanuele II 30, Scala A Interno 4",
       .street_location = "Corso Vittorio Emanuele II 30",
       .street_name = "Corso Vittorio Emanuele II",
       .house_number = "30",
       .overflow = "Scala A Interno 4"},
      {.country_code = "IT",
       .street_address = "Piazza Roma 15, Appartamento 3",
       .street_location = "Piazza Roma 15",
       .street_name = "Piazza Roma",
       .house_number = "15",
       .overflow = "Appartamento 3"},
      {.country_code = "IT",
       .street_address = "Corso Vittorio Emanuele II, Scala A Interno 4",
       .street_location = "Corso Vittorio Emanuele II",
       .street_name = "Corso Vittorio Emanuele II",
       .house_number = "",
       .overflow = "Scala A Interno 4"},
      {.country_code = "IT",
       .street_address = "Corso Vittorio Emanuele II",
       .street_location = "Corso Vittorio Emanuele II",
       .street_name = "Corso Vittorio Emanuele II",
       .house_number = "",
       .overflow = ""}};

  for (const auto &test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

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
        {.type = ADDRESS_HOME_OVERFLOW,
         .value = test_case.overflow,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);

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
        {.type = ADDRESS_HOME_OVERFLOW,
         .value = test_case.overflow,
         .status = VerificationStatus::kObserved}};
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseStreetLocationIT) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples for Italy.
      {.country_code = "IT",
       .street_location = "Via Nazionale 50",
       .street_name = "Via Nazionale",
       .house_number = "50"},
      {.country_code = "IT",
       .street_location = "Via Nazionale 73a",
       .street_name = "Via Nazionale",
       .house_number = "73a"},
      {.country_code = "IT",
       .street_location = "Via Nazionale, 73a",
       .street_name = "Via Nazionale",
       .house_number = "73a"},
      {.country_code = "IT",
       .street_location = "Via Nazionale no 73a",
       .street_name = "Via Nazionale",
       .house_number = "73a"},
      {.country_code = "IT",
       .street_location = "Via Nazionale °50",
       .street_name = "Via Nazionale",
       .house_number = "50"},
      {.country_code = "IT",
       .street_location = "Via Nazionale numero 50",
       .street_name = "Via Nazionale",
       .house_number = "50"},
  };

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);

    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_NAME,
         .value = test_case.street_name,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_HOUSE_NUMBER,
         .value = test_case.house_number,
         .status = VerificationStatus::kParsed},
    };
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseStreetAddressIT) {
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples of street addresses for Italy.
      {.country_code = "IT",
       .street_address = "Corso Vittorio Emanuele II 30",
       .street_location = "Corso Vittorio Emanuele II 30",
       .street_name = "Corso Vittorio Emanuele II",
       .house_number = "30",
       .overflow = ""},
      {.country_code = "IT",
       .street_address = "Corso Vittorio Emanuele II, 30",
       .street_location = "Corso Vittorio Emanuele II, 30",
       .street_name = "Corso Vittorio Emanuele II",
       .house_number = "30",
       .overflow = ""},
      {.country_code = "IT",
       .street_address = "Corso Vittorio Emanuele II 30 Scala A Interno 4",
       .street_location = "Corso Vittorio Emanuele II 30",
       .street_name = "Corso Vittorio Emanuele II",
       .house_number = "30",
       .overflow = "Scala A Interno 4"},
      {.country_code = "IT",
       .street_address = "Corso Vittorio Emanuele II 30, Scala A Interno 4",
       .street_location = "Corso Vittorio Emanuele II 30",
       .street_name = "Corso Vittorio Emanuele II",
       .house_number = "30",
       .overflow = "Scala A Interno 4"},
      {.country_code = "IT",
       .street_address = "Corso Vittorio Emanuele II 30, Scala A, Interno 4",
       .street_location = "Corso Vittorio Emanuele II 30",
       .street_name = "Corso Vittorio Emanuele II",
       .house_number = "30",
       .overflow = "Scala A, Interno 4"},
      {.country_code = "IT",
       .street_address = "Piazza Roma 15, Appartamento 3",
       .street_location = "Piazza Roma 15",
       .street_name = "Piazza Roma",
       .house_number = "15",
       .overflow = "Appartamento 3"},
      {.country_code = "IT",
       .street_address = "Piazza Roma numero 73 Palazzo 12, Piano 3",
       .street_location = "Piazza Roma numero 73",
       .street_name = "Piazza Roma",
       .house_number = "73",
       .overflow = "Palazzo 12, Piano 3"},
      {.country_code = "IT",
       .street_address = "Piazza Roma nr 73",
       .street_location = "Piazza Roma nr 73",
       .street_name = "Piazza Roma",
       .house_number = "73",
       .overflow = ""},
      {.country_code = "IT",
       .street_address = "Casella Postale 1234 abcdefg",
       .street_location = "Casella Postale 1234",
       .street_name = "Casella Postale",
       .house_number = "1234",
       .overflow = "abcdefg"},
      {.country_code = "IT",
       .street_address = "Casella Postale, 1234 abcdefg",
       .street_location = "Casella Postale, 1234",
       .street_name = "Casella Postale",
       .house_number = "1234",
       .overflow = "abcdefg"},
  };

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);
    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = (ADDRESS_HOME_STREET_ADDRESS),
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
        {.type = ADDRESS_HOME_OVERFLOW,
         .value = test_case.overflow,
         .status = VerificationStatus::kParsed},
    };
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseStreetLocationFR) {
  base::test::ScopedFeatureList features_{features::kAutofillUseFRAddressModel};
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples of street locations (building-location) for France.
      {.country_code = "FR",
       .street_location = "1661 Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "1661"},
      {.country_code = "FR",
       .street_location = "16A Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "16A"},
      {.country_code = "FR",
       .street_location = "17-19 Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "17-19"},
  };

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);
    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_LOCATION,
         .value = test_case.street_location,
         .status = VerificationStatus::kObserved},
        {.type = ADDRESS_HOME_STREET_NAME,
         .value = test_case.street_name,
         .status = VerificationStatus::kParsed},
        {.type = ADDRESS_HOME_HOUSE_NUMBER,
         .value = test_case.house_number,
         .status = VerificationStatus::kParsed},
    };
    VerifyTestValues(address.Root(), expectation);
  }
}

TEST_F(AutofillStructuredAddress, ParseStreetAddressFR) {
  base::test::ScopedFeatureList features_{features::kAutofillUseFRAddressModel};
  std::vector<AddressLineParsingTestCase> test_cases = {
      // Examples of street addresses for France.
      {.country_code = "FR",
       .street_address = "1661 Place Charles de Gaulle",
       .street_location = "1661 Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "1661"},
      {.country_code = "FR",
       .street_address = "16A Place Charles de Gaulle",
       .street_location = "16A Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "16A"},
      {.country_code = "FR",
       .street_address = "Appartement 36 1661 Place Charles de Gaulle",
       .street_location = "1661 Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "1661",
       .overflow = "Appartement 36"},
      {.country_code = "FR",
       .street_address = "Appartement 36\n1661 Place Charles de Gaulle",
       .street_location = "1661 Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "1661",
       .overflow = "Appartement 36"},
      {.country_code = "FR",
       .street_address = "Appartement 36, 1661 Place Charles de Gaulle",
       .street_location = "1661 Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "1661",
       .overflow = "Appartement 36"},
      {.country_code = "FR",
       .street_address = "App 36, 1661 Place Charles de Gaulle",
       .street_location = "1661 Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "1661",
       .overflow = "App 36"},
      {.country_code = "FR",
       .street_address = "Appt 36, 1661 Place Charles de Gaulle",
       .street_location = "1661 Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "1661",
       .overflow = "Appt 36"},
      {.country_code = "FR",
       .street_address = "App. 36, 1661 Place Charles de Gaulle",
       .street_location = "1661 Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "1661",
       .overflow = "App. 36"},
      {.country_code = "FR",
       .street_address = "Appt. 36, 1661 Place Charles de Gaulle",
       .street_location = "1661 Place Charles de Gaulle",
       .street_name = "Place Charles de Gaulle",
       .house_number = "1661",
       .overflow = "Appt. 36"},
  };

  for (const auto& test_case : test_cases) {
    AddressComponentsStore address =
        i18n_model_definition::CreateAddressComponentModel(
            AddressCountryCode(test_case.country_code));

    const AddressComponentTestValues test_value = {
        {.type = ADDRESS_HOME_STREET_ADDRESS,
         .value = test_case.street_address,
         .status = VerificationStatus::kObserved}};

    SetTestValues(address.Root(), test_value);
    const AddressComponentTestValues expectation = {
        {.type = ADDRESS_HOME_COUNTRY,
         .value = test_case.country_code,
         .status = VerificationStatus::kObserved},
        {.type = (ADDRESS_HOME_STREET_ADDRESS),
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
        {.type = ADDRESS_HOME_OVERFLOW,
         .value = test_case.overflow,
         .status = VerificationStatus::kParsed},
    };
    VerifyTestValues(address.Root(), expectation);
  }
}

}  // namespace

}  // namespace autofill
