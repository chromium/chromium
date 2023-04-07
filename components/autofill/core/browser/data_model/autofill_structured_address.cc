// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address.h"

#include <utility>
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_regex_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/browser/metrics/converge_to_extreme_length_address_metrics.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

std::u16string AddressComponentWithRewriter::RewriteValue(
    const std::u16string& value,
    const std::u16string& country_code) const {
  return RewriterCache::Rewrite(country_code.empty() ? u"US" : country_code,
                                value);
}

std::u16string AddressComponentWithRewriter::ValueForComparison(
    const AddressComponent& other) const {
  return RewriteValue(NormalizedValue(), GetCommonCountryForMerge(other));
}

StreetNameNode::StreetNameNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_STREET_NAME, parent, MergeMode::kDefault) {}

StreetNameNode::~StreetNameNode() = default;

DependentStreetNameNode::DependentStreetNameNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_DEPENDENT_STREET_NAME,
                       parent,
                       MergeMode::kDefault) {}

DependentStreetNameNode::~DependentStreetNameNode() = default;

StreetAndDependentStreetNameNode::StreetAndDependentStreetNameNode(
    AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME,
                       parent,
                       MergeMode::kDefault) {}

StreetAndDependentStreetNameNode::~StreetAndDependentStreetNameNode() = default;

HouseNumberNode::HouseNumberNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_HOUSE_NUMBER, parent, MergeMode::kDefault) {
}

HouseNumberNode::~HouseNumberNode() = default;

PremiseNode::PremiseNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_PREMISE_NAME, parent, MergeMode::kDefault) {
}

PremiseNode::~PremiseNode() = default;

FloorNode::FloorNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_FLOOR, parent, MergeMode::kDefault) {}

FloorNode::~FloorNode() = default;

ApartmentNode::ApartmentNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_APT_NUM, parent, MergeMode::kDefault) {}

ApartmentNode::~ApartmentNode() = default;

SubPremiseNode::SubPremiseNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_SUBPREMISE, parent, MergeMode::kDefault) {}

SubPremiseNode::~SubPremiseNode() = default;

// Address are mergeable if one is a subset of the other one.
// Take the longer one. If both addresses have the same tokens apply a recursive
// strategy to merge the substructure.
StreetAddressNode::StreetAddressNode(AddressComponent* parent)
    : AddressComponentWithRewriter(ADDRESS_HOME_STREET_ADDRESS,
                                   parent,
                                   MergeMode::kReplaceEmpty |
                                       MergeMode::kReplaceSubset |
                                       MergeMode::kDefault) {}

StreetAddressNode::~StreetAddressNode() = default;

std::vector<const re2::RE2*>
StreetAddressNode::GetParseRegularExpressionsByRelevance() const {
  auto* pattern_provider = StructuredAddressesRegExProvider::Instance();
  DCHECK(pattern_provider);
  return {pattern_provider->GetRegEx(RegEx::kParseHouseNumberStreetName),
          pattern_provider->GetRegEx(RegEx::kParseStreetNameHouseNumber),
          pattern_provider->GetRegEx(
              RegEx::kParseStreetNameHouseNumberSuffixedFloor),
          pattern_provider->GetRegEx(
              RegEx::kParseStreetNameHouseNumberSuffixedFloorAndAppartmentRe)};
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
    // By default, we prefer the newer street address over the old one in case
    // of a tie between verification statuses.
    if (!base::FeatureList::IsEnabled(
            features::kAutofillConvergeToExtremeLengthStreetAddress)) {
      return true;
    }
    // If street lengths are equal, prefer the old value. This is to avoid
    // constantly asking the user to update his profile just for formatting
    // purposes, which can negatively impact the Autofill experience.
    if (old_length == new_length) {
      return false;
    }
    // Otherwise, prefer the longer or shorter street address depending on the
    // feature `kAutofillConvergeToExtremeLengthStreetAddress` parameterization.
    const bool has_newer_value_precedence =
        features::kAutofillConvergeToLonger.Get() ? old_length < new_length
                                                  : old_length > new_length;
    autofill_metrics::LogAddressUpdateLengthConvergenceStatus(
        has_newer_value_precedence);
    return has_newer_value_precedence;
  }
  return false;
}

