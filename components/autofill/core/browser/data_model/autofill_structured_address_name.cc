// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"

#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_constants.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_format_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_regex_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

std::u16string ReduceToInitials(const std::u16string& value) {
  if (value.empty())
    return std::u16string();

  std::vector<std::u16string> middle_name_tokens =
      base::SplitString(value, base::ASCIIToUTF16(kNameSeparators),
                        base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);

  std::u16string result;
  result.reserve(middle_name_tokens.size());
  for (const auto& token : middle_name_tokens) {
    DCHECK(!token.empty());
    result += token[0];
  }
  return base::i18n::ToUpper(result);
}

NameFirst::NameFirst()
    : AddressComponent(NAME_FIRST, {}, MergeMode::kDefault) {}

NameFirst::~NameFirst() = default;

NameMiddle::NameMiddle()
    : AddressComponent(NAME_MIDDLE, {}, MergeMode::kDefault) {}

NameMiddle::~NameMiddle() = default;

const FieldTypeSet NameMiddle::GetAdditionalSupportedFieldTypes() const {
  constexpr FieldTypeSet additional_supported_field_types{NAME_MIDDLE_INITIAL};
  return additional_supported_field_types;
}

std::u16string NameMiddle::GetValueForOtherSupportedType(
    FieldType field_type) const {
  CHECK(IsSupportedType(field_type));
  return HasMiddleNameInitialsCharacteristics(base::UTF16ToUTF8(GetValue()))
             ? GetValue()
             : ReduceToInitials(GetValue());
}

void NameMiddle::SetValueForOtherSupportedType(
    FieldType field_type,
    const std::u16string& value,
    const VerificationStatus& status) {
  CHECK(IsSupportedType(field_type));
  SetValue(value, status);
}

NameLastFirst::NameLastFirst()
    : AddressComponent(NAME_LAST_FIRST, {}, MergeMode::kDefault) {}

NameLastFirst::~NameLastFirst() = default;

NameLastConjunction::NameLastConjunction()
    : AddressComponent(NAME_LAST_CONJUNCTION, {}, MergeMode::kDefault) {}

NameLastConjunction::~NameLastConjunction() = default;

std::vector<const re2::RE2*> NameLast::GetParseRegularExpressionsByRelevance()
    const {
  auto* pattern_provider = StructuredAddressesRegExProvider::Instance();
  DCHECK(pattern_provider);
  // Check if the name has the characteristics of an Hispanic/Latinx name.
  if (HasHispanicLatinxNameCharacteristics(base::UTF16ToUTF8(GetValue()))) {
    return {pattern_provider->GetRegEx(RegEx::kParseHispanicLastName)};
  }
  return {pattern_provider->GetRegEx(RegEx::kParseLastNameIntoSecondLastName)};
}

NameLastSecond::NameLastSecond()
    : AddressComponent(NAME_LAST_SECOND, {}, MergeMode::kDefault) {}

NameLastSecond::~NameLastSecond() = default;

NameLast::NameLast() : AddressComponent(NAME_LAST, {}, MergeMode::kDefault) {
  RegisterChildNode(&last_first_);
  RegisterChildNode(&last_conjuntion_);
  RegisterChildNode(&last_second_);
}

NameLast::~NameLast() = default;

void NameLast::ParseValueAndAssignSubcomponentsByFallbackMethod() {
  SetValueForType(NAME_LAST_SECOND, GetValue(), VerificationStatus::kParsed);
}

// TODO(crbug.com/40143553): Honorifics are temporally disabled.
NameFull::NameFull() : AddressComponent(NAME_FULL, {}, MergeMode::kDefault) {
  RegisterChildNode(&first_);
  RegisterChildNode(&middle_);
  RegisterChildNode(&last_);
}

NameFull::NameFull(const NameFull& other) : NameFull() {
  // The purpose of the copy operator is to copy the values and verification
  // statuses of all nodes in |other| to |this|. This exact functionality is
  // already implemented as a recursive operation in the base class.
  this->CopyFrom(other);
}

