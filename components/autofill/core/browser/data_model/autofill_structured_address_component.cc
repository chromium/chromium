// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_format_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

bool IsLessSignificantVerificationStatus(VerificationStatus left,
                                         VerificationStatus right) {
  // Both the KUserVerified and kObserved are larger then kServerParsed although
  // the underlying integer suggests differently.
  if (left == VerificationStatus::kServerParsed &&
      (right == VerificationStatus::kObserved ||
       right == VerificationStatus::kUserVerified)) {
    return true;
  }

  if (right == VerificationStatus::kServerParsed &&
      (left == VerificationStatus::kObserved ||
       left == VerificationStatus::kUserVerified)) {
    return false;
  }

  // In all other cases, it is sufficient to compare the underlying integer
  // values.
  return static_cast<std::underlying_type_t<VerificationStatus>>(left) <
         static_cast<std::underlying_type_t<VerificationStatus>>(right);
}

VerificationStatus GetMoreSignificantVerificationStatus(
    VerificationStatus left,
    VerificationStatus right) {
  if (IsLessSignificantVerificationStatus(left, right))
    return right;

  return left;
}

std::ostream& operator<<(std::ostream& os, VerificationStatus status) {
  switch (status) {
    case VerificationStatus::kNoStatus:
      os << "NoStatus";
      break;
    case VerificationStatus::kParsed:
      os << "Parsed";
      break;
    case VerificationStatus::kFormatted:
      os << "Formatted";
      break;
    case VerificationStatus::kObserved:
      os << "Observed";
      break;
    case VerificationStatus::kServerParsed:
      os << "ServerParsed";
      break;
    case VerificationStatus::kUserVerified:
      os << "UserVerified";
      break;
  }
  return os;
}

AddressComponent::AddressComponent(FieldType storage_type,
                                   SubcomponentsList subcomponents,
                                   unsigned int merge_mode)
    : value_verification_status_(VerificationStatus::kNoStatus),
      storage_type_(storage_type),
      merge_mode_(merge_mode) {
  subcomponents_ = std::move(subcomponents);
  for (AddressComponent* child : subcomponents_) {
    child->SetParent(this);
  }
}

AddressComponent::~AddressComponent() = default;

FieldType AddressComponent::GetStorageType() const {
  return storage_type_;
}

FieldType AddressComponent::GetFallbackType(FieldType field_type) const {
  CHECK(IsSupportedType(field_type));
  // TODO(crbug.com/40275657): Add logic for i18n fallback types.
  return field_type;
}

std::string AddressComponent::GetStorageTypeName() const {
  return FieldTypeToString(storage_type_);
}

void AddressComponent::CopyFrom(const AddressComponent& other) {
  CHECK_EQ(GetStorageType(), other.GetStorageType());
  if (this == &other) {
    return;
  }

  if (other.IsValueAssigned()) {
    value_ = other.value_;
    value_verification_status_ = other.value_verification_status_;
    sorted_normalized_tokens_ = other.sorted_normalized_tokens_;
  } else {
    UnsetValue();
  }

  CHECK_EQ(other.subcomponents_.size(), subcomponents_.size())
      << GetStorageTypeName();
  for (size_t i = 0; i < other.subcomponents_.size(); i++)
    subcomponents_[i]->CopyFrom(*other.subcomponents_[i]);

  PostAssignSanitization();
}

bool AddressComponent::SameAs(const AddressComponent& other) const {
  if (this == &other)
    return true;

  if (GetStorageType() != other.GetStorageType())
    return false;

  if (GetValue() != other.GetValue() ||
      value_verification_status_ != other.value_verification_status_) {
    return false;
  }

  if (subcomponents_.size() != other.subcomponents_.size()) {
    return false;
  }
  for (size_t i = 0; i < other.subcomponents_.size(); i++) {
    if (!(subcomponents_[i]->SameAs(*other.subcomponents_[i]))) {
      return false;
    }
  }
  return true;
}

bool AddressComponent::IsAtomic() const {
  return subcomponents_.empty();
}

bool AddressComponent::IsValueValid() const {
  return true;
}

AddressCountryCode AddressComponent::GetCommonCountry(
    const AddressComponent& other) const {
  const AddressCountryCode country_a = GetCountryCode();
  const AddressCountryCode country_b = other.GetCountryCode();
  if (country_a->empty()) {
    return country_b;
  }
  if (country_b->empty()) {
    return country_a;
  }
  return base::EqualsCaseInsensitiveASCII(country_a.value(), country_b.value())
             ? country_a
             : AddressCountryCode("");
}

AddressCountryCode AddressComponent::GetCountryCode() const {
  return AddressCountryCode(
      base::UTF16ToUTF8(GetRootNode().GetValueForType(ADDRESS_HOME_COUNTRY)));
}

bool AddressComponent::IsValueForTypeValid(FieldType field_type,
                                           bool wipe_if_not) {
  AddressComponent* node_for_type = GetNodeForType(field_type);
  if (!node_for_type) {
    return false;
  }
  const bool validity_status = node_for_type->IsValueValid();
  if (!validity_status && wipe_if_not) {
    node_for_type->UnsetValue();
  }
  return validity_status;
}

void AddressComponent::RegisterChildNode(AddressComponent* child,
                                         bool set_as_parent_of_child) {
  subcomponents_.push_back(child);

  if (set_as_parent_of_child) {
    child->SetParent(this);
  }
}

VerificationStatus AddressComponent::GetVerificationStatus() const {
  return value_verification_status_;
}

const std::u16string& AddressComponent::GetValue() const {
  if (value_.has_value())
    return value_.value();
  return base::EmptyString16();
}

std::optional<std::u16string> AddressComponent::GetCanonicalizedValue() const {
  return std::nullopt;
}

