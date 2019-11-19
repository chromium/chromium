// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/contact_info.h"

#include <stddef.h>
#include <ostream>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {

NameInfo::NameInfo() {}

NameInfo::NameInfo(const NameInfo& info) {
  *this = info;
}

NameInfo::~NameInfo() {}

NameInfo& NameInfo::operator=(const NameInfo& info) {
  if (this == &info)
    return *this;

  given_ = info.given_;
  middle_ = info.middle_;
  family_ = info.family_;
  full_ = info.full_;
  return *this;
}

bool NameInfo::operator==(const NameInfo& other) const {
  if (this == &other)
    return true;
  return given_ == other.given_ && middle_ == other.middle_ &&
         family_ == other.family_ && full_ == other.full_;
}

base::string16 NameInfo::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(NAME, AutofillType(type).group());
  switch (type) {
    case NAME_FIRST:
      return given_;

    case NAME_MIDDLE:
      return middle_;

    case NAME_LAST:
      return family_;

    case NAME_MIDDLE_INITIAL:
      return MiddleInitial();

    case NAME_FULL:
      return full_;

    default:
      return base::string16();
  }
}

void NameInfo::SetRawInfo(ServerFieldType type, const base::string16& value) {
  DCHECK_EQ(NAME, AutofillType(type).group());
  switch (type) {
    case NAME_FIRST:
      given_ = value;
      break;

    case NAME_MIDDLE:
    case NAME_MIDDLE_INITIAL:
      middle_ = value;
      break;

    case NAME_LAST:
      family_ = value;
      break;

    case NAME_FULL:
      full_ = value;
      break;

    default:
      NOTREACHED();
  }
}

void NameInfo::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(NAME_FIRST);
  supported_types->insert(NAME_MIDDLE);
  supported_types->insert(NAME_LAST);
  supported_types->insert(NAME_MIDDLE_INITIAL);
  supported_types->insert(NAME_FULL);
}

base::string16 NameInfo::GetInfoImpl(const AutofillType& type,
                                     const std::string& app_locale) const {
  if (type.GetStorableType() == NAME_FULL)
    return FullName();

  return GetRawInfo(type.GetStorableType());
}

bool NameInfo::SetInfoImpl(const AutofillType& type,
                           const base::string16& value,
                           const std::string& app_locale) {
  // Always clear out the full name if we're making a change.
  if (value != GetInfo(type, app_locale))
    full_.clear();

  if (type.GetStorableType() == NAME_FULL) {
    SetFullName(value);
    return true;
  }

  return FormGroup::SetInfoImpl(type, value, app_locale);
}

base::string16 NameInfo::FullName() const {
  if (!full_.empty())
    return full_;

  return data_util::JoinNameParts(given_, middle_, family_);
}

base::string16 NameInfo::MiddleInitial() const {
  return middle_.empty() ? base::string16() : middle_.substr(0U, 1U);
}

void NameInfo::SetFullName(const base::string16& full) {
  full_ = full;
  data_util::NameParts parts = data_util::SplitName(full);
  given_ = parts.given;
  middle_ = parts.middle;
  family_ = parts.family;
}

EmailInfo::EmailInfo() {}

EmailInfo::EmailInfo(const EmailInfo& info) {
  *this = info;
}

EmailInfo::~EmailInfo() {}

EmailInfo& EmailInfo::operator=(const EmailInfo& info) {
  if (this == &info)
    return *this;

  email_ = info.email_;
  return *this;
}

bool EmailInfo::operator==(const EmailInfo& other) const {
  return this == &other || email_ == other.email_;
}

void EmailInfo::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(EMAIL_ADDRESS);
}

base::string16 EmailInfo::GetRawInfo(ServerFieldType type) const {
  if (type == EMAIL_ADDRESS)
    return email_;

  return base::string16();
}

void EmailInfo::SetRawInfo(ServerFieldType type, const base::string16& value) {
  DCHECK_EQ(EMAIL_ADDRESS, type);
  email_ = value;
}

CompanyInfo::CompanyInfo() {}

CompanyInfo::CompanyInfo(const AutofillProfile* profile) : profile_(profile) {}

CompanyInfo::CompanyInfo(const CompanyInfo& info) {
  *this = info;
}

CompanyInfo::~CompanyInfo() {}

CompanyInfo& CompanyInfo::operator=(const CompanyInfo& info) {
  if (this == &info)
    return *this;

  company_name_ = info.GetRawInfo(COMPANY_NAME);
  return *this;
}

bool CompanyInfo::operator==(const CompanyInfo& other) const {
  return this == &other ||
         GetRawInfo(COMPANY_NAME) == other.GetRawInfo(COMPANY_NAME);
}

void CompanyInfo::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(COMPANY_NAME);
}

base::string16 CompanyInfo::GetRawInfo(ServerFieldType type) const {
  return IsValidOrVerified(company_name_) ? company_name_ : base::string16();
}

void CompanyInfo::SetRawInfo(ServerFieldType type,
                             const base::string16& value) {
  DCHECK_EQ(COMPANY_NAME, type);
  company_name_ = value;
}

bool CompanyInfo::IsValidOrVerified(const base::string16& value) const {
  if (profile_ && profile_->IsVerified()) {
    return true;
  }
  // |value| is a birthyear:
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillRejectCompanyBirthyear) &&
      MatchesPattern(value, base::UTF8ToUTF16("^(19|20)\\d{2}$"))) {
    return false;
  }
  // |value| is a social title:
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillRejectCompanySocialTitle) &&
      MatchesPattern(value, base::UTF8ToUTF16(
                                "^(Ms\\.?|Mrs\\.?|Mr\\.?|Miss|Mistress|Mister|"
                                "Frau|Herr|"
                                "Mlle|Mme|M\\.|"
                                "Dr\\.?|Prof\\.?)$"))) {
    return false;
  }
  return true;
}

}  // namespace autofill
