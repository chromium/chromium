// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_regex_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_rewriter.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

std::u16string AddressComponentWithRewriter::GetValueForComparison(
    const std::u16string& value,
    const AddressComponent& other) const {
  return NormalizeAndRewrite(GetCommonCountry(other), value,
                             /*keep_white_space=*/true);
}

StreetNameNode::StreetNameNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_STREET_NAME,
                       std::move(children),
                       MergeMode::kDefault) {}

StreetNameNode::~StreetNameNode() = default;

StreetLocationNode::StreetLocationNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_STREET_LOCATION,
                       std::move(children),
                       MergeMode::kDefault) {}

StreetLocationNode::~StreetLocationNode() = default;

HouseNumberNode::HouseNumberNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_HOUSE_NUMBER,
                       std::move(children),
                       MergeMode::kDefault) {}

HouseNumberNode::~HouseNumberNode() = default;

FloorNode::FloorNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_FLOOR,
                       std::move(children),
                       MergeMode::kDefault) {}

FloorNode::~FloorNode() = default;

ApartmentNode::ApartmentNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_APT_NUM,
                       std::move(children),
                       MergeMode::kDefault) {}

ApartmentNode::~ApartmentNode() = default;

SubPremiseNode::SubPremiseNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_SUBPREMISE,
                       std::move(children),
                       MergeMode::kDefault) {}

SubPremiseNode::~SubPremiseNode() = default;

// Address are mergeable if one is a subset of the other one.
// Take the longer one. If both addresses have the same tokens apply a recursive
// strategy to merge the substructure.
StreetAddressNode::StreetAddressNode(SubcomponentsList children)
    : AddressComponentWithRewriter(ADDRESS_HOME_STREET_ADDRESS,
                                   std::move(children),
                                   MergeMode::kReplaceEmpty |
                                       MergeMode::kReplaceSubset |
                                       MergeMode::kDefault) {}

StreetAddressNode::~StreetAddressNode() = default;

std::vector<const re2::RE2*>
StreetAddressNode::GetParseRegularExpressionsByRelevance() const {
  auto* pattern_provider = StructuredAddressesRegExProvider::Instance();
  DCHECK(pattern_provider);
  const std::string country_code =
      base::UTF16ToUTF8(GetRootNode().GetValueForType(ADDRESS_HOME_COUNTRY));
  // Countries with custom hierarchies are not guaranteed to support street
  // names, house numbers, floors and apartment numbers. Therefore, we don't
  // apply the hardcoded fallback regular expressions.
  if (i18n_model_definition::IsCustomHierarchyAvailableForCountry(
          AddressCountryCode(country_code))) {
    return {};
  }
  return {pattern_provider->GetRegEx(RegEx::kParseHouseNumberStreetName,
                                     country_code),
          pattern_provider->GetRegEx(RegEx::kParseStreetNameHouseNumber,
                                     country_code),
          pattern_provider->GetRegEx(
              RegEx::kParseStreetNameHouseNumberSuffixedFloor, country_code),
          pattern_provider->GetRegEx(
              RegEx::kParseStreetNameHouseNumberSuffixedFloorAndApartmentRe,
              country_code)};
}

