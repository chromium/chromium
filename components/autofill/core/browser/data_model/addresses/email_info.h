// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_EMAIL_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_EMAIL_INFO_H_

#include <string>
#include <string_view>

#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class AutofillType;
enum class VerificationStatus;

class EmailInfo : public FormGroup {
 public:
  // See `AutofillProfile::kDatabaseStoredTypes` for a documentation of the
  // purpose of this constant.
  static constexpr FieldTypeSet kDatabaseStoredTypes{EMAIL_ADDRESS};
  EmailInfo();
  EmailInfo(const EmailInfo& info);
  EmailInfo(EmailInfo&& info) noexcept;
  EmailInfo& operator=(const EmailInfo& info);
  EmailInfo& operator=(EmailInfo&& info) noexcept;
  ~EmailInfo() override;

  bool operator==(const EmailInfo& other) const;

  // FormGroup:
  using FormGroup::GetInfo;
  std::u16string GetInfo(const AutofillType& type,
                         std::string_view app_locale) const override;
  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        std::u16string_view value,
                                        VerificationStatus status) override;
  bool SetInfoWithVerificationStatus(const AutofillType& type,
                                     std::u16string_view value,
                                     std::string_view app_locale,
                                     const VerificationStatus status) override;
  VerificationStatus GetVerificationStatus(FieldType type) const override;

 private:
  // FormGroup:
  FieldTypeSet GetSupportedTypes() const override;

  std::u16string email_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_EMAIL_INFO_H_
