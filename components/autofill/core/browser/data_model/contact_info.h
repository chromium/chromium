// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CONTACT_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CONTACT_INFO_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/data_model/form_group.h"

namespace autofill {

class AutofillProfile;

// A form group that stores name information.
class NameInfo : public FormGroup {
 public:
  NameInfo();
  NameInfo(const NameInfo& info);
  ~NameInfo() override;

  NameInfo& operator=(const NameInfo& info);
  bool operator==(const NameInfo& other) const;
  bool operator!=(const NameInfo& other) const { return !operator==(other); }

  // FormGroup:
  base::string16 GetRawInfo(ServerFieldType type) const override;
  void SetRawInfo(ServerFieldType type, const base::string16& value) override;

 private:
  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;
  base::string16 GetInfoImpl(const AutofillType& type,
                             const std::string& app_locale) const override;
  bool SetInfoImpl(const AutofillType& type,
                   const base::string16& value,
                   const std::string& app_locale) override;

  // Returns the full name, which is either |full_|, or if |full_| is empty,
  // is composed of given, middle and family.
  base::string16 FullName() const;

  // Returns the middle initial if |middle_| is non-empty.  Returns an empty
  // string otherwise.
  base::string16 MiddleInitial() const;

  // Sets |given_|, |middle_|, and |family_| to the tokenized |full|.
  void SetFullName(const base::string16& full);

  base::string16 given_;
  base::string16 middle_;
  base::string16 family_;
  base::string16 full_;
};

class EmailInfo : public FormGroup {
 public:
  EmailInfo();
  EmailInfo(const EmailInfo& info);
  ~EmailInfo() override;

  EmailInfo& operator=(const EmailInfo& info);
  bool operator==(const EmailInfo& other) const;
  bool operator!=(const EmailInfo& other) const { return !operator==(other); }

  // FormGroup:
  base::string16 GetRawInfo(ServerFieldType type) const override;
  void SetRawInfo(ServerFieldType type, const base::string16& value) override;

 private:
  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;

  base::string16 email_;
};

class CompanyInfo : public FormGroup {
 public:
  CompanyInfo();
  CompanyInfo(const CompanyInfo& info);
  explicit CompanyInfo(const AutofillProfile* profile);
  ~CompanyInfo() override;

  CompanyInfo& operator=(const CompanyInfo& info);
  bool operator==(const CompanyInfo& other) const;
  bool operator!=(const CompanyInfo& other) const { return !operator==(other); }

  // FormGroup:
  base::string16 GetRawInfo(ServerFieldType type) const override;
  void SetRawInfo(ServerFieldType type, const base::string16& value) override;
  void set_profile(const AutofillProfile* profile) { profile_ = profile; }

 private:
  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;
  bool IsValidOrVerified(const base::string16& value) const;

  base::string16 company_name_;
  const AutofillProfile* profile_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CONTACT_INFO_H_
