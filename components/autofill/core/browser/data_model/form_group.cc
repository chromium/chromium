// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/form_group.h"

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_utils.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/logging/log_buffer.h"

namespace autofill {

void FormGroup::GetMatchingTypes(std::u16string_view text,
                                 std::string_view app_locale,
                                 FieldTypeSet* matching_types) const {
  if (text.empty()) {
    matching_types->insert(EMPTY_TYPE);
    return;
  }

  if (normalization::HasOnlySkippableCharacters(text)) {
    return;
  }

  std::u16string canonicalized_text =
      normalization::NormalizeForComparison(text);
  for (FieldType type : GetSupportedTypes()) {
    if (AutofillProfileComparator::Compare(
            canonicalized_text, GetInfo(type, app_locale),
            normalization::WhitespaceSpec::kDiscard, type)) {
      matching_types->insert(type);
    }
  }
}

void FormGroup::GetNonEmptyTypes(std::string_view app_locale,
                                 FieldTypeSet* non_empty_types) const {
  for (FieldType type : GetSupportedTypes()) {
    if (!GetInfo(type, app_locale).empty()) {
      non_empty_types->insert(type);
    }
  }
}

bool FormGroup::HasRawInfo(FieldType type) const {
  return !GetRawInfo(type).empty();
}

std::u16string FormGroup::GetInfo(FieldType type,
                                  std::string_view app_locale) const {
  return GetInfo(AutofillType(type), app_locale);
}

bool FormGroup::SetInfo(FieldType type,
                        std::u16string_view value,
                        std::string_view app_locale) {
  return SetInfoWithVerificationStatus(type, value, app_locale,
                                       VerificationStatus::kNoStatus);
}

bool FormGroup::SetInfo(const AutofillType& type,
                        std::u16string_view value,
                        std::string_view app_locale) {
  return SetInfoWithVerificationStatus(type, value, app_locale,
                                       VerificationStatus::kNoStatus);
}

bool FormGroup::HasInfo(FieldType type) const {
  return HasInfo(AutofillType(type));
}

bool FormGroup::HasInfo(const AutofillType& type) const {
  // Use "en-US" as a placeholder locale. We are only interested in emptiness,
  // not in the presentation of the string.
  return !GetInfo(type, "en-US").empty();
}

bool FormGroup::SetInfoWithVerificationStatus(FieldType type,
                                              std::u16string_view value,
                                              std::string_view app_locale,
                                              const VerificationStatus status) {
  return SetInfoWithVerificationStatus(AutofillType(type), value, app_locale,
                                       status);
}

void FormGroup::SetRawInfo(FieldType type, std::u16string_view value) {
  SetRawInfoWithVerificationStatus(type, value, VerificationStatus::kNoStatus);
}

LogBuffer& operator<<(LogBuffer& buffer, const FormGroup& form_group) {
  base::flat_map<std::string, FieldType> sorted_types;
  for (FieldType type : form_group.GetSupportedTypes()) {
    sorted_types[std::string(FieldTypeToStringView(type))] = type;
  }

  buffer << Tag{"table"};
  for (const auto& [type_string, type] : sorted_types) {
    std::u16string value = form_group.GetRawInfo(type);
    if (value.empty()) {
      continue;
    }
    LogBuffer rendered_value;
    rendered_value << Tag{"span"} << Attrib{"style", "white-space: pre"}
                   << base::StrCat(
                          {base::UTF16ToUTF8(value), " (",
                           std::string(VerificationStatusToStringView(
                               form_group.GetVerificationStatus(type))),
                           ")"})
                   << CTag{"span"};
    buffer << Tr{} << type_string << std::move(rendered_value);
  }
  buffer << CTag{"table"};
  return buffer;
}

}  // namespace autofill
