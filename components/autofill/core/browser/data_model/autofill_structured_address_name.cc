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
#include "components/autofill/core/browser/data_model/autofill_structured_address_constants.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_regex_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"

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

NameHonorific::NameHonorific(AddressComponent* parent)
    : AddressComponent(NAME_HONORIFIC_PREFIX, parent, MergeMode::kDefault) {}

NameHonorific::~NameHonorific() = default;

NameFirst::NameFirst(AddressComponent* parent)
    : AddressComponent(NAME_FIRST, parent, MergeMode::kDefault) {}

NameFirst::~NameFirst() = default;

NameMiddle::NameMiddle(AddressComponent* parent)
    : AddressComponent(NAME_MIDDLE, parent, MergeMode::kDefault) {}

NameMiddle::~NameMiddle() = default;

void NameMiddle::GetAdditionalSupportedFieldTypes(
    ServerFieldTypeSet* supported_types) const {
  supported_types->insert(NAME_MIDDLE_INITIAL);
}

bool NameMiddle::ConvertAndGetTheValueForAdditionalFieldTypeName(
    const std::string& type_name,
    std::u16string* value) const {
  if (type_name == AutofillType::ServerFieldTypeToString(NAME_MIDDLE_INITIAL)) {
    if (value) {
      // If the stored value has the characteristics of containing only
      // initials, use the value as it is. Otherwise, convert it to a
      // sequence of upper case letters, one for each space- or hyphen-separated
      // token.
      if (HasMiddleNameInitialsCharacteristics(base::UTF16ToUTF8(GetValue()))) {
        *value = GetValue();
      } else {
        *value = ReduceToInitials(GetValue());
      }
    }
    return true;
  }
  return false;
}

bool NameMiddle::ConvertAndSetValueForAdditionalFieldTypeName(
    const std::string& type_name,
    const std::u16string& value,
    const VerificationStatus& status) {
  if (type_name == AutofillType::ServerFieldTypeToString(NAME_MIDDLE_INITIAL)) {
    SetValue(value, status);
    return true;
  }
  return false;
}

NameLastFirst::NameLastFirst(AddressComponent* parent)
    : AddressComponent(NAME_LAST_FIRST, parent, MergeMode::kDefault) {}

NameLastFirst::~NameLastFirst() = default;

NameLastConjunction::NameLastConjunction(AddressComponent* parent)
    : AddressComponent(NAME_LAST_CONJUNCTION, parent, MergeMode::kDefault) {}

NameLastConjunction::~NameLastConjunction() = default;

std::vector<const re2::RE2*> NameLast::GetParseRegularExpressionsByRelevance()
    const {
  auto* pattern_provider = StructuredAddressesRegExProvider::Instance();
  DCHECK(pattern_provider);
  // Check if the name has the characteristics of an Hispanic/Latinx name.
  if (HasHispanicLatinxNameCharaceristics(base::UTF16ToUTF8(GetValue())))
    return {pattern_provider->GetRegEx(RegEx::kParseHispanicLastName)};
  return {pattern_provider->GetRegEx(RegEx::kParseLastNameIntoSecondLastName)};
}

NameLastSecond::NameLastSecond(AddressComponent* parent)
    : AddressComponent(NAME_LAST_SECOND, parent, MergeMode::kDefault) {}

NameLastSecond::~NameLastSecond() = default;

NameLast::NameLast(AddressComponent* parent)
    : AddressComponent(NAME_LAST,
                       parent,
                       MergeMode::kDefault) {}

NameLast::~NameLast() = default;

void NameLast::ParseValueAndAssignSubcomponentsByFallbackMethod() {
  SetValueForTypeIfPossible(NAME_LAST_SECOND, GetValue(),
                            VerificationStatus::kParsed);
}

NameFull::NameFull() : NameFull(nullptr) {}

// TODO(crbug.com/1113617): Honorifics are temporally disabled.
NameFull::NameFull(AddressComponent* parent)
    : AddressComponent(
          NAME_FULL,
          parent,
          MergeMode::kDefault) {}

NameFull::NameFull(const NameFull& other) : NameFull() {
  // The purpose of the copy operator is to copy the values and verification
  // statuses of all nodes in |other| to |this|. This exact functionality is
  // already implemented as a recursive operation in the base class.
  this->CopyFrom(other);
}