std::u16string StreetAddressNode::GetBestFormatString() const {
  std::string country_code =
      base::UTF16ToUTF8(GetRootNode().GetValueForType(ADDRESS_HOME_COUNTRY));

  if (country_code == "BR") {
    return u"${ADDRESS_HOME_STREET_NAME}${ADDRESS_HOME_HOUSE_NUMBER;, }"
           u"${ADDRESS_HOME_FLOOR;, ;º andar}${ADDRESS_HOME_APT_NUM;, apto ;}";
  }

  if (country_code == "DE") {
    return u"${ADDRESS_HOME_STREET_NAME} ${ADDRESS_HOME_HOUSE_NUMBER}"
           u"${ADDRESS_HOME_FLOOR;, ;. Stock}${ADDRESS_HOME_APT_NUM;, ;. "
           u"Wohnung}";
  }

  if (country_code == "MX") {
    return u"${ADDRESS_HOME_STREET_NAME} ${ADDRESS_HOME_HOUSE_NUMBER}"
           u"${ADDRESS_HOME_FLOOR; - Piso ;}${ADDRESS_HOME_APT_NUM; - ;}";
  }
  if (country_code == "ES") {
    return u"${ADDRESS_HOME_STREET_NAME} ${ADDRESS_HOME_HOUSE_NUMBER}"
           u"${ADDRESS_HOME_FLOOR;, ;º}${ADDRESS_HOME_APT_NUM;, ;ª}";
  }
  // Use the format for US/UK as the default.
  return u"${ADDRESS_HOME_HOUSE_NUMBER} ${ADDRESS_HOME_STREET_NAME} "
         u"${ADDRESS_HOME_FLOOR;FL } ${ADDRESS_HOME_APT_NUM;APT }";
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

bool StreetAddressNode::ConvertAndGetTheValueForAdditionalFieldTypeName(
    const std::string& type_name,
    std::u16string* value) const {
  if (type_name == AutofillType::ServerFieldTypeToString(ADDRESS_HOME_LINE1)) {
    if (value) {
      *value =
          address_lines_.size() > 0 ? address_lines_.at(0) : std::u16string();
    }
    return true;
  }
  if (type_name == AutofillType::ServerFieldTypeToString(ADDRESS_HOME_LINE2)) {
    if (value) {
      *value =
          address_lines_.size() > 1 ? address_lines_.at(1) : std::u16string();
    }
    return true;
  }
  if (type_name == AutofillType::ServerFieldTypeToString(ADDRESS_HOME_LINE3)) {
    if (value) {
      *value =
          address_lines_.size() > 2 ? address_lines_.at(2) : std::u16string();
    }
    return true;
  }

  return false;
}

// Implements support for setting the value of the individual address lines.
bool StreetAddressNode::ConvertAndSetValueForAdditionalFieldTypeName(
    const std::string& type_name,
    const std::u16string& value,
    const VerificationStatus& status) {
  size_t index = 0;
  if (type_name == AutofillType::ServerFieldTypeToString(ADDRESS_HOME_LINE1)) {
    index = 0;
  } else if (type_name ==
             AutofillType::ServerFieldTypeToString(ADDRESS_HOME_LINE2)) {
    index = 1;
  } else if (type_name ==
             AutofillType::ServerFieldTypeToString(ADDRESS_HOME_LINE3)) {
    index = 2;
  } else {
    return false;
  }

  // Make sure that there are three address lines stored.
  if (index >= address_lines_.size())
    address_lines_.resize(index + 1, std::u16string());

  bool change = address_lines_[index] != value;
  if (change)
    address_lines_[index] = value;

  while (!address_lines_.empty() && address_lines_.back().empty())
    address_lines_.pop_back();

  // By calling the base class implementation, the recreation of the address
  // lines from the street address is omitted.
  if (change) {
    AddressComponent::SetValue(base::JoinString(address_lines_, u"\n"), status);
  }

  return true;
}

void StreetAddressNode::PostAssignSanitization() {
  CalculateAddressLines();
}

void StreetAddressNode::GetAdditionalSupportedFieldTypes(
    ServerFieldTypeSet* supported_types) const {
  supported_types->insert(ADDRESS_HOME_LINE1);
  supported_types->insert(ADDRESS_HOME_LINE2);
  supported_types->insert(ADDRESS_HOME_LINE3);
}

// Country codes are mergeable if they are the same of if one is empty.
// For merging, pick the non-empty one.
CountryCodeNode::CountryCodeNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_COUNTRY,
                       parent,
                       MergeMode::kReplaceEmpty |
                           MergeMode::kUseBetterOrNewerForSameValue) {}

