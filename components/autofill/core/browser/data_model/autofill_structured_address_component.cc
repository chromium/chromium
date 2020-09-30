// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_constants.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace structured_address {

AddressComponent::AddressComponent(ServerFieldType storage_type,
                                   AddressComponent* parent,
                                   std::vector<AddressComponent*> subcomponents,
                                   unsigned int merge_mode)
    : value_verification_status_(VerificationStatus::kNoStatus),
      storage_type_(storage_type),
      subcomponents_(subcomponents),
      parent_(parent),
      merge_mode_(merge_mode) {}

AddressComponent::~AddressComponent() = default;

ServerFieldType AddressComponent::GetStorageType() const {
  return storage_type_;
}

std::string AddressComponent::GetStorageTypeName() const {
  return AutofillType(storage_type_).ToString();
}

AddressComponent& AddressComponent::operator=(const AddressComponent& right) {
  DCHECK(GetStorageType() == right.GetStorageType());
  if (this == &right)
    return *this;

  if (right.IsValueAssigned()) {
    value_ = right.value_;
    value_verification_status_ = right.value_verification_status_;
    sorted_normalized_tokens_ = right.sorted_normalized_tokens_;
  } else {
    UnsetValue();
  }

  DCHECK(right.subcomponents_.size() == subcomponents_.size());

  for (size_t i = 0; i < right.subcomponents_.size(); i++)
    *subcomponents_[i] = *right.subcomponents_[i];

  PostAssignSanitization();

  return *this;
}

bool AddressComponent::operator==(const AddressComponent& right) const {
  if (this == &right)
    return true;

  if (GetStorageType() != right.GetStorageType())
    return false;

  if (GetValue() != right.GetValue() ||
      value_verification_status_ != right.value_verification_status_) {
    return false;
  }

  DCHECK(right.subcomponents_.size() == subcomponents_.size());
  for (size_t i = 0; i < right.subcomponents_.size(); i++)
    if (!(*subcomponents_[i] == *right.subcomponents_[i]))
      return false;
  return true;
}

bool AddressComponent::operator!=(const AddressComponent& right) const {
  return !(*this == right);
}

bool AddressComponent::IsAtomic() const {
  return subcomponents_.empty();
}

bool AddressComponent::IsValueValid() const {
  return true;
}

bool AddressComponent::IsValueForTypeValid(const std::string& field_type_name,
                                           bool wipe_if_not) {
  bool validity_status;
  if (GetIsValueForTypeValidIfPossible(field_type_name, &validity_status,
                                       wipe_if_not))
    return validity_status;
  return false;
}

bool AddressComponent::IsValueForTypeValid(ServerFieldType field_type,
                                           bool wipe_if_not) {
  return IsValueForTypeValid(AutofillType(field_type).ToString(), wipe_if_not);
}

bool AddressComponent::GetIsValueForTypeValidIfPossible(
    const std::string& field_type_name,
    bool* validity_status,
    bool wipe_if_not) {
  if (field_type_name == GetStorageTypeName()) {
    *validity_status = IsValueValid();
    if (!(*validity_status) && wipe_if_not)
      UnsetValue();
    return true;
  }

  for (auto* subcomponent : subcomponents_) {
    if (subcomponent->GetIsValueForTypeValidIfPossible(
            field_type_name, validity_status, wipe_if_not))
      return true;
  }
  return false;
}

VerificationStatus AddressComponent::GetVerificationStatus() const {
  return value_verification_status_;
}

const base::string16& AddressComponent::GetValue() const {
  if (value_.has_value())
    return value_.value();
  return base::EmptyString16();
}

bool AddressComponent::IsValueAssigned() const {
  return value_.has_value();
}

void AddressComponent::SetValue(base::string16 value,
                                VerificationStatus status) {
  value_ = std::move(value);
  value_verification_status_ = status;
}

void AddressComponent::UnsetValue() {
  value_.reset();
  value_verification_status_ = VerificationStatus::kNoStatus;
  sorted_normalized_tokens_.reset();
}

void AddressComponent::GetSupportedTypes(
    ServerFieldTypeSet* supported_types) const {
  // A proper AddressComponent tree contains every type only once.
  DCHECK(supported_types->find(storage_type_) == supported_types->end())
      << "The AddressComponent already contains a node that supports this "
         "type: "
      << storage_type_;
  supported_types->insert(storage_type_);
  GetAdditionalSupportedFieldTypes(supported_types);
  for (auto* subcomponent : subcomponents_)
    subcomponent->GetSupportedTypes(supported_types);
}

bool AddressComponent::ConvertAndSetValueForAdditionalFieldTypeName(
    const std::string& field_type_name,
    const base::string16& value,
    const VerificationStatus& status) {
  return false;
}

