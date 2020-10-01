// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address.h"

#include <iostream>
#include <utility>
#include "base/i18n/case_conversion.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_rewriter.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_constants.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_regex_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace structured_address {

base::string16 AddressComponentWithRewriter::RewriteValue(
    const base::string16& value) const {
  // Retrieve the country name from the structured tree the node resides in.
  base::string16 country = GetRootNode().GetValueForType(ADDRESS_HOME_COUNTRY);
  // If no country is available (this should not be the case for a valid
  // importable profile), use the US as a fallback country for the rewriter.
  return RewriterCache::Rewrite(
      !country.empty() ? country : base::ASCIIToUTF16("US"), value);
}

base::string16 AddressComponentWithRewriter::ValueForComparison() const {
  return RewriteValue(NormalizedValue());
}

StreetName::StreetName(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_STREET_NAME,
                       parent,
                       {},
                       MergeMode::kDefault) {}

StreetName::~StreetName() = default;

DependentStreetName::DependentStreetName(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_DEPENDENT_STREET_NAME,
                       parent,
                       {},
                       MergeMode::kDefault) {}

DependentStreetName::~DependentStreetName() = default;

StreetAndDependentStreetName::StreetAndDependentStreetName(
    AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME,
                       parent,
                       {&thoroughfare_name_, &dependent_thoroughfare_name_},
                       MergeMode::kDefault) {}

StreetAndDependentStreetName::~StreetAndDependentStreetName() = default;

HouseNumber::HouseNumber(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_HOUSE_NUMBER,
                       parent,
                       {},
                       MergeMode::kDefault) {}

HouseNumber::~HouseNumber() = default;

Premise::Premise(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_PREMISE_NAME,
                       parent,
                       {},
                       MergeMode::kDefault) {}

Premise::~Premise() = default;

Floor::Floor(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_FLOOR, parent, {}, MergeMode::kDefault) {}

Floor::~Floor() = default;

Apartment::Apartment(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_APT_NUM, parent, {}, MergeMode::kDefault) {}

Apartment::~Apartment() = default;

SubPremise::SubPremise(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_SUBPREMISE,
                       parent,
                       {&floor_, &apartment_},
                       MergeMode::kDefault) {}

SubPremise::~SubPremise() = default;

// Address are mergeable if one is a subset of the other one.
// Take the longer one. If both addresses have the same tokens apply a recursive
// strategy to merge the substructure.
StreetAddress::StreetAddress(AddressComponent* parent)
    : AddressComponentWithRewriter(
          ADDRESS_HOME_STREET_ADDRESS,
          parent,
          {&streets_, &number_, &premise_, &sub_premise_},
          MergeMode::kReplaceEmpty | MergeMode::kReplaceSubset |
              MergeMode::kDefault) {}

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

void StreetAddress::ParseValueAndAssignSubcomponentsByFallbackMethod() {
  // There is no point in doing a line-wise approach if there aren't multiple
  // lines.
  if (address_lines_.size() < 2)
    return;

  // Try to parse the address using only the first line.
  for (const auto* parse_expression : GetParseRegularExpressionsByRelevance()) {
    if (ParseValueAndAssignSubcomponentsByRegularExpression(
            address_lines_.at(0), parse_expression)) {
      return;
    }
  }
}

bool StreetAddress::HasNewerValuePrecendenceInMerging(
    const AddressComponent& newer_component) const {
  // If the newer component has a better verification status, use the newer one.
  if (GetVerificationStatus() < newer_component.GetVerificationStatus())
    return true;

  // If the verification statuses are the same, do not use the newer component
  // if the older one has new lines but the newer one doesn't.
  if (GetVerificationStatus() == newer_component.GetVerificationStatus()) {
    if (GetValue().find('\n') != base::string16::npos &&
        newer_component.GetValue().find('\n') == base::string16::npos) {
      return false;
    }
    return true;
  }
  return false;
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

// Country codes are mergeable if they are the same of if one is empty.
// For merging, pick the non-empty one.
CountryCode::CountryCode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_COUNTRY,
                       parent,
                       {},
                       MergeMode::kReplaceEmpty |
                           MergeMode::kUseBetterOrNewerForSameValue) {}

CountryCode::~CountryCode() = default;

// DependentLocalities are mergeable when the tokens of one is a subset of the
// other one. Take the longer one.
DependentLocality::DependentLocality(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_DEPENDENT_LOCALITY,
                       parent,
                       {},
                       MergeMode::kReplaceSubset | MergeMode::kReplaceEmpty) {}

DependentLocality::~DependentLocality() = default;

// Cities are mergeable when the tokens of one is a subset of the other one.
// Take the shorter non-empty one.
City::City(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_CITY,
                       parent,
                       {},
                       MergeMode::kReplaceSubset | MergeMode::kReplaceEmpty) {}

City::~City() = default;

// States are mergeable when the tokens of one is a subset of the other one.
// Take the shorter non-empty one.
State::State(AddressComponent* parent)
    : AddressComponentWithRewriter(
          ADDRESS_HOME_STATE,
          parent,
          {},
          MergeMode::kPickShorterIfOneContainsTheOther | kReplaceEmpty) {}

State::~State() = default;

// Zips are mergeable when one is a substring of the other one.
// For merging, the shorter substring is taken.
PostalCode::PostalCode(AddressComponent* parent)
    : AddressComponentWithRewriter(
          ADDRESS_HOME_ZIP,
          parent,
          {},
          MergeMode::kUseMostRecentSubstring | kReplaceEmpty) {}

PostalCode::~PostalCode() = default;

base::string16 PostalCode::NormalizedValue() const {
  return NormalizeValue(GetValue(), /*keep_white_space=*/false);
}

SortingCode::SortingCode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_SORTING_CODE,
                       parent,
                       {},
                       MergeMode::kReplaceEmpty | kUseMostRecentSubstring) {}

SortingCode::~SortingCode() = default;

Address::Address() : Address{nullptr} {}

Address::Address(const Address& other) : Address() {
  *this = other;
}

// Addresses are mergeable when all of their children are mergeable.
// Reformat the address from their children after merge.
Address::Address(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_ADDRESS,
                       parent,
                       {&street_address_, &postal_code_, &sorting_code_,
                        &dependent_locality_, &city_, &state_, &country_code_},
                       MergeMode::kMergeChildrenAndReformat) {}

Address::~Address() = default;

void Address::MigrateLegacyStructure(bool is_verified_profile) {
  // If this component already has a verification status, no profile is regarded
  // as already verified.
  std::cout << "APply migration" << std::endl;
  if (GetVerificationStatus() != VerificationStatus::kNoStatus)
    return;

  // Otherwise set the status of the subcomponents either to observed or
  // verified depending on |is_verified_profile| if they already have a value
  // assigned. Note, those are all the tokens that are already present in the
  // unstructured address representation.
  for (auto* component : Subcomponents()) {
    if (!component->GetValue().empty() &&
        component->GetVerificationStatus() == VerificationStatus::kNoStatus) {
      component->SetValue(component->GetValue(),
                          is_verified_profile
                              ? VerificationStatus::kUserVerified
                              : VerificationStatus::kObserved);
    }
  }
}

}  // namespace structured_address

}  // namespace autofill
