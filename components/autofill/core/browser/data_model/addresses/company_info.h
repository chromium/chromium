// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_COMPANY_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_COMPANY_INFO_H_

#include <string>
#include <string_view>

#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class AutofillType;

class CompanyInfo : public FormGroup {
 public:
  // See `AutofillProfile::kDatabaseStoredTypes` for a documentation of the
  // purpose of this constant.
  static constexpr FieldTypeSet kDatabaseStoredTypes{COMPANY_NAME};
  CompanyInfo();
  CompanyInfo(const CompanyInfo& info);
  CompanyInfo(CompanyInfo&& info) noexcept;
  CompanyInfo& operator=(const CompanyInfo& info);
  CompanyInfo& operator=(CompanyInfo&& info) noexcept;
  ~CompanyInfo() override;

  bool operator==(const CompanyInfo& other) const;

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
                                     std::string_view locale,
                                     VerificationStatus status) override;

  VerificationStatus GetVerificationStatus(FieldType type) const override;

  // The `company_name_` is considered valid if it doesn't look like a birthdate
  // or social title. Only valid company names are considered for voting.
  bool IsValid() const;

 private:
  // FormGroup:
  FieldTypeSet GetSupportedTypes() const override;
  void GetMatchingTypes(std::u16string_view text,
                        std::string_view app_locale,
                        FieldTypeSet* matching_types) const override;

  std::u16string company_name_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_COMPANY_INFO_H_