void StreetAddressNode::ParseValueAndAssignSubcomponentsByFallbackMethod() {
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

bool StreetAddressNode::HasNewerValuePrecedenceInMerging(
    const AddressComponent& newer_component) const {
  // If the newer component has a better verification status, use the newer one.
  if (IsLessSignificantVerificationStatus(
          GetVerificationStatus(), newer_component.GetVerificationStatus())) {
    return true;
  }

  // If the verification statuses are the same, do not use the newer component
  // if the older one has new lines but the newer one doesn't.
  if (GetVerificationStatus() == newer_component.GetVerificationStatus()) {
    if (GetValue().find('\n') != std::u16string::npos &&
        newer_component.GetValue().find('\n') == std::u16string::npos) {
      return false;
    }
    const int old_length = GetValue().size();
    const int new_length = newer_component.GetValue().size();
    // Only prefer the newer address if it increased in length. This is to avoid
    // constantly asking the user to update his profile just for formatting
    // purposes, which can negatively impact the Autofill experience.
    return old_length < new_length;
  }
  return false;
}

void StreetAddressNode::UnsetValue() {
  AddressComponent::UnsetValue();
  address_lines_.clear();
}

void StreetAddressNode::SetValue(std::u16string value,
                                 VerificationStatus status) {
  AddressComponent::SetValue(value, status);
  CalculateAddressLines();
}

void StreetAddressNode::CalculateAddressLines() {
  // Recalculate |address_lines_| after changing the street address.
  address_lines_ = base::SplitString(GetValue(), u"\n", base::TRIM_WHITESPACE,
                                     base::SPLIT_WANT_ALL);

  // If splitting of the address line results in more than 3 entries, join the
  // additional entries into the third line.
  if (address_lines_.size() > 3) {
    address_lines_[2] =
        base::JoinString(std::vector<std::u16string>(address_lines_.begin() + 2,
                                                     address_lines_.end()),
                         u" ");
    // Drop the addition address lines.
    while (address_lines_.size() > 3)
      address_lines_.pop_back();
  }
}

bool StreetAddressNode::IsValueValid() const {
  return !base::Contains(address_lines_, std::u16string());
}

std::u16string StreetAddressNode::GetValueForOtherSupportedType(
    FieldType field_type) const {
  // It is assumed below that field_type is an address line type.
  CHECK(IsSupportedType(field_type));
  return GetAddressLine(field_type);
}

std::u16string StreetAddressNode::GetAddressLine(FieldType type) const {
  const size_t line_index = AddressLineIndex(type);
  return address_lines_.size() > line_index ? address_lines_.at(line_index)
                                            : std::u16string();
}

// Implements support for setting the value of the individual address lines.
void StreetAddressNode::SetValueForOtherSupportedType(
    FieldType field_type,
    const std::u16string& value,
    const VerificationStatus& status) {
  CHECK(IsSupportedType(field_type));
  const size_t line_index = AddressLineIndex(field_type);
  // Make sure that there are enough address lines stored.
  if (line_index >= address_lines_.size()) {
    address_lines_.resize(line_index + 1);
  }
  const bool change = address_lines_[line_index] != value;
  if (change) {
    address_lines_[line_index] = value;
  }
  // Remove empty trailing address lines.
  while (!address_lines_.empty() && address_lines_.back().empty()) {
    address_lines_.pop_back();
  }
  // By calling the base class implementation, the recreation of the address
  // lines from the street address is omitted.
  if (change) {
    AddressComponent::SetValue(base::JoinString(address_lines_, u"\n"), status);
  }
}

void StreetAddressNode::PostAssignSanitization() {
  CalculateAddressLines();
}

const FieldTypeSet StreetAddressNode::GetAdditionalSupportedFieldTypes() const {
  constexpr FieldTypeSet additional_supported_field_types{
      ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_LINE3};
  return additional_supported_field_types;
}

// Country codes are mergeable if they are the same of if one is empty.
// For merging, pick the non-empty one.
CountryCodeNode::CountryCodeNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_COUNTRY,
                       std::move(children),
                       MergeMode::kReplaceEmpty |
                           MergeMode::kUseBetterOrNewerForSameValue) {}

CountryCodeNode::~CountryCodeNode() = default;

// DependentLocalities are mergeable when the tokens of one is a subset of the
// other one. Take the longer one.
DependentLocalityNode::DependentLocalityNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_DEPENDENT_LOCALITY,
                       std::move(children),
                       MergeMode::kReplaceSubset | MergeMode::kReplaceEmpty) {}

DependentLocalityNode::~DependentLocalityNode() = default;

// Cities are mergeable when the tokens of one is a subset of the other one.
// Take the shorter non-empty one.
CityNode::CityNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_CITY,
                       std::move(children),
                       MergeMode::kReplaceSubset | MergeMode::kReplaceEmpty) {}

CityNode::~CityNode() = default;

// States are mergeable when the tokens of one is a subset of the other one.
// Take the shorter non-empty one.
StateNode::StateNode(SubcomponentsList children)
    : AddressComponentWithRewriter(
          ADDRESS_HOME_STATE,
          std::move(children),
          kPickShorterIfOneContainsTheOther |
              MergeMode::kMergeBasedOnCanonicalizedValues | kReplaceEmpty) {}

StateNode::~StateNode() = default;

std::optional<std::u16string> StateNode::GetCanonicalizedValue() const {
  std::string country_code =
      base::UTF16ToUTF8(GetRootNode().GetValueForType(ADDRESS_HOME_COUNTRY));

  if (country_code.empty()) {
    return std::nullopt;
  }

  std::optional<AlternativeStateNameMap::CanonicalStateName>
      canonicalized_state_name = AlternativeStateNameMap::GetCanonicalStateName(
          country_code, GetValue());

  if (!canonicalized_state_name.has_value()) {
    return std::nullopt;
  }

  return canonicalized_state_name.value().value();
}

// Zips are mergeable when one is a substring of the other one.
// For merging, the shorter substring is taken.
PostalCodeNode::PostalCodeNode(SubcomponentsList children)
    : AddressComponentWithRewriter(
          ADDRESS_HOME_ZIP,
          std::move(children),
          MergeMode::kUseMostRecentSubstring | kReplaceEmpty) {}