CountryCodeNode::~CountryCodeNode() = default;

// DependentLocalities are mergeable when the tokens of one is a subset of the
// other one. Take the longer one.
DependentLocalityNode::DependentLocalityNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_DEPENDENT_LOCALITY,
                       parent,
                       MergeMode::kReplaceSubset | MergeMode::kReplaceEmpty) {}

DependentLocalityNode::~DependentLocalityNode() = default;

// Cities are mergeable when the tokens of one is a subset of the other one.
// Take the shorter non-empty one.
CityNode::CityNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_CITY,
                       parent,
                       MergeMode::kReplaceSubset | MergeMode::kReplaceEmpty) {}

CityNode::~CityNode() = default;

// States are mergeable when the tokens of one is a subset of the other one.
// Take the shorter non-empty one.
StateNode::StateNode(AddressComponent* parent)
    : AddressComponentWithRewriter(
          ADDRESS_HOME_STATE,
          parent,
          kPickShorterIfOneContainsTheOther |
              (base::FeatureList::IsEnabled(
                   features::kAutofillUseAlternativeStateNameMap)
                   ? MergeMode::kMergeBasedOnCanonicalizedValues
                   : 0) |
              kReplaceEmpty) {}

StateNode::~StateNode() = default;

absl::optional<std::u16string> StateNode::GetCanonicalizedValue() const {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillUseAlternativeStateNameMap)) {
    return absl::nullopt;
  }

  std::string country_code =
      base::UTF16ToUTF8(GetRootNode().GetValueForType(ADDRESS_HOME_COUNTRY));

  if (country_code.empty()) {
    return absl::nullopt;
  }

  absl::optional<AlternativeStateNameMap::CanonicalStateName>
      canonicalized_state_name = AlternativeStateNameMap::GetCanonicalStateName(
          country_code, GetValue());

  if (!canonicalized_state_name.has_value()) {
    return absl::nullopt;
  }

  return canonicalized_state_name.value().value();
}

// Zips are mergeable when one is a substring of the other one.
// For merging, the shorter substring is taken.
PostalCodeNode::PostalCodeNode(AddressComponent* parent)
    : AddressComponentWithRewriter(
          ADDRESS_HOME_ZIP,
          parent,
          MergeMode::kUseMostRecentSubstring | kReplaceEmpty) {}

PostalCodeNode::~PostalCodeNode() = default;

std::u16string PostalCodeNode::NormalizedValue() const {
  return NormalizeValue(GetValue(), /*keep_white_space=*/false);
}

SortingCodeNode::SortingCodeNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_SORTING_CODE,
                       parent,
                       MergeMode::kReplaceEmpty | kUseMostRecentSubstring) {}

SortingCodeNode::~SortingCodeNode() = default;

AddressNode::AddressNode() : AddressNode(nullptr) {}

AddressNode::AddressNode(const AddressNode& other) : AddressNode() {
  CopyFrom(other);
}

AddressNode& AddressNode::operator=(const AddressNode& other) {
  CopyFrom(other);
  return *this;
}

// Addresses are mergeable when all of their children are mergeable.
// Reformat the address from the children after merge if it changed.
AddressNode::AddressNode(AddressComponent* parent)
    : AddressComponent(ADDRESS_HOME_ADDRESS,
                       parent,
                       MergeMode::kMergeChildrenAndReformatIfNeeded) {}

AddressNode::~AddressNode() = default;

bool AddressNode::WipeInvalidStructure() {
  // For structured addresses, currently it is sufficient to wipe the structure
  // of the street address, because this is the only directly assignable value
  // that has a substructure.
  return street_address_.WipeInvalidStructure();
}

void AddressNode::MigrateLegacyStructure(bool is_verified_profile) {
  // If this component already has a verification status, no profile is regarded
  // as already verified.
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

}  // namespace autofill
