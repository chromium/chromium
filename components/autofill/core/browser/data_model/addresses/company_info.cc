// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/company_info.h"

#include <string>
#include <string_view>

#include "base/check_op.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {

CompanyInfo::CompanyInfo() = default;

CompanyInfo::CompanyInfo(const CompanyInfo& info) = default;

CompanyInfo::CompanyInfo(CompanyInfo&& info) noexcept = default;

CompanyInfo& CompanyInfo::operator=(const CompanyInfo& info) = default;

CompanyInfo& CompanyInfo::operator=(CompanyInfo&& info) noexcept = default;

CompanyInfo::~CompanyInfo() = default;

bool CompanyInfo::operator==(const CompanyInfo& other) const {
  return this == &other ||
         GetRawInfo(COMPANY_NAME) == other.GetRawInfo(COMPANY_NAME);
}

FieldTypeSet CompanyInfo::GetSupportedTypes() const {
  static constexpr FieldTypeSet supported_types{COMPANY_NAME};
  return supported_types;
}

void CompanyInfo::GetMatchingTypes(std::u16string_view text,
                                   std::string_view app_locale,
                                   FieldTypeSet* matching_types) const {
  if (IsValid()) {
    FormGroup::GetMatchingTypes(text, app_locale, matching_types);
  } else if (text.empty()) {
    matching_types->insert(EMPTY_TYPE);
  }
}

std::u16string CompanyInfo::GetInfo(const AutofillType& type,
                                    std::string_view app_locale) const {
  return GetRawInfo(type.GetAddressType());
}

std::u16string CompanyInfo::GetRawInfo(FieldType type) const {
  CHECK_EQ(COMPANY_NAME, type);
  return company_name_;
}

void CompanyInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                   std::u16string_view value,
                                                   VerificationStatus status) {
  CHECK_EQ(COMPANY_NAME, type);
  company_name_ = value;
}

bool CompanyInfo::SetInfoWithVerificationStatus(
    const AutofillType& type,
    std::u16string_view value,
    std::string_view app_locale,
    const VerificationStatus status) {
  SetRawInfoWithVerificationStatus(type.GetAddressType(), value, status);
  return true;
}

VerificationStatus CompanyInfo::GetVerificationStatus(FieldType type) const {
  return VerificationStatus::kNoStatus;
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