bool AddressComponent::IsValueAssigned() const {
  return value_.has_value();
}

void AddressComponent::SetValue(std::u16string value,
                                VerificationStatus status) {
  value_ = std::move(value);
  value_verification_status_ = status;
}

void AddressComponent::UnsetValue() {
  value_.reset();
  value_verification_status_ = VerificationStatus::kNoStatus;
  sorted_normalized_tokens_.reset();
}

bool AddressComponent::IsSupportedType(FieldType field_type) const {
  return field_type == storage_type_ ||
         GetAdditionalSupportedFieldTypes().contains(field_type);
}

bool AddressComponent::IsValueReadOnly() const {
  return false;
}

void AddressComponent::GetSupportedTypes(FieldTypeSet* supported_types) const {
  return AddressComponent::GetTypes(/*storable_only=*/false, supported_types);
}

void AddressComponent::GetStorableTypes(FieldTypeSet* supported_types) const {
  return AddressComponent::GetTypes(/*storable_only=*/true, supported_types);
}

void AddressComponent::GetTypes(bool storable_only,
                                FieldTypeSet* supported_types) const {
  // A proper AddressComponent tree contains every type only once.
  CHECK(supported_types->find(storage_type_) == supported_types->end())
      << "The AddressComponent already contains a node that supports this "
         "type: "
      << storage_type_;
  supported_types->insert(storage_type_);
  if (!storable_only) {
    supported_types->insert_all(GetAdditionalSupportedFieldTypes());
    // Include synthesized types in the list of supported (not storable) types.
    for (const AddressComponent* synthesized_node :
         synthesized_subcomponents_) {
      supported_types->insert(synthesized_node->GetStorageType());
    }
  }

  for (AddressComponent* subcomponent : subcomponents_) {
    subcomponent->GetTypes(storable_only, supported_types);
  }
}

std::optional<FieldType> AddressComponent::GetStorableTypeOf(
    FieldType type) const {
  if (const AddressComponent* node = GetNodeForType(type)) {
    return node->GetStorageType();
  }
  return std::nullopt;
}

const FieldTypeSet AddressComponent::GetAdditionalSupportedFieldTypes() const {
  constexpr FieldTypeSet additional_supported_field_types;
  return additional_supported_field_types;
}

void AddressComponent::SetValueForOtherSupportedType(
    FieldType field_type,
    const std::u16string& value,
    const VerificationStatus& status) {}

std::u16string AddressComponent::GetValueForOtherSupportedType(
    FieldType field_type) const {
  return {};
}

std::u16string AddressComponent::GetFormatString() const {
  std::u16string result = i18n_model_definition::GetFormattingExpression(
      GetStorageType(), GetCountryCode());
  if (!result.empty()) {
    return result;
  }

  // If the component is atomic, the format string is just the value.
  if (IsAtomic()) {
    return base::ASCIIToUTF16(GetPlaceholderToken(GetStorageTypeName()));
  }

  // Otherwise, the canonical format string is the concatenation of all
  // subcomponents by their natural order.
  std::vector<std::string> format_pieces;
  for (const AddressComponent* subcomponent : subcomponents_) {
    std::string format_piece = GetPlaceholderToken(
        FieldTypeToStringView(subcomponent->GetStorageType()));
    format_pieces.emplace_back(std::move(format_piece));
  }
  return base::ASCIIToUTF16(base::JoinString(format_pieces, " "));
}

std::vector<FieldType> AddressComponent::GetSubcomponentTypes() const {
  std::vector<FieldType> subcomponent_types;
  subcomponent_types.reserve(subcomponents_.size());
  for (const AddressComponent* subcomponent : subcomponents_) {
    subcomponent_types.emplace_back(subcomponent->GetStorageType());
  }
  return subcomponent_types;
}

bool AddressComponent::SetValueForType(
    FieldType field_type,
    const std::u16string& value,
    const VerificationStatus& verification_status) {
  AddressComponent* node_for_type = GetNodeForType(field_type);
  if (!node_for_type || node_for_type->IsValueReadOnly()) {
    return false;
  }
  node_for_type->GetStorageType() == field_type
      ? node_for_type->SetValue(value, verification_status)
      : node_for_type->SetValueForOtherSupportedType(field_type, value,
                                                     verification_status);
  return true;
}

bool AddressComponent::SetValueForTypeAndResetSubstructure(
    FieldType field_type,
    const std::u16string& value,
    const VerificationStatus& verification_status) {
  AddressComponent* node_for_type = GetNodeForType(field_type);
  if (!node_for_type) {
    return false;
  }
  node_for_type->GetStorageType() == field_type
      ? node_for_type->SetValue(value, verification_status)
      : node_for_type->SetValueForOtherSupportedType(field_type, value,
                                                     verification_status);
  node_for_type->UnsetSubcomponents();
  return true;
}

void AddressComponent::UnsetAddressComponentAndItsSubcomponents() {
  UnsetValue();
  UnsetSubcomponents();
}

void AddressComponent::UnsetSubcomponents() {
  for (AddressComponent* component : subcomponents_) {
    component->UnsetAddressComponentAndItsSubcomponents();
  }
}

void AddressComponent::FillTreeGaps() {
  if (IsAtomic()) {
    return;
  }

  bool has_empty_child = std::ranges::any_of(
      Subcomponents(),
      [](const AddressComponent* c) { return c->GetValue().empty(); });

  // If the current node is not empty and at least one child is empty, we can
  // try filling the empty children by parsing the value on the current node.
  if (!GetValue().empty() && has_empty_child) {
    TryParseValueAndAssignSubcomponentsRespectingSetValues();
  }

  for (AddressComponent* component : subcomponents_) {
    component->FillTreeGaps();
  }

  if (GetValue().empty() &&
      GetVerificationStatus() == VerificationStatus::kNoStatus) {
    std::u16string formatted_value = GetFormattedValueFromSubcomponents();
    if (!formatted_value.empty() &&
        IsValueCompatibleWithAncestors(formatted_value)) {
      SetValue(formatted_value, VerificationStatus::kFormatted);
    }
  }
}

