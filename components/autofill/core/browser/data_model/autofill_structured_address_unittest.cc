// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address.h"

#include <stddef.h>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace structured_address {

using AddressComponentTestValues = std::vector<AddressComponentTestValue>;

struct AddressLineParsingTestCase {
  std::string country_code = "";
  std::string street_address = "";
  std::string street_name = "";
  std::string house_number = "";
  std::string floor = "";
  std::string apartment = "";
};

std::ostream& operator<<(std::ostream& out,
                         const AddressLineParsingTestCase& test_case) {
  out << "Country: " << test_case.country_code << std::endl;
  out << "Street address: " << test_case.street_address << std::endl;
  out << "Street name: " << test_case.street_name << std::endl;
  out << "House number: " << test_case.house_number << std::endl;
  out << "Floor: " << test_case.floor << std::endl;
  out << "Apartment: " << test_case.apartment << std::endl;
  return out;
}

void TestAddressLineParsing(const AddressLineParsingTestCase& test_case) {
  Address address(nullptr);
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
  Address address;
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
       .apartment = "101"}};

  for (const auto& test_case : test_cases)
    TestAddressLineParsing(test_case);
}

TEST(AutofillStructuredAddress, TestStreetAddressFormatting) {
  Address address;

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
       .apartment = "12"}};

  for (const auto& test_case : test_cases)
    TestAddressLineFormatting(test_case);
}

}  // namespace
}  // namespace structured_address
}  // namespace autofill
