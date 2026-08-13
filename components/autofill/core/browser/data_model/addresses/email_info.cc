// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/email_info.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

EmailInfo::EmailInfo() = default;

EmailInfo::EmailInfo(const EmailInfo& info) = default;

EmailInfo::EmailInfo(EmailInfo&& info) noexcept = default;

EmailInfo& EmailInfo::operator=(const EmailInfo& info) = default;

EmailInfo& EmailInfo::operator=(EmailInfo&& info) noexcept = default;

EmailInfo::~EmailInfo() = default;

bool EmailInfo::operator==(const EmailInfo& other) const {
  return this == &other || email_ == other.email_;
}

FieldTypeSet EmailInfo::GetSupportedTypes() const {
  static constexpr FieldTypeSet supported_types{EMAIL_ADDRESS};
  return supported_types;
}

std::u16string EmailInfo::GetInfo(const AutofillType& type,
                                  std::string_view app_locale) const {
  return GetRawInfo(type.GetAddressType());
}

std::u16string EmailInfo::GetRawInfo(FieldType type) const {
  if (type == EMAIL_ADDRESS || type == EMAIL_OR_LOYALTY_MEMBERSHIP_ID) {
    return email_;
  }

  return std::u16string();
}

void EmailInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                 std::u16string_view value,
                                                 VerificationStatus status) {
  CHECK(type == EMAIL_ADDRESS || type == EMAIL_OR_LOYALTY_MEMBERSHIP_ID);
  email_ = value;
}

bool EmailInfo::SetInfoWithVerificationStatus(const AutofillType& type,
                                              std::u16string_view value,
                                              std::string_view app_locale,
                                              const VerificationStatus status) {
  SetRawInfoWithVerificationStatus(type.GetAddressType(), value, status);
  return true;
}

VerificationStatus EmailInfo::GetVerificationStatus(FieldType type) const {
  return VerificationStatus::kNoStatus;
}

}  // namespace autofill