const AddressComponent* AddressComponent::GetNodeForType(
    FieldType field_type) const {
  // Check if the current node supports `field_type`. If so return the node.
  if (IsSupportedType(field_type)) {
    return this;
  }

  // Check if the requested node is one the synthesized subcomponents.
  for (const AddressComponent* synthesized_node : synthesized_subcomponents_) {
    if (synthesized_node->GetStorageType() == field_type) {
      return synthesized_node;
    }
  }
  // Check if any of the descendants of the node support `field_type`
  for (const AddressComponent* subcomponent : subcomponents_) {
    if (const AddressComponent* matched_subcomponent =
            subcomponent->GetNodeForType(field_type);
        matched_subcomponent) {
      return matched_subcomponent;
    }
  }
  return nullptr;
}

AddressComponent* AddressComponent::GetNodeForType(FieldType field_type) {
  return const_cast<AddressComponent*>(
      const_cast<const AddressComponent*>(this)->GetNodeForType(field_type));
}

std::u16string AddressComponent::GetValueForType(FieldType field_type) const {
  const AddressComponent* node_for_type = GetNodeForType(field_type);
  if (!node_for_type) {
    return {};
  }
  return node_for_type->GetStorageType() == field_type
             ? node_for_type->GetValue()
             : node_for_type->GetValueForOtherSupportedType(field_type);
}

std::u16string AddressComponent::GetValueForComparisonForType(
    FieldType field_type,
    const AddressComponent& other) const {
  const AddressComponent* node_for_type = GetNodeForType(field_type);
  if (!node_for_type) {
    return {};
  }
  return node_for_type->GetValueForComparison(
      node_for_type->GetStorageType() == field_type
          ? node_for_type->GetValue()
          : node_for_type->GetValueForOtherSupportedType(field_type),
      other);
}

VerificationStatus AddressComponent::GetVerificationStatusForType(
    FieldType field_type) const {
  const AddressComponent* node_for_type = GetNodeForType(field_type);
  return node_for_type ? node_for_type->GetVerificationStatus()
                       : VerificationStatus::kNoStatus;
}

FieldType AddressComponent::GetFallbackTypeForType(FieldType field_type) const {
  const AddressComponent* node_for_type = GetNodeForType(field_type);
  return node_for_type ? node_for_type->GetFallbackType(field_type)
                       : field_type;
}

bool AddressComponent::UnsetValueForTypeIfSupported(FieldType field_type) {
  AddressComponent* node_for_type = GetNodeForType(field_type);
  if (!node_for_type || field_type != node_for_type->GetStorageType()) {
    return false;
  }
  node_for_type->UnsetAddressComponentAndItsSubcomponents();
  return true;
}

std::vector<const re2::RE2*>
AddressComponent::GetParseRegularExpressionsByRelevance() const {
  return {};
}

void AddressComponent::ParseValueAndAssignSubcomponents() {
  // Set the values of all subcomponents to the empty string and set the
  // verification status to kParsed.
  for (AddressComponent* subcomponent : subcomponents_) {
    subcomponent->SetValue(std::u16string(), VerificationStatus::kParsed);
  }

  bool parsing_successful =
      GroupTypeOfFieldType(GetStorageType()) == FieldTypeGroup::kAddress
          ? ParseValueAndAssignSubcomponentsByI18nParsingRules()
          : ParseValueAndAssignSubcomponentsByRegularExpressions();

  if (parsing_successful) {
    return;
  }

  // As a final fallback, parse using the fallback method.
  ParseValueAndAssignSubcomponentsByFallbackMethod();
}