PostalCodeNode::~PostalCodeNode() = default;

std::u16string PostalCodeNode::GetNormalizedValue() const {
  return NormalizeValue(GetValue(), /*keep_white_space=*/false);
}

std::u16string PostalCodeNode::GetValueForComparison(
    const std::u16string& value,
    const AddressComponent& other) const {
  return NormalizeAndRewrite(GetCommonCountry(other), value,
                             /*keep_white_space=*/false);
}

SortingCodeNode::SortingCodeNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_SORTING_CODE,
                       std::move(children),
                       MergeMode::kReplaceEmpty | kUseMostRecentSubstring) {}

SortingCodeNode::~SortingCodeNode() = default;

LandmarkNode::LandmarkNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_LANDMARK,
                       std::move(children),
                       MergeMode::kReplaceEmpty | kReplaceSubset) {}

LandmarkNode::~LandmarkNode() = default;

BetweenStreetsNode::BetweenStreetsNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_BETWEEN_STREETS,
                       std::move(children),
                       MergeMode::kReplaceEmpty | kReplaceSubset) {}

BetweenStreetsNode::~BetweenStreetsNode() = default;

BetweenStreets1Node::BetweenStreets1Node(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_BETWEEN_STREETS_1,
                       std::move(children),
                       MergeMode::kDefault) {}

BetweenStreets1Node::~BetweenStreets1Node() = default;

BetweenStreets2Node::BetweenStreets2Node(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_BETWEEN_STREETS_2,
                       std::move(children),
                       MergeMode::kDefault) {}

BetweenStreets2Node::~BetweenStreets2Node() = default;

AdminLevel2Node::AdminLevel2Node(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_ADMIN_LEVEL2,
                       std::move(children),
                       MergeMode::kReplaceEmpty | kReplaceSubset) {}

AdminLevel2Node::~AdminLevel2Node() = default;

AddressOverflowNode::AddressOverflowNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_OVERFLOW,
                       std::move(children),
                       MergeMode::kReplaceEmpty | kReplaceSubset) {}

AddressOverflowNode::~AddressOverflowNode() = default;

AddressOverflowAndLandmarkNode::AddressOverflowAndLandmarkNode(
    SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
                       std::move(children),
                       MergeMode::kReplaceEmpty | kReplaceSubset) {}

AddressOverflowAndLandmarkNode::~AddressOverflowAndLandmarkNode() = default;

BetweenStreetsOrLandmarkNode::BetweenStreetsOrLandmarkNode(
    SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
                       std::move(children),
                       MergeMode::kReplaceEmpty | kReplaceSubset) {}

BetweenStreetsOrLandmarkNode::~BetweenStreetsOrLandmarkNode() = default;

AddressNode::AddressNode() : AddressNode(SubcomponentsList{}) {}

AddressNode::AddressNode(const AddressNode& other) : AddressNode() {
  CopyFrom(other);
}

AddressNode& AddressNode::operator=(const AddressNode& other) {
  CopyFrom(other);
  return *this;
}

// Addresses are mergeable when all of their children are mergeable.
// Reformat the address from the children after merge if it changed.
AddressNode::AddressNode(SubcomponentsList children)
    : AddressComponent(ADDRESS_HOME_ADDRESS,
                       std::move(children),
                       MergeMode::kMergeChildrenAndReformatIfNeeded) {}

AddressNode::~AddressNode() = default;

bool AddressNode::WipeInvalidStructure() {
  // For i18n addresses, currently it is sufficient to wipe the structure
  // of the street address, because this is the only directly assignable value
  // that has a substructure.
  AddressComponent* street_address =
      GetNodeForType(ADDRESS_HOME_STREET_ADDRESS);
  DCHECK(street_address);
  if (street_address->WipeInvalidStructure()) {
    // Unset value for the root, which is the remaining non settings visible
    // node.
    UnsetValue();
    return true;
  }

  return false;
}

void AddressNode::MigrateLegacyStructure() {
  // If this component already has a verification status, no profile is regarded
  // as already verified.
  if (GetVerificationStatus() != VerificationStatus::kNoStatus)
    return;

  // Otherwise set the status of the subcomponents to observed if they already
  // have a value assigned. Note, those are all the tokens that are already
  // present in the unstructured address representation.
  for (AddressComponent* component : Subcomponents()) {
    if (!component->GetValue().empty() &&
        component->GetVerificationStatus() == VerificationStatus::kNoStatus) {
      component->SetValue(component->GetValue(), VerificationStatus::kObserved);
    }
  }
}

}  // namespace autofill