bool AddressComponent::ConvertAndGetTheValueForAdditionalFieldTypeName(
    const std::string& field_type_name,
    base::string16* value) const {
  return false;
}

base::string16 AddressComponent::GetBestFormatString() const {
  // If the component is atomic, the format string is just the value.
  if (IsAtomic())
    return base::ASCIIToUTF16(GetPlaceholderToken(GetStorageTypeName()));

  // Otherwise, the canonical format string is the concatenation of all
  // subcomponents by their natural order.
  std::vector<std::string> format_pieces;
  for (const auto* subcomponent : subcomponents_) {
    std::string format_piece = GetPlaceholderToken(
        AutofillType(subcomponent->GetStorageType()).ToString());
    format_pieces.emplace_back(std::move(format_piece));
  }
  return base::ASCIIToUTF16(base::JoinString(format_pieces, " "));
}

std::vector<ServerFieldType> AddressComponent::GetSubcomponentTypes() const {
  std::vector<ServerFieldType> subcomponent_types;
  subcomponent_types.reserve(subcomponents_.size());
  for (const auto* subcomponent : subcomponents_) {
    subcomponent_types.emplace_back(subcomponent->GetStorageType());
  }
  return subcomponent_types;
}

bool AddressComponent::SetValueForTypeIfPossible(
    const ServerFieldType& type,
    const base::string16& value,
    const VerificationStatus& verification_status,
    bool invalidate_child_nodes,
    bool invalidate_parent_nodes) {
  return SetValueForTypeIfPossible(AutofillType(type).ToString(), value,
                                   verification_status, invalidate_child_nodes,
                                   invalidate_parent_nodes);
}

bool AddressComponent::SetValueForTypeIfPossible(
    const ServerFieldType& type,
    const std::string& value,
    const VerificationStatus& verification_status,
    bool invalidate_child_nodes,
    bool invalidate_parent_nodes) {
  return SetValueForTypeIfPossible(type, base::UTF8ToUTF16(value),
                                   verification_status, invalidate_child_nodes,
                                   invalidate_parent_nodes);
}

bool AddressComponent::SetValueForTypeIfPossible(
    const std::string& type_name,
    const base::string16& value,
    const VerificationStatus& verification_status,
    bool invalidate_child_nodes,
    bool invalidate_parent_nodes) {
  bool value_set = false;
  // If the type is the storage type of the component, it can directly be
  // returned.
  if (type_name == GetStorageTypeName()) {
    SetValue(value, verification_status);
    value_set = true;
  } else if (ConvertAndSetValueForAdditionalFieldTypeName(
                 type_name, value, verification_status)) {
    // The conversion using a field type was successful.
    value_set = true;
  }

  if (value_set) {
    if (invalidate_child_nodes)
      UnsetSubcomponents();
    return true;
  }

  // Finally, probe if the type is supported by one of the subcomponents.
  for (auto* subcomponent : subcomponents_) {
    if (subcomponent->SetValueForTypeIfPossible(
            type_name, value, verification_status, invalidate_child_nodes,
            invalidate_parent_nodes)) {
      if (invalidate_parent_nodes)
        UnsetValue();
      return true;
    }
  }

  return false;
}

bool AddressComponent::SetValueForTypeIfPossible(
    const std::string& type_name,
    const std::string& value,
    const VerificationStatus& verification_status,
    bool invalidate_child_nodes,
    bool invalidate_parent_nodes) {
  return SetValueForTypeIfPossible(type_name, base::UTF8ToUTF16(value),
                                   verification_status, invalidate_child_nodes,
                                   invalidate_parent_nodes);
}

void AddressComponent::UnsetAddressComponentAndItsSubcomponents() {
  UnsetValue();
  UnsetSubcomponents();
}

void AddressComponent::UnsetSubcomponents() {
  for (auto* component : subcomponents_)
    component->UnsetAddressComponentAndItsSubcomponents();
}

bool AddressComponent::GetValueAndStatusForTypeIfPossible(
    const ServerFieldType& type,
    base::string16* value,
    VerificationStatus* status) const {
  return GetValueAndStatusForTypeIfPossible(AutofillType(type).ToString(),
                                            value, status);
}

bool AddressComponent::GetValueAndStatusForTypeIfPossible(
    const std::string& type_name,
    base::string16* value,
    VerificationStatus* status) const {
  // If the value is the storage type, it can be simply returned.
  if (type_name == GetStorageTypeName()) {
    if (value)
      *value = value_.value_or(base::string16());
    if (status)
      *status = GetVerificationStatus();
    return true;
  }

  // Otherwise, probe if it is a supported field type that can be converted.
  if (this->ConvertAndGetTheValueForAdditionalFieldTypeName(type_name, value)) {
    if (status)
      *status = GetVerificationStatus();
    return true;
  }

  // Finally, try to retrieve the value from one of the subcomponents.
  for (const auto* subcomponent : subcomponents_) {
    if (subcomponent->GetValueAndStatusForTypeIfPossible(type_name, value,
                                                         status))
      return true;
  }
  return false;
}

