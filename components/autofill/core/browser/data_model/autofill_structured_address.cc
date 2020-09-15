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

void StreetAddress::UnsetValue() {
  AddressComponent::UnsetValue();
  address_lines_.clear();
}

void StreetAddress::SetValue(base::string16 value, VerificationStatus status) {
  AddressComponent::SetValue(value, status);
  CalculateAddressLines();
}

void StreetAddress::CalculateAddressLines() {
  // Recalculate |address_lines_| after changing the street address.
  address_lines_ =
      base::SplitString(GetValue(), base::ASCIIToUTF16("\n"),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // If splitting of the address line results in more than 3 entries, join the
  // additional entries into the third line.
  if (address_lines_.size() > 3) {
    address_lines_[2] =
        base::JoinString(std::vector<base::string16>(address_lines_.begin() + 2,
                                                     address_lines_.end()),
                         base::ASCIIToUTF16(" "));
    // Drop the addition address lines.
    while (address_lines_.size() > 3)
      address_lines_.pop_back();
  }
}

bool StreetAddress::IsValueValid() const {
  return !base::Contains(address_lines_, base::string16());
}

bool StreetAddress::ConvertAndGetTheValueForAdditionalFieldTypeName(
    const std::string& type_name,
    base::string16* value) const {
  if (type_name == AutofillType(ADDRESS_HOME_LINE1).ToString()) {
    if (value) {
      *value =
          address_lines_.size() > 0 ? address_lines_.at(0) : base::string16();
    }
    return true;
  }
  if (type_name == AutofillType(ADDRESS_HOME_LINE2).ToString()) {
    if (value) {
      *value =
          address_lines_.size() > 1 ? address_lines_.at(1) : base::string16();
    }
    return true;
  }
  if (type_name == AutofillType(ADDRESS_HOME_LINE3).ToString()) {
    if (value) {
      *value =
          address_lines_.size() > 2 ? address_lines_.at(2) : base::string16();
    }
    return true;
  }

  return false;
}

// Implements support for setting the value of the individual address lines.
bool StreetAddress::ConvertAndSetValueForAdditionalFieldTypeName(
    const std::string& type_name,
    const base::string16& value,
    const VerificationStatus& status) {
  size_t index = 0;
  if (type_name == AutofillType(ADDRESS_HOME_LINE1).ToString()) {
    index = 0;
  } else if (type_name == AutofillType(ADDRESS_HOME_LINE2).ToString()) {
    index = 1;
  } else if (type_name == AutofillType(ADDRESS_HOME_LINE3).ToString()) {
    index = 2;
  } else {
    return false;
  }

  // Make sure that there are three address lines stored.
  if (index >= address_lines_.size())
    address_lines_.resize(index + 1, base::string16());

  bool change = address_lines_[index] != value;
  if (change)
    address_lines_[index] = value;

  while (!address_lines_.empty() && address_lines_.back().empty())
    address_lines_.pop_back();

  // By calling the base class implementation, the recreation of the address
  // lines from the street address is omitted.
  if (change) {
    AddressComponent::SetValue(
        base::JoinString(address_lines_, base::ASCIIToUTF16("\n")), status);
  }

  return true;
}

void StreetAddress::PostAssignSanitization() {
  CalculateAddressLines();
}

void StreetAddress::GetAdditionalSupportedFieldTypes(
    ServerFieldTypeSet* supported_types) const {
  supported_types->insert(ADDRESS_HOME_LINE1);
  supported_types->insert(ADDRESS_HOME_LINE2);
  supported_types->insert(ADDRESS_HOME_LINE3);
}

CountryCode::CountryCode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_COUNTRY, parent) {}

CountryCode::~CountryCode() = default;

DependentLocality::DependentLocality(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_DEPENDENT_LOCALITY, parent) {}

DependentLocality::~DependentLocality() = default;

City::City(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_CITY, parent) {}

City::~City() = default;

State::State(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_STATE, parent) {}

State::~State() = default;

PostalCode::PostalCode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_ZIP, parent) {}

PostalCode::~PostalCode() = default;

SortingCode::SortingCode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_SORTING_CODE, parent) {}

SortingCode::~SortingCode() = default;

Address::Address() : Address{nullptr} {}

Address::Address(const Address& other) : Address() {
  *this = other;
}

Address::Address(AddressComponent* parent)
    : AddressComponent(
          ADDRESS_HOME_ADDRESS,
          parent,
          {&street_address_, &postal_code_, &sorting_code_,
           &dependent_locality_, &city_, &state_, &country_code_}) {}

Address::~Address() = default;

void Address::MigrateLegacyStructure(bool is_verified_profile) {
  VerificationStatus status = is_verified_profile
                                  ? VerificationStatus::kUserVerified
                                  : VerificationStatus::kObserved;
  if (GetVerificationStatus() == VerificationStatus::kNoStatus &&
      !GetValue().empty()) {
    SetValue(GetValue(), status);
  }
}

}  // namespace structured_address

}  // namespace autofill
