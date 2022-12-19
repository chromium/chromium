// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/form_group.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/common/autofill_l10n_util.h"

namespace autofill {

void FormGroup::GetMatchingTypes(const std::u16string& text,
                                 const std::string& app_locale,
                                 ServerFieldTypeSet* matching_types) const {
  if (text.empty()) {
    matching_types->insert(EMPTY_TYPE);
    return;
  }

  AutofillProfileComparator comparator(app_locale);
  if (comparator.HasOnlySkippableCharacters(text)) {
    return;
  }

  std::u16string canonicalized_text = comparator.NormalizeForComparison(text);
  ServerFieldTypeSet types;
  GetSupportedTypes(&types);
  for (auto type : types) {
    if (comparator.Compare(canonicalized_text,
                           GetInfo(AutofillType(type), app_locale))) {
      matching_types->insert(type);
    }
  }
}

void FormGroup::GetNonEmptyTypes(const std::string& app_locale,
                                 ServerFieldTypeSet* non_empty_types) const {
  ServerFieldTypeSet types;
  GetSupportedTypes(&types);
  for (auto type : types) {
    if (!GetInfo(AutofillType(type), app_locale).empty())
      non_empty_types->insert(type);
  }
}

void FormGroup::GetNonEmptyRawTypes(ServerFieldTypeSet* non_empty_types) const {
  ServerFieldTypeSet types;
  GetSupportedTypes(&types);
  for (auto type : types) {
    if (!GetRawInfo(type).empty())
      non_empty_types->insert(type);
  }
}

int FormGroup::GetRawInfoAsInt(ServerFieldType type) const {
  NOTREACHED();
  return 0;
}

bool FormGroup::HasRawInfo(ServerFieldType type) const {
  return !GetRawInfo(type).empty();
}

std::u16string FormGroup::GetInfo(ServerFieldType type,
                                  const std::string& app_locale) const {
  return GetInfoImpl(AutofillType(type), app_locale);
}

std::u16string FormGroup::GetInfo(const AutofillType& type,
                                  const std::string& app_locale) const {
  return GetInfoImpl(type, app_locale);
}

VerificationStatus FormGroup::GetVerificationStatus(
    ServerFieldType type) const {
  return GetVerificationStatusImpl(type);
}

VerificationStatus FormGroup::GetVerificationStatus(
    const AutofillType& type) const {
  return GetVerificationStatus(type.GetStorableType());
}

int FormGroup::GetVerificationStatusInt(ServerFieldType type) const {
  return static_cast<int>(GetVerificationStatus(type));
}

int FormGroup::GetVerificationStatusInt(const AutofillType& type) const {
  return static_cast<int>(GetVerificationStatus(type));
}

bool FormGroup::SetInfo(ServerFieldType type,
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

bool FormGroup::SetInfoWithVerificationStatus(ServerFieldType type,
                                              const std::u16string& value,
                                              const std::string& app_locale,
                                              const VerificationStatus status) {
  return SetInfoWithVerificationStatusImpl(AutofillType(type), value,
                                           app_locale, status);
}

bool FormGroup::SetInfoWithVerificationStatus(const AutofillType& type,
                                              const std::u16string& value,
                                              const std::string& app_locale,
                                              VerificationStatus status) {
  return SetInfoWithVerificationStatusImpl(type, value, app_locale, status);
}

bool FormGroup::HasInfo(ServerFieldType type) const {
  return HasInfo(AutofillType(type));
}

bool FormGroup::HasInfo(const AutofillType& type) const {
  // Use "en-US" as a placeholder locale. We are only interested in emptiness,
  // not in the presentation of the string.
  return !GetInfo(type, "en-US").empty();
}

std::u16string FormGroup::GetInfoImpl(const AutofillType& type,
                                      const std::string& app_locale) const {
  return GetRawInfo(type.GetStorableType());
}

bool FormGroup::SetInfoWithVerificationStatusImpl(const AutofillType& type,
                                                  const std::u16string& value,
                                                  const std::string& app_locale,
                                                  VerificationStatus status) {
  SetRawInfoWithVerificationStatus(type.GetStorableType(), value, status);
  return true;
}

void FormGroup::SetRawInfoAsIntWithVerificationStatus(
    ServerFieldType type,
    int value,
    VerificationStatus status) {
  SetRawInfoWithVerificationStatus(type, base::NumberToString16(value), status);
}

void FormGroup::SetRawInfoWithVerificationStatusInt(ServerFieldType type,
                                                    const std::u16string& value,
                                                    int status) {
  SetRawInfoWithVerificationStatus(type, value,
                                   static_cast<VerificationStatus>(status));
}

void FormGroup::SetRawInfo(ServerFieldType type, const std::u16string& value) {
  SetRawInfoWithVerificationStatus(type, value, VerificationStatus::kNoStatus);
}

void FormGroup::SetRawInfoAsInt(ServerFieldType type, int value) {
  SetRawInfoAsIntWithVerificationStatus(type, value,
                                        VerificationStatus::kNoStatus);
}

VerificationStatus FormGroup::GetVerificationStatusImpl(
    ServerFieldType type) const {
  return VerificationStatus::kNoStatus;
}

}  // namespace autofill