base::string16 AddressComponent::GetValueForType(
    const ServerFieldType& type) const {
  return GetValueForType(AutofillType(type).ToString());
}

base::string16 AddressComponent::GetValueForType(
    const std::string& type_name) const {
  base::string16 value;
  bool success = GetValueAndStatusForTypeIfPossible(type_name, &value, nullptr);
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  DCHECK(success || type_name == AutofillType(NAME_HONORIFIC_PREFIX).ToString())
      << type_name;
  return value;
}

VerificationStatus AddressComponent::GetVerificationStatusForType(
    const ServerFieldType& type) const {
  return GetVerificationStatusForType(AutofillType(type).ToString());
}

VerificationStatus AddressComponent::GetVerificationStatusForType(
    const std::string& type_name) const {
  VerificationStatus status = VerificationStatus::kNoStatus;
  bool success =
      GetValueAndStatusForTypeIfPossible(type_name, nullptr, &status);
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  DCHECK(success ||
         type_name == AutofillType(NAME_HONORIFIC_PREFIX).ToString());
  return status;
}

bool AddressComponent::UnsetValueForTypeIfSupported(
    const ServerFieldType& type) {
  if (type == storage_type_) {
    UnsetAddressComponentAndItsSubcomponents();
    return true;
  }

  for (auto* subcomponent : subcomponents_) {
    if (subcomponent->UnsetValueForTypeIfSupported(type))
      return true;
  }

  return false;
}

bool AddressComponent::ParseValueAndAssignSubcomponentsByMethod() {
  return false;
}

std::vector<const re2::RE2*>
AddressComponent::GetParseRegularExpressionsByRelevance() const {
  return {};
}

void AddressComponent::ParseValueAndAssignSubcomponents() {
  // Set the values of all subcomponents to the empty string and set the
  // verification status to kParsed.
  for (auto* subcomponent : subcomponents_)
    subcomponent->SetValue(base::string16(), VerificationStatus::kParsed);

  // First attempt, try to parse by method.
  if (ParseValueAndAssignSubcomponentsByMethod())
    return;

  // Second attempt, try to parse by expressions.
  if (ParseValueAndAssignSubcomponentsByRegularExpressions())
    return;

  // As a final fallback, parse using the fallback method.
  ParseValueAndAssignSubcomponentsByFallbackMethod();
}

bool AddressComponent::ParseValueAndAssignSubcomponentsByRegularExpressions() {
  for (const auto* parse_expression : GetParseRegularExpressionsByRelevance()) {
    if (!parse_expression)
      continue;
    if (ParseValueAndAssignSubcomponentsByRegularExpression(GetValue(),
                                                            parse_expression))
      return true;
  }
  return false;
}

bool AddressComponent::ParseValueAndAssignSubcomponentsByRegularExpression(
    const base::string16& value,
    const RE2* parse_expression) {
  std::map<std::string, std::string> result_map;
  if (ParseValueByRegularExpression(base::UTF16ToUTF8(value), parse_expression,
                                    &result_map)) {
    // Parsing was successful and results from the result map can be written
    // to the structure.
    for (const auto& result_entry : result_map) {
      const std::string& field_type = result_entry.first;
      base::string16 field_value = base::UTF8ToUTF16(result_entry.second);
      // Do not reassign the value of this node.
      if (field_type == GetStorageTypeName())
        continue;
      // crbug.com(1113617): Honorifics are temporarily disabled.
      if (field_type == AutofillType(NAME_HONORIFIC_PREFIX).ToString())
        continue;
      bool success = SetValueForTypeIfPossible(field_type, field_value,
                                               VerificationStatus::kParsed);
      // Setting the value should always work unless the regular expression is
      // invalid.
      DCHECK(success);
    }
    return true;
  }
  return false;
}