void NameFull::MigrateLegacyStructure() {
  // Only if the name was imported from a legacy structure, the component has no
  if (GetVerificationStatus() != VerificationStatus::kNoStatus)
    return;

  // If the value of the component is set, use this value as a basis to migrate
  // the name.
  if (!GetValue().empty()) {
    SetValue(GetValue(), VerificationStatus::kObserved);

    // Set the verification status of all subcomponents to |kParsed|.
    for (AddressComponent* subcomponent : Subcomponents()) {
      subcomponent->SetValue(subcomponent->GetValue(),
                             subcomponent->GetValue().empty()
                                 ? VerificationStatus::kNoStatus
                                 : VerificationStatus::kParsed);
    }
    AddressComponent* name_last = GetNodeForType(NAME_LAST);
    // Finally, unset the substructure of the last name and complete it;
    name_last->RecursivelyUnsetSubcomponents();
    // This tree is not trivially completable, because it has values both at
    // the root node and in the first layer. Make a manual completion call for
    // the structure of the last name.
    name_last->RecursivelyCompleteTree();

    return;
  }

  // Otherwise, at least one of the subcomponents should be set.
  // Set its verification status to observed.
  for (AddressComponent* subcomponent : Subcomponents()) {
    if (!subcomponent->GetValue().empty())
      subcomponent->SetValue(subcomponent->GetValue(),
                             VerificationStatus::kObserved);
  }

  // If no subcomponent is set, the name is empty. In any case, the name was
  // successfully migrated.
  return;
}

std::vector<const re2::RE2*> NameFull::GetParseRegularExpressionsByRelevance()
    const {
  auto* pattern_provider = StructuredAddressesRegExProvider::Instance();
  DCHECK(pattern_provider);
  // If the name is a CJK name, try to match in the following order:
  //
  // * Match CJK names that include a separator.
  // If a separator is present, dividing the name between first and last name is
  // trivial.
  //
  // * Match Korean 4+ character names with two-character last names.
  // Note, although some of the two-character last names are ambiguous in the
  // sense that they share a common prefix with single character last names. For
  // 4+ character names, it is more likely that the first two characters belong
  // to the last name.
  //
  // * Match known two-character CJK last names.
  // Note, this expressions uses only non-ambiguous two-character last names.
  //
  // * Match only the first character into the last name.
  // This is the catch all expression that uses only the first character for the
  // last name and puts all other characters into the first name.
  //
  if (HasCjkNameCharacteristics(base::UTF16ToUTF8(GetValue()))) {
    return {
        pattern_provider->GetRegEx(RegEx::kParseSeparatedCjkName),
        pattern_provider->GetRegEx(RegEx::kParseKoreanTwoCharacterLastName),
        pattern_provider->GetRegEx(RegEx::kParseCommonCjkTwoCharacterLastName),
        pattern_provider->GetRegEx(RegEx::kParseCjkSingleCharacterLastName)};
  }
  if (HasHispanicLatinxNameCharacteristics(base::UTF16ToUTF8(GetValue()))) {
    return {pattern_provider->GetRegEx(RegEx::kParseHispanicFullName)};
  }

  return {pattern_provider->GetRegEx(RegEx::kParseOnlyLastName),
          pattern_provider->GetRegEx(RegEx::kParseLastCommaFirstMiddleName),
          pattern_provider->GetRegEx(RegEx::kParseFirstMiddleLastName)};
}

std::u16string NameFull::GetFormatString() const {
  StructuredAddressesFormatProvider::ContextInfo info;
  info.name_has_cjk_characteristics =
      HasCjkNameCharacteristics(
          base::UTF16ToUTF8(GetNodeForType(NAME_FIRST)->GetValue())) &&
      HasCjkNameCharacteristics(
          base::UTF16ToUTF8(GetNodeForType(NAME_LAST)->GetValue()));

  auto* pattern_provider = StructuredAddressesFormatProvider::GetInstance();
  CHECK(pattern_provider);
  // TODO(crbug.com/40275657): Add i18n support for name format strings.
  return pattern_provider->GetPattern(GetStorageType(), /*country_code=*/"",
                                      info);
}

NameFull::~NameFull() = default;

}  // namespace autofill
