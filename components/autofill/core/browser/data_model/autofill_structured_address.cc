// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address.h"

#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_constants.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_regex_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace structured_address {

StreetName::StreetName(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_STREET_NAME, parent) {}

StreetName::~StreetName() = default;

DependentStreetName::DependentStreetName(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_DEPENDENT_STREET_NAME, parent) {}

DependentStreetName::~DependentStreetName() = default;

StreetAndDependentStreetName::StreetAndDependentStreetName(
    AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME,
                       parent,
                       {&thoroughfare_name_, &dependent_thoroughfare_name_}) {}

StreetAndDependentStreetName::~StreetAndDependentStreetName() = default;

HouseNumber::HouseNumber(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_HOUSE_NUMBER, parent) {}

HouseNumber::~HouseNumber() = default;

Premise::Premise(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_PREMISE_NAME, parent) {}

Premise::~Premise() = default;

Floor::Floor(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_FLOOR, parent) {}

Floor::~Floor() = default;

Apartment::Apartment(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_APT_NUM, parent) {}

Apartment::~Apartment() = default;

SubPremise::SubPremise(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_SUBPREMISE,
                       parent,
                       {&floor_, &apartment_}) {}

SubPremise::~SubPremise() = default;

StreetAddress::StreetAddress(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_STREET_ADDRESS,
                       parent,
                       {&streets_, &number_, &premise_, &sub_premise_}) {}

StreetAddress::~StreetAddress() = default;

std::vector<const re2::RE2*>
StreetAddress::GetParseRegularExpressionsByRelevance() const {
  auto* pattern_provider = StructuredAddressesRegExProvider::Instance();
  DCHECK(pattern_provider);
  return {pattern_provider->GetRegEx(RegEx::kParseHouseNumberStreetName),
          pattern_provider->GetRegEx(RegEx::kParseStreetNameHouseNumber),
          pattern_provider->GetRegEx(
              RegEx::kParseStreetNameHouseNumberSuffixedFloor)};
}

base::string16 StreetAddress::GetBestFormatString() const {
  std::string country_code =
      base::UTF16ToUTF8(GetRootNode().GetValueForType(ADDRESS_HOME_COUNTRY));

  if (country_code == "BR") {
    return base::UTF8ToUTF16(
        "${ADDRESS_HOME_STREET_NAME}${ADDRESS_HOME_HOUSE_NUMBER;, }"
        "${ADDRESS_HOME_FLOOR;, ;º andar}${ADDRESS_HOME_APT_NUM;, apto ;}");
  }

  if (country_code == "DE") {
    return base::ASCIIToUTF16(
        "${ADDRESS_HOME_STREET_NAME} ${ADDRESS_HOME_HOUSE_NUMBER}"
        "${ADDRESS_HOME_FLOOR;, ;. Stock}${ADDRESS_HOME_APT_NUM;, ;. Wohnung}");
  }

  // Use the format for US/UK as the default.
  return base::ASCIIToUTF16(
      "${ADDRESS_HOME_HOUSE_NUMBER} ${ADDRESS_HOME_STREET_NAME} "
      "${ADDRESS_HOME_FLOOR;FL } ${ADDRESS_HOME_APT_NUM;APT }");
}

CountryCode::CountryCode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_COUNTRY, parent) {}

CountryCode::~CountryCode() = default;

City::City(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_CITY, parent) {}

City::~City() = default;

State::State(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_STATE, parent) {}

State::~State() = default;

PostalCode::PostalCode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_ZIP, parent) {}

PostalCode::~PostalCode() = default;

Address::Address() : Address{nullptr} {}

Address::Address(AddressComponent* parent)
    : AddressComponent(
          ADDRESS_HOME_ADDRESS,
          parent,
          {&street_address_, &postal_code_, &city_, &state_, &country_code_}) {}

Address::~Address() = default;

}  // namespace structured_address

}  // namespace autofill