NameHonorificPrefix::NameHonorificPrefix(AddressComponent* parent)
    : AddressComponent(NAME_HONORIFIC_PREFIX,
                       parent,
                       MergeMode::kUseBetterOrNewerForSameValue |
                           MergeMode::kReplaceEmpty |
                           MergeMode::kUseBetterOrMostRecentIfDifferent) {}

NameHonorificPrefix::~NameHonorificPrefix() = default;

void NameFull::MigrateLegacyStructure(bool is_verified_profile) {
  // Only if the name was imported from a legacy structure, the component has no
  if (GetVerificationStatus() != VerificationStatus::kNoStatus)
    return;

  // If the value of the component is set, use this value as a basis to migrate
  // the name.
  if (!GetValue().empty()) {
    // If the profile is verified, set the verification status to accordingly
    // and reset all the subcomponents.
    VerificationStatus status = is_verified_profile
                                    ? VerificationStatus::kUserVerified
                                    : VerificationStatus::kObserved;
    SetValue(GetValue(), status);

    // Set the verification status of all subcomponents to |kParsed|.
    for (auto* subcomponent : Subcomponents()) {
      subcomponent->SetValue(subcomponent->GetValue(),
                             subcomponent->GetValue().empty()
                                 ? VerificationStatus::kNoStatus
                                 : VerificationStatus::kParsed);
    }

    // Finally, unset the substructure of the last name and complete it;
    name_last_.RecursivelyUnsetSubcomponents();
    // This tree is not trivially completable, because it has values both at the
    // root node and in the first layer. Make a manual completion call for the
    // structure of the last name.
    name_last_.RecursivelyCompleteTree();

    return;
  }

  // Otherwise, at least one of the subcomponents should be set.
  // Set its verification status to observed.
  for (auto* subcomponent : Subcomponents()) {
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
  if (HasHispanicLatinxNameCharaceristics(base::UTF16ToUTF8(GetValue())))
    return {pattern_provider->GetRegEx(RegEx::kParseHispanicFullName)};

  return {pattern_provider->GetRegEx(RegEx::kParseOnlyLastName),
          pattern_provider->GetRegEx(RegEx::kParseLastCommaFirstMiddleName),
          pattern_provider->GetRegEx(RegEx::kParseFirstMiddleLastName)};
}
std::u16string NameFull::GetBestFormatString() const {
  if (HasCjkNameCharacteristics(base::UTF16ToUTF8(name_first_.GetValue())) &&
      HasCjkNameCharacteristics(base::UTF16ToUTF8(name_last_.GetValue()))) {
    return u"${NAME_LAST}${NAME_FIRST}";
  }
  return u"${NAME_FIRST} ${NAME_MIDDLE} ${NAME_LAST}";
}
NameFull::~NameFull() = default;

NameFullWithPrefix::NameFullWithPrefix() : NameFullWithPrefix(nullptr) {}

NameFullWithPrefix::NameFullWithPrefix(AddressComponent* parent)
    : AddressComponent(NAME_FULL_WITH_HONORIFIC_PREFIX,
                       parent,
                       MergeMode::kMergeChildrenAndReformatIfNeeded) {}

NameFullWithPrefix::NameFullWithPrefix(const NameFullWithPrefix& other)
    : NameFullWithPrefix() {
  // The purpose of the copy operator is to copy the values and verification
  // statuses of all nodes in |other| to |this|. This exact functionality is
  // already implemented as a recursive operation in the base class.
  this->CopyFrom(other);
}

NameFullWithPrefix::~NameFullWithPrefix() = default;

std::vector<const re2::RE2*>
NameFullWithPrefix::GetParseRegularExpressionsByRelevance() const {
  auto* pattern_provider = StructuredAddressesRegExProvider::Instance();
  return {pattern_provider->GetRegEx(RegEx::kParsePrefixedName)};
}

void NameFullWithPrefix::MigrateLegacyStructure(bool is_verified_profile) {
  // If a verification status is set, the structure is already migrated.
  if (GetVerificationStatus() != VerificationStatus::kNoStatus) {
    return;
  }

  // If it is not migrated, continue with migrating the full name.
  name_full_.MigrateLegacyStructure(is_verified_profile);

  // Check if the tree is already in a completed state.
  // If yes, build the root node from the subcomponents.
  // Otherwise, this step is not necessary and will be taken care of in a later
  // stage of the import process.
  if (MaximumNumberOfAssignedAddressComponentsOnNodeToLeafPaths() > 1) {
    FormatValueFromSubcomponents();
  }
}

}  // namespace autofill
