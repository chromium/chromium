// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/form_group.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/common/autofill_l10n_util.h"

namespace autofill {

void FormGroup::GetMatchingTypes(const std::u16string& text,
                                 const std::string& app_locale,
                                 FieldTypeSet* matching_types) const {
  if (text.empty()) {
    matching_types->insert(EMPTY_TYPE);
    return;
  }

  AutofillProfileComparator comparator(app_locale);
  if (comparator.HasOnlySkippableCharacters(text)) {
    return;
  }

  std::u16string canonicalized_text =
      AutofillProfileComparator::NormalizeForComparison(text);
  for (FieldType type : GetSupportedTypes()) {
    if (comparator.Compare(canonicalized_text, GetInfo(type, app_locale),
                           AutofillProfileComparator::DISCARD_WHITESPACE,
                           type)) {
      matching_types->insert(type);
    }
  }
}

void FormGroup::GetNonEmptyTypes(const std::string& app_locale,
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

bool FormGroup::SetInfo(FieldType type,
                        const std::u16string& value,
                        const std::string& app_locale) {
  return SetInfoWithVerificationStatus(type, value, app_locale,
                                       VerificationStatus::kNoStatus);
}

bool FormGroup::SetInfo(const AutofillType& type,
                        const std::u16string& value,
                        const std::string& app_locale) {
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
                                              const std::u16string& value,
                                              const std::string& app_locale,
                                              const VerificationStatus status) {
  return SetInfoWithVerificationStatus(AutofillType(type), value, app_locale,
                                       status);
}

void FormGroup::SetRawInfo(FieldType type, const std::u16string& value) {
  SetRawInfoWithVerificationStatus(type, value, VerificationStatus::kNoStatus);
}

}  // namespace autofill