void AddressComponent::ParseValueAndAssignSubcomponentsByFallbackMethod() {
  // There is nothing to do for an atomic component.
  if (IsAtomic())
    return;

  // An empty string is trivially parsable.
  if (GetValue().empty())
    return;

  // Split the string by spaces.
  std::vector<base::string16> space_separated_tokens =
      base::SplitString(GetValue(), base::UTF8ToUTF16(" "),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  auto token_iterator = space_separated_tokens.begin();
  auto subcomponent_types = GetSubcomponentTypes();

  // Assign one space-separated token each to all but the last subcomponent.
  for (size_t i = 0; (i + 1) < subcomponent_types.size(); i++) {
    // If there are no tokens left, parsing is done.
    if (token_iterator == space_separated_tokens.end())
      return;
    // Set the current token to the type and advance the token iterator.
    bool success = SetValueForTypeIfPossible(
        subcomponent_types[i], *token_iterator, VerificationStatus::kParsed);
    // By design, setting the value should never fail.
    DCHECK(success);
    token_iterator++;
  }

  // Collect all remaining tokens in the last subcomponent.
  base::string16 remaining_tokens = base::JoinString(
      std::vector<base::string16>(token_iterator, space_separated_tokens.end()),
      base::ASCIIToUTF16(" "));
  // By design, it should be possible to assign the value unless the regular
  // expression is wrong.
  bool success = SetValueForTypeIfPossible(
      subcomponent_types.back(), remaining_tokens, VerificationStatus::kParsed);
  DCHECK(success);
}

void AddressComponent::FormatValueFromSubcomponents() {
  // Get the most suited format string.
  base::string16 format_string = GetBestFormatString();

  // Perform the following steps on a copy of the format string.
  // * Replace all the placeholders of the form ${TYPE_NAME} with the
  // corresponding value.
  // * Strip away double spaces as they may occur after replacing a placeholder
  // with an empty value.

  base::string16 result = ReplacePlaceholderTypesWithValues(format_string);
  result = base::CollapseWhitespace(result, /*trim_line_breaks=*/false);
  SetValue(result, VerificationStatus::kFormatted);
}

base::string16 AddressComponent::ReplacePlaceholderTypesWithValues(
    const base::string16& format) const {
  // Replaces placeholders using the following rules.
  // Assumptions: Placeholder values are not nested.
  //
  // * Search for a substring of the form "{$[^}]*}".
  // The substring can contain semicolon-separated tokens. The first token is
  // always the type name. If present, the second token is a prefix that is only
  // inserted if the corresponding value is not empty. Accordingly, the third
  // token is a suffix.
  //
  // * Check if this substring is a supported type of this component.
  //
  // * If yes, replace the substring with the corresponding value.
  //
  // * If the corresponding value is empty, return false.

  auto control_parmater = base::ASCIIToUTF16("$").at(0);
  auto control_parmater_open_delimitor = base::ASCIIToUTF16("{").at(0);
  auto control_parmater_close_delimitor = base::ASCIIToUTF16("}").at(0);

  // Create a result vector for the tokens that are joined in the end.
  std::vector<base::StringPiece16> result_pieces;

  // Store the token pieces that are joined in the end.
  std::vector<base::string16> inserted_values;
  inserted_values.reserve(20);

  // Use a StringPiece rather than the string since this allows for getting
  // cheap views onto substrings.
  const base::StringPiece16 format_piece = format;

  bool started_control_sequence = false;
  // Track until which index the format string was fully processed.
  size_t processed_until_index = 0;

  for (size_t i = 0; i < format_piece.size(); ++i) {
    // Check if a control sequence is started by '${'
    if (format_piece[i] == control_parmater && i < format_piece.size() - 1 &&
        format_piece[i + 1] == control_parmater_open_delimitor) {
      // A control sequence is started.
      started_control_sequence = true;
      // Append the preceding string since it can't be a valid placeholder.
      if (i > 0) {
        inserted_values.emplace_back(format_piece.substr(
            processed_until_index, i - processed_until_index));
      }
      processed_until_index = i;
      ++i;
    } else if (started_control_sequence &&
               format_piece[i] == control_parmater_close_delimitor) {
      // The control sequence came to an end.
      started_control_sequence = false;
      size_t placeholder_start = processed_until_index + 2;
      base::string16 placeholder(
          format_piece.substr(placeholder_start, i - placeholder_start));

      std::vector<base::string16> placeholder_tokens =
          base::SplitString(placeholder, base::ASCIIToUTF16(";"),
                            base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      DCHECK(placeholder_tokens.size() > 0);

      // By convention, the first token is the type of the placeholder.
      base::string16 type_name = placeholder_tokens.at(0);
      // If present, the second token is the prefix.
      base::string16 prefix = placeholder_tokens.size() > 1
                                  ? placeholder_tokens.at(1)
                                  : base::string16();
      // And the third token the suffix.
      base::string16 suffix = placeholder_tokens.size() > 2
                                  ? placeholder_tokens.at(2)
                                  : base::string16();

      base::string16 value;
      if (GetValueAndStatusForTypeIfPossible(base::UTF16ToASCII(type_name),
                                             &value, nullptr)) {
        // The type is valid and should be substituted.
        if (!value.empty()) {
          // Add the prefix if present.
          if (!prefix.empty())
            inserted_values.emplace_back(std::move(prefix));

          // Add the substituted value.
          inserted_values.emplace_back(std::move(value));
          // Add the suffix if present.
          if (!suffix.empty())
            inserted_values.emplace_back(std::move(suffix));
        }
      } else {
        // Append the control sequence as it is, because the type is not
        // supported by the component tree.
        inserted_values.emplace_back(format_piece.substr(
            processed_until_index, i - processed_until_index + 1));
      }
      processed_until_index = i + 1;
    }
  }
  // Append the rest of the string.
  inserted_values.emplace_back(
      format_piece.substr(processed_until_index, base::string16::npos));

  // Build the final result.
  return base::JoinString(inserted_values, base::ASCIIToUTF16(""));
}

bool AddressComponent::CompleteFullTree() {
  int max_nodes_on_root_to_leaf_path =
      GetRootNode().MaximumNumberOfAssignedAddressComponentsOnNodeToLeafPaths();
  // With more than one node the tree cannot be completed.
  switch (max_nodes_on_root_to_leaf_path) {
    // An empty tree is already complete.
    case 0:
      return true;
    // With a single node, the tree is completable.
    case 1:
      GetRootNode().RecursivelyCompleteTree();
      return true;
    // In any other case, the tree is not completable.
    default:
      return false;
  }
}

void AddressComponent::RecursivelyCompleteTree() {
  if (IsAtomic())
    return;

  // If the value is assigned, parse the subcomponents from the value.
  if (!GetValue().empty() &&
      MaximumNumberOfAssignedAddressComponentsOnNodeToLeafPaths() == 1)
    ParseValueAndAssignSubcomponents();

  // First call completion on all subcomponents.
  for (auto* subcomponent : subcomponents_)
    subcomponent->RecursivelyCompleteTree();

  // Finally format the value from the sucomponents if it is not already
  // assigned.
  if (GetValue().empty())
    FormatValueFromSubcomponents();
}

int AddressComponent::
    MaximumNumberOfAssignedAddressComponentsOnNodeToLeafPaths() const {
  int result = 0;

  for (auto* subcomponent : subcomponents_) {
    result = std::max(
        result,
        subcomponent
            ->MaximumNumberOfAssignedAddressComponentsOnNodeToLeafPaths());
  }

  // Only count non-empty nodes.
  if (!GetValue().empty())
    ++result;

  return result;
}

bool AddressComponent::IsTreeCompletable() {
  // An empty tree is also a completable tree.
  return MaximumNumberOfAssignedAddressComponentsOnNodeToLeafPaths() <= 1;
}

const AddressComponent& AddressComponent::GetRootNode() const {
  if (!parent_)
    return *this;
  return parent_->GetRootNode();
}

AddressComponent& AddressComponent::GetRootNode() {
  return const_cast<AddressComponent&>(
      const_cast<const AddressComponent*>(this)->GetRootNode());
}

void AddressComponent::RecursivelyUnsetParsedAndFormattedValues() {
  if (IsValueAssigned() &&
      (GetVerificationStatus() == VerificationStatus::kFormatted ||
       GetVerificationStatus() == VerificationStatus::kParsed))
    UnsetValue();

  for (auto* component : subcomponents_)
    component->RecursivelyUnsetParsedAndFormattedValues();
}

void AddressComponent::RecursivelyUnsetSubcomponents() {
  for (auto* subcomponent : subcomponents_) {
    subcomponent->UnsetValue();
    subcomponent->RecursivelyUnsetSubcomponents();
  }
}

void AddressComponent::UnsetParsedAndFormattedValuesInEntireTree() {
  GetRootNode().RecursivelyUnsetParsedAndFormattedValues();
}

void AddressComponent::MergeVerificationStatuses(
    const AddressComponent& newer_component) {
  if (IsValueAssigned() && (GetValue() == newer_component.GetValue()) &&
      (GetVerificationStatus() < newer_component.GetVerificationStatus())) {
    value_verification_status_ = newer_component.GetVerificationStatus();
  }

  DCHECK(newer_component.subcomponents_.size() == subcomponents_.size());
  for (size_t i = 0; i < newer_component.subcomponents_.size(); i++) {
    subcomponents_[i]->MergeVerificationStatuses(
        *newer_component.subcomponents_.at(i));
  }
}

const std::vector<AddressToken> AddressComponent::GetSortedTokens() const {
  return TokenizeValue(GetValue());
}

bool AddressComponent::IsMergeableWithComponent(
    const AddressComponent& newer_component) const {
  const base::string16 value = ValueForComparison();
  const base::string16 value_newer = newer_component.ValueForComparison();

  // If both components are the same, there is nothing to do.
  if (*this == newer_component)
    return true;

  if ((merge_mode_ & kReplaceEmpty) && (value.empty() || value_newer.empty())) {
    return true;
  }

  if (merge_mode_ & kUseBetterOrNewerForSameValue) {
    if (base::ToUpperASCII(value) == base::ToUpperASCII(value_newer)) {
      return true;
    }
  }

  SortedTokenComparisonResult token_comparison_result =
      CompareSortedTokens(value, value_newer);

  if ((merge_mode_ & (kRecursivelyMergeTokenEquivalentValues |
                      kRecursivelyMergeSingleTokenSubset)) &&
      token_comparison_result.status == MATCH) {
    return true;
  }

  if ((merge_mode_ & (kReplaceSubset | kReplaceSuperset)) &&
      (token_comparison_result.OneIsSubset() ||
       token_comparison_result.status == MATCH)) {
    return true;
  }

  if ((merge_mode_ & kRecursivelyMergeSingleTokenSubset) &&
      token_comparison_result.IsSingleTokenSuperset()) {
    return true;
  }

  if (merge_mode_ == kUseNewerIfDifferent)
    return true;

  // If the one value is a substring of the other, use the substring of the
  // corresponding mode is active.
  if ((merge_mode_ & kUseMostRecentSubstring) &&
      (value.find(value_newer) != base::string16::npos ||
       value_newer.find(value) != base::string16::npos)) {
    return true;
  }

  if ((merge_mode_ & kPickShorterIfOneContainsTheOther) &&
      token_comparison_result.ContainEachOther()) {
    return true;
  }

  if (merge_mode_ == kMergeChildrenAndReformat) {
    bool is_mergeable = true;
    DCHECK(newer_component.subcomponents_.size() == subcomponents_.size());
    for (size_t i = 0; i < newer_component.subcomponents_.size(); i++) {
      if (!subcomponents_[i]->IsMergeableWithComponent(
              *newer_component.subcomponents_[i])) {
        is_mergeable = false;
        break;
      }
    }
    if (is_mergeable)
      return true;
  }
  return false;
}

bool AddressComponent::MergeWithComponent(
    const AddressComponent& newer_component,
    bool newer_was_more_recently_used) {
  // If both components are the same, there is nothing to do.

  const base::string16 value = ValueForComparison();
  const base::string16 value_newer = newer_component.ValueForComparison();

  if (*this == newer_component)
    return true;

  // Now, it is guaranteed that both values are not identical.
  // Use the non empty one if the corresponding mode is active.
  if (merge_mode_ & kReplaceEmpty) {
    if (value.empty()) {
      *this = newer_component;
      return true;
    }
    if (value_newer.empty())
      return true;
  }

  // If the normalized values are the same, optimize the verification status.
  if ((merge_mode_ & kUseBetterOrNewerForSameValue) && (value == value_newer)) {
    if (newer_component.GetVerificationStatus() >= GetVerificationStatus()) {
      *this = newer_component;
    }
    return true;
  }

  // Compare the tokens of both values.
  SortedTokenComparisonResult token_comparison_result =
      CompareSortedTokens(value, value_newer);

  // Use the recursive merge strategy for token equivalent values if the
  // corresponding mode is active.
  if ((merge_mode_ & kRecursivelyMergeTokenEquivalentValues) &&
      (token_comparison_result.status == MATCH)) {
    return MergeTokenEquivalentComponent(newer_component);
  }

  // Replace the subset with the superset if the corresponding mode is active.
  if ((merge_mode_ & kReplaceSubset) && token_comparison_result.OneIsSubset()) {
    if (token_comparison_result.status == SUBSET)
      *this = newer_component;
    return true;
  }

  // Replace the superset with the subset if the corresponding mode is active.
  if ((merge_mode_ & kReplaceSuperset) &&
      token_comparison_result.OneIsSubset()) {
    if (token_comparison_result.status == SUPERSET)
      *this = newer_component;
    return true;
  }

  // If the tokens are already equivalent, use the more recently used one.
  if ((merge_mode_ & (kReplaceSuperset | kReplaceSubset)) &&
      token_comparison_result.status == MATCH) {
    if (newer_was_more_recently_used)
      *this = newer_component;
    return true;
  }

  // Recursively merge a single-token subset if the corresponding mode is
  // active.
  if ((merge_mode_ & kRecursivelyMergeSingleTokenSubset) &&
      token_comparison_result.IsSingleTokenSuperset()) {
    // For the merging of subset token, the tokenization must be done without
    // prior normalization of the values.
    SortedTokenComparisonResult token_comparison_result =
        CompareSortedTokens(GetValue(), newer_component.GetValue());
    return MergeSubsetComponent(newer_component, token_comparison_result);
  }

  // Replace the older value with the newer one if the corresponding mode is
  // active.
  if (merge_mode_ & kUseNewerIfDifferent) {
    *this = newer_component;
    return true;
  }

  // If the one value is a substring of the other, use the substring of the
  // corresponding mode is active.
  if ((merge_mode_ & kUseMostRecentSubstring) &&
      (value.find(value_newer) != base::string16::npos ||
       value_newer.find(value) != base::string16::npos)) {
    if (newer_was_more_recently_used)
      *this = newer_component;
    return true;
  }

  if ((merge_mode_ & kPickShorterIfOneContainsTheOther) &&
      token_comparison_result.ContainEachOther()) {
    if (newer_component.GetValue().size() <= GetValue().size())
      *this = newer_component;
    return true;
  }

  // If the corresponding mode is active, ignore this mode and pair-wise merge
  // the child tokens. Reformat this nodes from its children after the merge.
  if (merge_mode_ & kMergeChildrenAndReformat) {
    DCHECK(newer_component.subcomponents_.size() == subcomponents_.size());
    for (size_t i = 0; i < newer_component.subcomponents_.size(); i++) {
      bool success = subcomponents_[i]->MergeWithComponent(
          *newer_component.subcomponents_[i], newer_was_more_recently_used);
      if (!success)
        return false;
    }
    FormatValueFromSubcomponents();
    return true;
  }
  return false;
}

bool AddressComponent::HasNewerValuePrecendenceInMerging(
    const AddressComponent& newer_component) const {
  return newer_component.GetVerificationStatus() >= GetVerificationStatus();
}

bool AddressComponent::MergeTokenEquivalentComponent(
    const AddressComponent& newer_component) {
  if (!AreSortedTokensEqual(
          TokenizeValue(ValueForComparison()),
          TokenizeValue(newer_component.ValueForComparison()))) {
    return false;
  }

  // Assumption:
  // The values of both components are a permutation of the same tokens.
  // The componentization of the components can be different in terms of
  // how the tokens are divided between the subomponents. The valdiation
  // status of the component and its subcomponent can be different.
  //
  // Merge Strategy:
  // * Adopt the exact value (and validation status) of the node with the higher
  // validation status.
  //
  // * For all subcomponents that have the same value, make a recursive call and
  // use the result.
  //
  // * For the set of all non-matching subcomponents. Either use the ones from
  // this component or the other depending on which substructure is better in
  // terms of the number of validated tokens.

  if (HasNewerValuePrecendenceInMerging(newer_component)) {
    SetValue(newer_component.GetValue(),
             newer_component.GetVerificationStatus());
  }

  // Now, the substructure of the node must be merged. There are three cases:
  //
  // * All nodes of the substructure are pairwise mergeable. In this case it
  // is sufficient to apply a recursive merging strategy.
  //
  // * None of the nodes of the substructure are pairwise mergeable. In this
  // case, either the complete substructure of |this| or |newer_component|
  // must be used. Which one to use can be decided by the higher validation
  // score.
  //
  // * In a mixed scenario, there is at least one pair of mergeable nodes
  // in the substructure and at least on pair of non-mergeable nodes. Here,
  // the mergeable nodes are merged while all other nodes are taken either
  // from |this| or the |newer_component| decided by the higher validation
  // score of the unmerged nodes.
  //
  // The following algorithm combines the three cases by first trying to merge
  // all components pair-wise. For all components that couldn't be merged, the
  // verification score is summed for this and the other component. If the other
  // component has an equal or larger score, finalize the merge by using its
  // components. It is assumed that the other component is the newer of the two
  // components. By favoring the other component in a tie, the most recently
  // used structure wins.

  const std::vector<AddressComponent*> other_subcomponents =
      newer_component.Subcomponents();

  DCHECK(subcomponents_.size() == other_subcomponents.size());

  int this_component_verification_score = 0;
  int newer_component_verification_score = 0;

  std::vector<int> unmerged_indices;
  unmerged_indices.reserve(subcomponents_.size());

  for (size_t i = 0; i < subcomponents_.size(); i++) {
    DCHECK(subcomponents_[i]->GetStorageType() ==
           other_subcomponents.at(i)->GetStorageType());

    // If the components can't be merged directly, store the ungermed index and
    // sum the verification scores to decide which component's substructure to
    // use.
    if (!subcomponents_[i]->MergeTokenEquivalentComponent(
            *other_subcomponents.at(i))) {
      this_component_verification_score +=
          subcomponents_[i]->GetStructureVerificationScore();
      newer_component_verification_score +=
          other_subcomponents.at(i)->GetStructureVerificationScore();
      unmerged_indices.emplace_back(i);
    }
  }

  // If the total verification score of all unmerged components of the other
  // component is equal or larger than the score of this component, use its
  // subcomponents including their substructure for all unmerged components.
  if (newer_component_verification_score >= this_component_verification_score) {
    for (size_t i : unmerged_indices)
      *subcomponents_[i] = *other_subcomponents[i];
  }

  return true;
}

void AddressComponent::ConsumeAdditionalToken(
    const base::string16& token_value) {
  if (IsAtomic()) {
    if (GetValue().empty()) {
      SetValue(token_value, VerificationStatus::kParsed);
    } else {
      SetValue(base::StrCat({GetValue(), base::ASCIIToUTF16(" "), token_value}),
               VerificationStatus::kParsed);
    }
    return;
  }

  // Try the first free subcomponent.
  for (auto* subcomponent : subcomponents_) {
    if (subcomponent->GetValue().empty()) {
      subcomponent->SetValue(token_value, VerificationStatus::kParsed);
      return;
    }
  }

  // Otherwise append the value to the first component.
  subcomponents_[0]->SetValue(
      base::StrCat({GetValue(), base::ASCIIToUTF16(" "), token_value}),
      VerificationStatus::kParsed);
}

bool AddressComponent::MergeSubsetComponent(
    const AddressComponent& subset_component,
    const SortedTokenComparisonResult& token_comparison_result) {
  DCHECK(token_comparison_result.IsSingleTokenSuperset());
  DCHECK(token_comparison_result.additional_tokens.size() == 1);

  base::string16 token_to_consume =
      token_comparison_result.additional_tokens.back().value;

  int this_component_verification_score = 0;
  int newer_component_verification_score = 0;
  bool found_subset_component = false;

  std::vector<int> unmerged_indices;
  unmerged_indices.reserve(subcomponents_.size());

  const std::vector<AddressComponent*>& subset_subcomponents =
      subset_component.Subcomponents();

  unmerged_indices.reserve(subcomponents_.size());

  for (size_t i = 0; i < subcomponents_.size(); i++) {
    DCHECK(subcomponents_[i]->GetStorageType() ==
           subset_subcomponents.at(i)->GetStorageType());

    AddressComponent* subcomponent = subcomponents_[i];
    const AddressComponent* subset_subcomponent = subset_subcomponents.at(i);

    base::string16 additional_token;

    // If the additional token is the value of this token. Just leave it in.
    if (!found_subset_component &&
        subcomponent->GetValue() == token_to_consume &&
        subset_subcomponent->GetValue().empty()) {
      found_subset_component = true;
      continue;
    }

    SortedTokenComparisonResult subtoken_comparison_result =
        CompareSortedTokens(subcomponent->GetSortedTokens(),
                            subset_subcomponent->GetSortedTokens());

    // Recursive case.
    if (!found_subset_component &&
        subtoken_comparison_result.IsSingleTokenSuperset()) {
      found_subset_component = true;
      subcomponent->MergeSubsetComponent(*subset_subcomponent,
                                         subtoken_comparison_result);
      continue;
    }

    // If the tokens are the equivalent, they can directly be merged.
    if (subtoken_comparison_result.status == MATCH) {
      subcomponent->MergeTokenEquivalentComponent(*subset_subcomponent);
      continue;
    }

    // Otherwise calculate the verification score.
    this_component_verification_score +=
        subcomponent->GetStructureVerificationScore();
    newer_component_verification_score +=
        subset_subcomponent->GetStructureVerificationScore();
    unmerged_indices.emplace_back(i);
  }

  // If the total verification score of all unmerged components of the other
  // component is equal or larger than the score of this component, use its
  // subcomponents including their substructure for all unmerged components.
  if (newer_component_verification_score >= this_component_verification_score) {
    for (size_t i : unmerged_indices)
      *subcomponents_[i] = *subset_subcomponents[i];

    if (!found_subset_component)
      this->ConsumeAdditionalToken(token_to_consume);
  }

  // In the current implementation it is always possible to merge.
  // Once more tokens are supported this may change.
  return true;
}

int AddressComponent::GetStructureVerificationScore() const {
  int result = 0;
  switch (GetVerificationStatus()) {
    case VerificationStatus::kNoStatus:
    case VerificationStatus::kParsed:
    case VerificationStatus::kFormatted:
      break;
    case VerificationStatus::kObserved:
      result += 1;
      break;
    case VerificationStatus::kUserVerified:
      // In the current implementation, only the root not can be verified by
      // the user.
      NOTREACHED();
      break;
  }
  for (const AddressComponent* component : subcomponents_)
    result += component->GetStructureVerificationScore();

  return result;
}

base::string16 AddressComponent::NormalizedValue() const {
  return NormalizeValue(GetValue());
}

base::string16 AddressComponent::ValueForComparison() const {
  return NormalizedValue();
}

}  // namespace structured_address

}  // namespace autofill
