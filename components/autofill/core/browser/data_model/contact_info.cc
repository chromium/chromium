// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/contact_info.h"

#include <stddef.h>
#include <ostream>
#include <string>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {

NameInfo::NameInfo() : name_(std::make_unique<NameFull>()) {}

NameInfo::NameInfo(const NameInfo& info) : NameInfo() {
  *this = info;
}

NameInfo::~NameInfo() = default;

NameInfo& NameInfo::operator=(const NameInfo& info) {
  if (this == &info)
    return *this;

  name_->CopyFrom(*info.name_);

  return *this;
}

bool NameInfo::MergeStructuredName(const NameInfo& newer) {
  return name_->MergeWithComponent(newer.GetStructuredName());
}

void NameInfo::MergeStructuredNameValidationStatuses(const NameInfo& newer) {
  name_->MergeVerificationStatuses(newer.GetStructuredName());
}

bool NameInfo::IsStructuredNameMergeable(const NameInfo& newer) const {
  return name_->IsMergeableWithComponent(newer.GetStructuredName());
}

bool NameInfo::FinalizeAfterImport() {
  name_->MigrateLegacyStructure();
  return name_->CompleteFullTree();
}

bool NameInfo::operator==(const NameInfo& other) const {
  if (this == &other)
    return true;

  return name_->SameAs(*other.name_);
}

std::u16string NameInfo::GetRawInfo(FieldType type) const {
  DCHECK_EQ(FieldTypeGroup::kName, GroupTypeOfFieldType(type));
  return name_->GetValueForType(type);
}

void NameInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                const std::u16string& value,
                                                VerificationStatus status) {
  DCHECK_EQ(FieldTypeGroup::kName, GroupTypeOfFieldType(type));
  name_->SetValueForType(type, value, status);
}

void NameInfo::GetSupportedTypes(FieldTypeSet* supported_types) const {
  name_->GetSupportedTypes(supported_types);
}

std::u16string NameInfo::GetInfoImpl(const AutofillType& type,
                                     const std::string& app_locale) const {
  return GetRawInfo(type.GetStorableType());
}

bool NameInfo::SetInfoWithVerificationStatusImpl(const AutofillType& type,
                                                 const std::u16string& value,
                                                 const std::string& app_locale,
                                                 VerificationStatus status) {
  if (type.GetStorableType() == NAME_FULL) {
    // If the set string is token equivalent to the old one, the value can
    // just be updated, otherwise create a new name record and complete it in
    // the end.
    // TODO(crbug.com/40266145): Move this logic to the data model.
    AreStringTokenEquivalent(value, name_->GetValueForType(NAME_FULL))
        ? name_->SetValueForType(type.GetStorableType(), value, status)
        : name_->SetValueForTypeAndResetSubstructure(type.GetStorableType(),
                                                     value, status);
    return true;
  }
  return FormGroup::SetInfoWithVerificationStatusImpl(type, value, app_locale,
                                                      status);
}

VerificationStatus NameInfo::GetVerificationStatusImpl(FieldType type) const {
  return name_->GetVerificationStatusForType(type);
}

EmailInfo::EmailInfo() = default;

EmailInfo::EmailInfo(const EmailInfo& info) {
  *this = info;
}

EmailInfo::~EmailInfo() = default;

EmailInfo& EmailInfo::operator=(const EmailInfo& info) {
  if (this == &info)
    return *this;

  email_ = info.email_;
  return *this;
}

bool EmailInfo::operator==(const EmailInfo& other) const {
  return this == &other || email_ == other.email_;
}

void EmailInfo::GetSupportedTypes(FieldTypeSet* supported_types) const {
  supported_types->insert(EMAIL_ADDRESS);
}

std::u16string EmailInfo::GetRawInfo(FieldType type) const {
  if (type == EMAIL_ADDRESS)
    return email_;

  return std::u16string();
}

void EmailInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                 const std::u16string& value,
                                                 VerificationStatus status) {
  DCHECK_EQ(EMAIL_ADDRESS, type);
  email_ = value;
}

CompanyInfo::CompanyInfo() = default;

CompanyInfo::CompanyInfo(const CompanyInfo& info) = default;

CompanyInfo::~CompanyInfo() = default;

bool CompanyInfo::operator==(const CompanyInfo& other) const {
  return this == &other ||
         GetRawInfo(COMPANY_NAME) == other.GetRawInfo(COMPANY_NAME);
}

void CompanyInfo::GetSupportedTypes(FieldTypeSet* supported_types) const {
  supported_types->insert(COMPANY_NAME);
}

void CompanyInfo::GetMatchingTypesWithProfileSources(
    const std::u16string& text,
    const std::string& app_locale,
    FieldTypeSet* matching_types,
    PossibleProfileValueSources* profile_value_sources) const {
  if (IsValid()) {
    FormGroup::GetMatchingTypesWithProfileSources(
        text, app_locale, matching_types, profile_value_sources);
  } else if (text.empty()) {
    matching_types->insert(EMPTY_TYPE);
  }
}

std::u16string CompanyInfo::GetRawInfo(FieldType type) const {
  return company_name_;
}

void CompanyInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                   const std::u16string& value,
                                                   VerificationStatus status) {
  DCHECK_EQ(COMPANY_NAME, type);
  company_name_ = value;
}

bool CompanyInfo::IsValid() const {
  static constexpr char16_t kBirthyearRe[] = u"^(19|20)\\d{2}$";
  static constexpr char16_t kSocialTitleRe[] =
      u"^(Ms\\.?|Mrs\\.?|Mr\\.?|Miss|Mistress|Mister|"
      u"Frau|Herr|"
      u"Mlle|Mme|M\\.|"
      u"Dr\\.?|Prof\\.?)$";
  return !MatchesRegex<kBirthyearRe>(company_name_) &&
         !MatchesRegex<kSocialTitleRe>(company_name_);
}

}  // namespace autofill