bool AddressComponent::ParseValueAndAssignSubcomponentsByI18nParsingRules() {
  i18n_model_definition::ValueParsingResults results =
      i18n_model_definition::ParseValueByI18nRegularExpression(
          base::UTF16ToUTF8(GetValue()), GetStorageType(), GetCountryCode());

  if (results) {
    AssignParsedValuesToSubcomponents(std::move(results));
    return true;
  }
  return false;
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

void AddressComponent::
    TryParseValueAndAssignSubcomponentsRespectingSetValues() {
  if (GroupTypeOfFieldType(GetStorageType()) == FieldTypeGroup::kAddress) {
    i18n_model_definition::ValueParsingResults results =
        i18n_model_definition::ParseValueByI18nRegularExpression(
            base::UTF16ToUTF8(GetValue()), GetStorageType(), GetCountryCode());

    AssignParsedValuesToSubcomponentsRespectingSetValues(std::move(results));
    return;
  }

  for (const auto* parse_expression : GetParseRegularExpressionsByRelevance()) {
    if (!parse_expression) {
      continue;
    }
    if (ParseValueAndAssignSubcomponentsRespectingSetValues(GetValue(),
                                                            parse_expression)) {
      return;
    }
  }
}

bool AddressComponent::ParseValueAndAssignSubcomponentsRespectingSetValues(
    const std::u16string& value,
    const RE2* parse_expression) {
  i18n_model_definition::ValueParsingResults results =
      ParseValueByRegularExpression(base::UTF16ToUTF8(GetValue()),
                                    parse_expression);

  return AssignParsedValuesToSubcomponentsRespectingSetValues(
      std::move(results));
}

bool AddressComponent::IsValueCompatibleWithDescendants(
    const std::u16string& value) const {
  if (!GetValue().empty()) {
    // If the node is token compatible, then the remaining subtree also is.
    return AreStringTokenCompatible(GetValue(), value);
  }

  return std::ranges::all_of(
      Subcomponents(), [value](const AddressComponent* c) {
        return c->IsValueCompatibleWithDescendants(value);
      });
}

bool AddressComponent::ParseValueAndAssignSubcomponentsByRegularExpression(
    const std::u16string& value,
    const RE2* parse_expression) {
  i18n_model_definition::ValueParsingResults results =
      ParseValueByRegularExpression(base::UTF16ToUTF8(value), parse_expression);
  if (results) {
    AssignParsedValuesToSubcomponents(std::move(results));
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
  std::vector<std::u16string> space_separated_tokens = base::SplitString(
      GetValue(), u" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  auto token_iterator = space_separated_tokens.begin();
  auto subcomponent_types = GetSubcomponentTypes();

  // Assign one space-separated token each to all but the last subcomponent.
  for (size_t i = 0; (i + 1) < subcomponent_types.size(); i++) {
    // If there are no tokens left, parsing is done.
    if (token_iterator == space_separated_tokens.end())
      return;
    // Set the current token to the type and advance the token iterator. By
    // design, this should never fail.
    CHECK(SetValueForType(subcomponent_types[i], *token_iterator,
                          VerificationStatus::kParsed));
    token_iterator++;
  }

  // Collect all remaining tokens in the last subcomponent.
  std::u16string remaining_tokens = base::JoinString(
      std::vector<std::u16string>(token_iterator, space_separated_tokens.end()),
      u" ");
  // By design, it should be possible to assign the value unless the regular
  // expression is wrong.
  CHECK(SetValueForType(subcomponent_types.back(), remaining_tokens,
                        VerificationStatus::kParsed));
}

void AddressComponent::AssignParsedValuesToSubcomponents(
    i18n_model_definition::ValueParsingResults values) {
  if (!values) {
    return;
  }
  // Parsing was successful and results from the result map can be written
  // to the structure.
  for (const auto& [field_type, field_value] : *values) {
    // Do not reassign the value of this node.
    if (field_type == GetStorageTypeName()) {
      continue;
    }
    // Setting the value should always work unless the regular expression is
    // invalid.
    CHECK(SetValueForType(TypeNameToFieldType(field_type),
                          base::UTF8ToUTF16(field_value),
                          VerificationStatus::kParsed));
  }
}

bool AddressComponent::AssignParsedValuesToSubcomponentsRespectingSetValues(
    i18n_model_definition::ValueParsingResults values) {
  if (!values) {
    return false;
  }
  // Make sure that parsing matches non-empty values.
  for (AddressComponent* subcomponent : Subcomponents()) {
    if (!subcomponent->GetValue().empty()) {
      auto it = values->find(subcomponent->GetStorageTypeName());
      if (it == values->end() ||
          base::UTF8ToUTF16(it->second) != subcomponent->GetValue()) {
        return false;
      }
    }
  }

  // Parsing was successful and results from the result map can be written
  // to the structure.
  for (AddressComponent* subcomponent : Subcomponents()) {
    auto it = values->find(subcomponent->GetStorageTypeName());
    if (subcomponent->GetValue().empty() && it != values->end()) {
      const std::u16string parsed_value = base::UTF8ToUTF16(it->second);
      if (!parsed_value.empty() &&
          subcomponent->IsValueCompatibleWithDescendants(parsed_value)) {
        subcomponent->SetValue(parsed_value, VerificationStatus::kParsed);
      }
    }
  }
  return true;
}

bool AddressComponent::AllDescendantsAreEmpty() const {
  return std::ranges::all_of(Subcomponents(), [](const AddressComponent* c) {
    return c->GetValue().empty() && c->AllDescendantsAreEmpty();
  });
}

void AddressComponent::WipeRawPtrsForDestruction() {
  subcomponents_.clear();
  synthesized_subcomponents_.clear();
  parent_ = nullptr;
}

bool AddressComponent::IsValueCompatibleWithAncestors(
    const std::u16string& value) const {
  if (!GetValue().empty()) {
    return AreStringTokenCompatible(value, GetValue());
  }
  return (!parent_ || parent_->IsValueCompatibleWithAncestors(value));
}

bool AddressComponent::IsStructureValid() const {
  if (IsAtomic()) {
    return true;
  }
  // Test that each structured token is part of the subcomponent.
  // This is not perfect, because different components can match with an
  // overlapping portion of the unstructured string, but it guarantees that all
  // information in the components is contained in the unstructured
  // representation.
  return std::ranges::all_of(
      Subcomponents(), [this](const AddressComponent* c) {
        return AreStringTokenCompatible(c->GetValue(), GetValue());
      });
}

bool AddressComponent::WipeInvalidStructure() {
  if (!IsStructureValid()) {
    RecursivelyUnsetSubcomponents();
    return true;
  }
  return false;
}

std::u16string AddressComponent::GetFormattedValueFromSubcomponents() {
  // * Replace all the placeholders of the form ${TYPE_NAME} with the
  // corresponding value.
  // * Strip away double spaces as they may occur after replacing a placeholder
  // with an empty value.
  // * Strip away double new lines as they may occur after replacing a
  // placeholder with an empty value.

  std::vector<std::u16string> lines =
      base::SplitString(GetFormatString(), u"\n", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  std::vector<std::u16string> formatted_lines;
  formatted_lines.reserve(lines.size());
  for (const std::u16string& line : lines) {
    std::u16string formatted_line =
        base::CollapseWhitespace(ReplacePlaceholderTypesWithValues(line),
                                 /*trim_sequences_with_line_breaks=*/false);
    if (!formatted_line.empty()) {
      formatted_lines.emplace_back(std::move(formatted_line));
    }
  }

  return base::JoinString(formatted_lines, u"\n");
}

void AddressComponent::FormatValueFromSubcomponents() {
  SetValue(GetFormattedValueFromSubcomponents(),
           VerificationStatus::kFormatted);
}

std::u16string AddressComponent::ReplacePlaceholderTypesWithValues(
    std::u16string_view format) const {
  // Replaces placeholders using the following rules.
  // Assumptions: Placeholder values are not nested.
  //
  // * Search for a substring of the form "{\$[^}]*}".
  // The substring can contain semicolon-separated tokens. The first token is
  // always the type name. If present, the second token is a prefix that is only
  // inserted if the corresponding value is not empty. Accordingly, the third
  // token is a suffix.
  //
  // * Check if this substring is a supported type of this component.
  //
  // * If yes, replace the substring with the corresponding value.

  // Store the token pieces that are joined in the end.
  std::vector<std::u16string> values_to_join;

  bool started_control_sequence = false;
  // Track until which index the format string was fully processed.
  size_t first_unprocessed_index = 0;
  std::u16string pending_separator = u"";
  for (size_t i = 0; i < format.size(); ++i) {
    // Check if a control sequence is started by '${'
    if (i + 1 < format.size() && format.substr(i, 2) == u"${") {
      CHECK(!started_control_sequence) << format;
      started_control_sequence = true;
      // Append the preceding string as a separator of the current control
      // sequence and the previous one.
      pending_separator =
          format.substr(first_unprocessed_index, i - first_unprocessed_index);
      // Mark character '{' as the last processed character and skip it.
      first_unprocessed_index = i + 2;
      ++i;
    } else if (format[i] == u'}') {
      CHECK(started_control_sequence);
      // The control sequence came to an end.
      started_control_sequence = false;
      std::u16string placeholder(
          format.substr(first_unprocessed_index, i - first_unprocessed_index));

      std::vector<std::u16string_view> placeholder_tokens =
          base::SplitStringPiece(placeholder, u";", base::KEEP_WHITESPACE,
                                 base::SPLIT_WANT_ALL);
      CHECK(!placeholder_tokens.empty() && placeholder_tokens.size() <= 3u);

      std::u16string_view type_name = placeholder_tokens[0];
      std::u16string_view prefix = placeholder_tokens.size() > 1
                                       ? placeholder_tokens[1]
                                       : std::u16string_view();
      std::u16string_view suffix = placeholder_tokens.size() > 2
                                       ? placeholder_tokens[2]
                                       : std::u16string_view();

      const AddressComponent* node_for_type =
          GetNodeForType(TypeNameToFieldType(base::UTF16ToASCII(type_name)));
      CHECK(node_for_type) << type_name;

      const std::u16string& value = node_for_type->GetValue();
      if (!value.empty()) {
        if (!values_to_join.empty()) {
          values_to_join.emplace_back(pending_separator);
        }
        values_to_join.emplace_back(base::StrCat({prefix, value, suffix}));
      } else {
        pending_separator.clear();
      }
      first_unprocessed_index = i + 1;
    }
  }
  CHECK(!started_control_sequence) << format;
  // Append the rest of the string.
  values_to_join.emplace_back(
      format.substr(first_unprocessed_index, std::u16string::npos));

  // Build the final result.
  return base::JoinString(values_to_join, u"");
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
      // The tree is potentially complete, we can try filling gaps.
      GetRootNode().FillTreeGaps();
      return false;
  }
}

void AddressComponent::GenerateTreeSynthesizedNodes() {
  for (AddressComponent* subcomponent : subcomponents_) {
    subcomponent->GenerateTreeSynthesizedNodes();
  }

  for (AddressComponent* synthesized_component : synthesized_subcomponents_) {
    synthesized_component->FormatValueFromSubcomponents();
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
  for (AddressComponent* subcomponent : subcomponents_) {
    subcomponent->RecursivelyCompleteTree();
  }

  // Finally format the value from the subcomponents if it is not already
  // assigned.
  if (GetValue().empty())
    FormatValueFromSubcomponents();
}

int AddressComponent::
    MaximumNumberOfAssignedAddressComponentsOnNodeToLeafPaths() const {
  int result = 0;

  for (AddressComponent* subcomponent : subcomponents_) {
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

  for (AddressComponent* component : subcomponents_) {
    component->RecursivelyUnsetParsedAndFormattedValues();
  }
}

void AddressComponent::RecursivelyUnsetSubcomponents() {
  for (AddressComponent* subcomponent : subcomponents_) {
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
      HasNewerValuePrecedenceInMerging(newer_component)) {
    value_verification_status_ = newer_component.GetVerificationStatus();
  }
  CHECK_EQ(newer_component.subcomponents_.size(), subcomponents_.size())
      << GetStorageTypeName();
  for (size_t i = 0; i < newer_component.subcomponents_.size(); i++) {
    subcomponents_[i]->MergeVerificationStatuses(
        *newer_component.subcomponents_.at(i));
  }
}

void AddressComponent::RegisterSynthesizedSubcomponent(
    AddressComponent* synthesized_component) {
  synthesized_component->SetParent(this);
  synthesized_subcomponents_.push_back(synthesized_component);
}

const std::vector<AddressToken> AddressComponent::GetSortedTokens() const {
  return TokenizeValue(GetValue());
}

bool AddressComponent::IsMergeableWithComponent(
    const AddressComponent& newer_component) const {
  const std::u16string older_comparison_value =
      GetValueForComparison(newer_component);
  const std::u16string newer_comparison_value =
      newer_component.GetValueForComparison(*this);

  // If both components are the same, there is nothing to do.
  if (SameAs(newer_component))
    return true;

  if (merge_mode_ & kUseNewerIfDifferent ||
      merge_mode_ & kUseBetterOrMostRecentIfDifferent) {
    return true;
  }

  if ((merge_mode_ & kReplaceEmpty) &&
      (older_comparison_value.empty() || newer_comparison_value.empty())) {
    return true;
  }

  SortedTokenComparisonResult token_comparison_result =
      CompareSortedTokens(older_comparison_value, newer_comparison_value);

  bool comparison_values_are_substrings_of_each_other =
      (older_comparison_value.find(newer_comparison_value) !=
           std::u16string::npos ||
       newer_comparison_value.find(older_comparison_value) !=
           std::u16string::npos);

  if (merge_mode_ & kMergeBasedOnCanonicalizedValues) {
    std::optional<std::u16string> older_canonical_value =
        GetCanonicalizedValue();
    std::optional<std::u16string> newer_canonical_value =
        newer_component.GetCanonicalizedValue();

    bool older_has_canonical_value = older_canonical_value.has_value();
    bool newer_has_canonical_value = newer_canonical_value.has_value();

    // If both have a canonical value and the value is the same, they are
    // obviously mergeable.
    if (older_has_canonical_value && newer_has_canonical_value &&
        older_canonical_value == newer_canonical_value) {
      return true;
    }

    // If one value does not have canonicalized representation but the actual
    // values are substrings of each other, or the tokens contain each other we
    // will merge by just using the one with the canonicalized name.
    if (older_has_canonical_value != newer_has_canonical_value &&
        (comparison_values_are_substrings_of_each_other ||
         token_comparison_result.ContainEachOther())) {
      return true;
    }
  }

  if (merge_mode_ & kUseBetterOrNewerForSameValue) {
    if (base::ToUpperASCII(older_comparison_value) ==
        base::ToUpperASCII(newer_comparison_value)) {
      return true;
    }
  }

  if ((merge_mode_ & (kRecursivelyMergeTokenEquivalentValues |
                      kRecursivelyMergeSingleTokenSubset)) &&
      token_comparison_result.status == SortedTokenComparisonStatus::kMatch) {
    return true;
  }

  if ((merge_mode_ & (kReplaceSubset | kReplaceSuperset)) &&
      (token_comparison_result.OneIsSubset() ||
       token_comparison_result.status == SortedTokenComparisonStatus::kMatch)) {
    return true;
  }

  if ((merge_mode_ & kRecursivelyMergeSingleTokenSubset) &&
      token_comparison_result.IsSingleTokenSuperset()) {
    // This strategy is only applicable if also the unnormalized values have a
    // single-token-superset relation.
    SortedTokenComparisonResult unnormalized_token_comparison_result =
        CompareSortedTokens(GetValue(), newer_component.GetValue());
    if (unnormalized_token_comparison_result.IsSingleTokenSuperset()) {
      return true;
    }
  }

  // If the one value is a substring of the other, use the substring of the
  // corresponding mode is active.
  if ((merge_mode_ & kUseMostRecentSubstring) &&
      comparison_values_are_substrings_of_each_other) {
    return true;
  }

  if ((merge_mode_ & kPickShorterIfOneContainsTheOther) &&
      token_comparison_result.ContainEachOther()) {
    return true;
  }

  // Checks if all child nodes are mergeable.
  if (merge_mode_ & kMergeChildrenAndReformatIfNeeded) {
    bool is_mergeable = true;

    if (subcomponents_.size() != newer_component.subcomponents_.size()) {
      return false;
    }
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

  const std::u16string value = GetValueForComparison(newer_component);
  const std::u16string value_newer =
      newer_component.GetValueForComparison(*this);

  bool newer_component_has_better_or_equal_status =
      !IsLessSignificantVerificationStatus(
          newer_component.GetVerificationStatus(), GetVerificationStatus());
  bool components_have_the_same_status =
      GetVerificationStatus() == newer_component.GetVerificationStatus();
  bool newer_component_has_better_status =
      newer_component_has_better_or_equal_status &&
      !components_have_the_same_status;

  if (SameAs(newer_component))
    return true;

  // Now, it is guaranteed that both values are not identical.
  // Use the non empty one if the corresponding mode is active.
  if (merge_mode_ & kReplaceEmpty) {
    if (value.empty()) {
      // Only replace the value if the verification status is not kUserVerified.
      if (GetVerificationStatus() != VerificationStatus::kUserVerified) {
        CopyFrom(newer_component);
      }
      return true;
    }
    if (value_newer.empty()) {
      return true;
    }
  }

  // If the normalized values are the same, optimize the verification status.
  if ((merge_mode_ & kUseBetterOrNewerForSameValue) && (value == value_newer)) {
    if (HasNewerValuePrecedenceInMerging(newer_component)) {
      CopyFrom(newer_component);
    }
    return true;
  }

  // Compare the tokens of both values.
  SortedTokenComparisonResult token_comparison_result =
      CompareSortedTokens(value, value_newer);

  // Use the recursive merge strategy for token equivalent values if the
  // corresponding mode is active.
  if ((merge_mode_ & kRecursivelyMergeTokenEquivalentValues) &&
      (token_comparison_result.status == SortedTokenComparisonStatus::kMatch)) {
    return MergeTokenEquivalentComponent(newer_component);
  }

  // Replace the subset with the superset if the corresponding mode is active.
  if ((merge_mode_ & kReplaceSubset) && token_comparison_result.OneIsSubset()) {
    if (token_comparison_result.status ==
            SortedTokenComparisonStatus::kSubset &&
        newer_component_has_better_or_equal_status) {
      CopyFrom(newer_component);
    }
    return true;
  }

  // Replace the superset with the subset if the corresponding mode is active.
  if ((merge_mode_ & kReplaceSuperset) &&
      token_comparison_result.OneIsSubset()) {
    if (token_comparison_result.status ==
        SortedTokenComparisonStatus::kSuperset) {
      CopyFrom(newer_component);
    }
    return true;
  }

  // If the tokens are already equivalent, use the more recently used one.
  if ((merge_mode_ & (kReplaceSuperset | kReplaceSubset)) &&
      token_comparison_result.status == SortedTokenComparisonStatus::kMatch) {
    if (newer_was_more_recently_used &&
        newer_component_has_better_or_equal_status) {
      CopyFrom(newer_component);
    }
    return true;
  }

  // Recursively merge a single-token subset if the corresponding mode is
  // active.
  if ((merge_mode_ & kRecursivelyMergeSingleTokenSubset) &&
      token_comparison_result.IsSingleTokenSuperset()) {
    // For the merging of subset token, the tokenization must be done without
    // prior normalization of the values.
    SortedTokenComparisonResult unnormalized_token_comparison_result =
        CompareSortedTokens(GetValue(), newer_component.GetValue());
    // The merging strategy can only be applied when the comparison of the
    // unnormalized tokens still yields a single token superset.
    if (unnormalized_token_comparison_result.IsSingleTokenSuperset()) {
      return MergeSubsetComponent(newer_component,
                                  unnormalized_token_comparison_result);
    }
  }

  // Replace the older value with the newer one if the corresponding mode is
  // active.
  if (merge_mode_ & kUseNewerIfDifferent) {
    CopyFrom(newer_component);
    return true;
  }

  // If the one value is a substring of the other, use the substring of the
  // corresponding mode is active.
  if ((merge_mode_ & kUseMostRecentSubstring) &&
      (value.find(value_newer) != std::u16string::npos ||
       value_newer.find(value) != std::u16string::npos)) {
    if (newer_was_more_recently_used &&
        newer_component_has_better_or_equal_status) {
      CopyFrom(newer_component);
    }
    return true;
  }

  bool comparison_values_are_substrings_of_each_other =
      (value.find(value_newer) != std::u16string::npos ||
       value_newer.find(value) != std::u16string::npos);

  if (merge_mode_ & kMergeBasedOnCanonicalizedValues) {
    std::optional<std::u16string> canonical_value = GetCanonicalizedValue();
    std::optional<std::u16string> other_canonical_value =
        newer_component.GetCanonicalizedValue();

    bool this_has_canonical_value = canonical_value.has_value();
    bool newer_has_canonical_value = other_canonical_value.has_value();

    // When both have the same canonical value they are obviously mergeable.
    if (canonical_value.has_value() && other_canonical_value.has_value() &&
        *canonical_value == *other_canonical_value) {
      // If the newer component has a better verification status use the newer
      // one.
      if (newer_component_has_better_status) {
        CopyFrom(newer_component);
      }
      // If they have the same status use the shorter one.
      if (components_have_the_same_status &&
          newer_component.GetValue().size() <= GetValue().size()) {
        CopyFrom(newer_component);
      }
      return true;
    }

    // If only one component has a canonicalized name but the actual values
    // contain each other either tokens-wise or as substrings, use the component
    // that has a canonicalized name unless the other component has a better
    // verification status.
    if (this_has_canonical_value != newer_has_canonical_value &&
        (comparison_values_are_substrings_of_each_other ||
         token_comparison_result.ContainEachOther())) {
      // Copy the new component if it has a canonicalized name and a status
      // that is not worse or if it has a better status even if it is not
      // canonicalized.
      if ((!this_has_canonical_value &&
           newer_component_has_better_or_equal_status) ||
          (this_has_canonical_value && newer_component_has_better_status)) {
        CopyFrom(newer_component);
      }
      return true;
    }
  }

  if ((merge_mode_ & kPickShorterIfOneContainsTheOther) &&
      token_comparison_result.ContainEachOther()) {
    if (newer_component.GetValue().size() <= GetValue().size() &&
        !IsLessSignificantVerificationStatus(
            newer_component.GetVerificationStatus(), GetVerificationStatus())) {
      CopyFrom(newer_component);
    }
    return true;
  }

  if (merge_mode_ & kUseBetterOrMostRecentIfDifferent) {
    if (HasNewerValuePrecedenceInMerging(newer_component)) {
      SetValue(newer_component.GetValue(),
               newer_component.GetVerificationStatus());
    }
    return true;
  }

  // If the corresponding mode is active, ignore this mode and pair-wise merge
  // the child tokens. Reformat this nodes from its children after the merge.
  if (merge_mode_ & kMergeChildrenAndReformatIfNeeded) {
    CHECK_EQ(newer_component.subcomponents_.size(), subcomponents_.size());
    for (size_t i = 0; i < newer_component.subcomponents_.size(); i++) {
      if (!subcomponents_[i]->MergeWithComponent(
              *newer_component.subcomponents_[i],
              newer_was_more_recently_used)) {
        return false;
      }
    }
    // If the two values are already token equivalent, use the value of the
    // component with the better verification status, or if both are the same,
    // use the newer one.
    if (token_comparison_result.TokensMatch()) {
      if (HasNewerValuePrecedenceInMerging(newer_component)) {
        SetValue(newer_component.GetValue(),
                 newer_component.GetVerificationStatus());
      }
    } else {
      // Otherwise do a reformat from the subcomponents.
      std::u16string formatted_value = GetFormattedValueFromSubcomponents();
      // If the current value is maintained, keep the more significant
      // verification status.
      if (formatted_value == GetValue()) {
        SetValue(formatted_value,
                 GetMoreSignificantVerificationStatus(
                     VerificationStatus::kFormatted, GetVerificationStatus()));
      } else if (formatted_value == newer_component.GetValue()) {
        // Otherwise test if the value is the same as the one of
        // |newer_component|. If yes, maintain the better verification status.
        SetValue(formatted_value, GetMoreSignificantVerificationStatus(
                                      VerificationStatus::kFormatted,
                                      newer_component.GetVerificationStatus()));
      } else {
        // In all other cases, set the formatted_value.
        SetValue(formatted_value, VerificationStatus::kFormatted);
      }
    }
    return true;
  }

  return false;
}

bool AddressComponent::HasNewerValuePrecedenceInMerging(
    const AddressComponent& newer_component) const {
  // In case of equality of verification statuses, the newer component gets
  // precedence over the old one.
  return !IsLessSignificantVerificationStatus(
      newer_component.GetVerificationStatus(), GetVerificationStatus());
}

bool AddressComponent::MergeTokenEquivalentComponent(
    const AddressComponent& newer_component) {
  if (!AreSortedTokensEqual(
          TokenizeValue(GetValueForComparison(newer_component)),
          TokenizeValue(newer_component.GetValueForComparison(*this)))) {
    return false;
  }

  // Assumption:
  // The values of both components are a permutation of the same tokens.
  // The componentization of the components can be different in terms of
  // how the tokens are divided between the subcomponents. The validation
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

  const SubcomponentsList& other_subcomponents =
      newer_component.Subcomponents();
  CHECK_EQ(subcomponents_.size(), other_subcomponents.size())
      << GetStorageTypeName();
  if (HasNewerValuePrecedenceInMerging(newer_component)) {
    SetValue(newer_component.GetValue(),
             newer_component.GetVerificationStatus());
  }
  if (IsAtomic()) {
    return true;
  }
  // If the other component has subtree, just keep this one.
  if (newer_component.AllDescendantsAreEmpty()) {
    return true;
  } else if (AllDescendantsAreEmpty()) {
    // Otherwise, replace this subtree with the other one if this subtree is
    // empty.
    for (size_t i = 0; i < subcomponents_.size(); ++i) {
      subcomponents_[i]->CopyFrom(*other_subcomponents[i]);
    }
    return true;
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

  int this_component_verification_score = 0;
  int newer_component_verification_score = 0;

  std::vector<int> unmerged_indices;
  unmerged_indices.reserve(subcomponents_.size());

  for (size_t i = 0; i < subcomponents_.size(); i++) {
    CHECK_EQ(subcomponents_[i]->GetStorageType(),
             other_subcomponents.at(i)->GetStorageType());
    // If the components can't be merged directly, store the unmerged index and
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
    for (size_t i : unmerged_indices) {
      subcomponents_[i]->CopyFrom(*other_subcomponents[i]);
    }
  }
  return true;
}

void AddressComponent::ConsumeAdditionalToken(
    const std::u16string& token_value) {
  if (IsAtomic()) {
    if (GetValue().empty()) {
      SetValue(token_value, VerificationStatus::kParsed);
    } else {
      SetValue(base::StrCat({GetValue(), u" ", token_value}),
               VerificationStatus::kParsed);
    }
    return;
  }

  // Try the first free subcomponent.
  for (AddressComponent* subcomponent : subcomponents_) {
    if (subcomponent->GetValue().empty()) {
      subcomponent->SetValue(token_value, VerificationStatus::kParsed);
      return;
    }
  }

  // Otherwise append the value to the first component.
  subcomponents_[0]->SetValue(base::StrCat({GetValue(), u" ", token_value}),
                              VerificationStatus::kParsed);
}

bool AddressComponent::MergeSubsetComponent(
    const AddressComponent& subset_component,
    const SortedTokenComparisonResult& token_comparison_result) {
  CHECK(token_comparison_result.IsSingleTokenSuperset());
  CHECK_EQ(token_comparison_result.additional_tokens.size(), 1u);
  std::u16string token_to_consume =
      token_comparison_result.additional_tokens.back().value;

  int this_component_verification_score = 0;
  int newer_component_verification_score = 0;
  bool found_subset_component = false;

  std::vector<int> unmerged_indices;
  unmerged_indices.reserve(subcomponents_.size());

  const SubcomponentsList& subset_subcomponents =
      subset_component.Subcomponents();

  unmerged_indices.reserve(subcomponents_.size());

  for (size_t i = 0; i < subcomponents_.size(); i++) {
    CHECK_EQ(subcomponents_[i]->GetStorageType(),
             subset_subcomponents.at(i)->GetStorageType());
    AddressComponent* subcomponent = subcomponents_[i];
    const AddressComponent* subset_subcomponent = subset_subcomponents.at(i);

    std::u16string additional_token;

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
    if (subtoken_comparison_result.status ==
        SortedTokenComparisonStatus::kMatch) {
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
      subcomponents_[i]->CopyFrom(*subset_subcomponents[i]);

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
    case VerificationStatus::kServerParsed:
      break;
    case VerificationStatus::kObserved:
      result += 1;
      break;
    case VerificationStatus::kUserVerified:
      // This score is chosen so high that a component with a verified value
      // will always win over non-verified ones.
      result += 1000;
      break;
  }
  for (const AddressComponent* component : subcomponents_) {
    result += component->GetStructureVerificationScore();
  }

  return result;
}

std::u16string AddressComponent::GetNormalizedValue() const {
  return NormalizeValue(GetValue());
}

std::u16string AddressComponent::GetValueForComparison(
    const AddressComponent& other) const {
  return GetValueForComparison(GetValue(), other);
}

std::u16string AddressComponent::GetValueForComparison(
    const std::u16string& value,
    const AddressComponent& other) const {
  return NormalizeValue(value);
}

}  // namespace autofill
