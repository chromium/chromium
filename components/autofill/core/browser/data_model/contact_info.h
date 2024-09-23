// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CONTACT_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CONTACT_INFO_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/form_group.h"

namespace autofill {

// A form group that stores name information.
class NameInfo : public FormGroup {
 public:
  NameInfo();
  NameInfo(const NameInfo& info);
  ~NameInfo() override;

  NameInfo& operator=(const NameInfo& info);
  bool operator==(const NameInfo& other) const;

  // FormGroup:
  std::u16string GetRawInfo(FieldType type) const override;

  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;

  // Derives all missing tokens in the structured representation of the name by
  // either parsing missing tokens from their assigned parent or by formatting
  // them from their assigned children.
  // Return false if the completion is not possible either because no value is
  // set or because there are two conflicting values set. Two values are
  // conflicting iff they are on the same root-to-leaf path.
  // For example, NAME_FIRST is child of NAME_LAST and if both are set, the tree
  // cannot be completed.
  bool FinalizeAfterImport();

  // Returns true if the structured-name information in |this| and |newer| are
  // mergeable. Note, returns false if |newer| is variant of |this| or vice
  // versa. A name variant is a variation that allows for abbreviations, a
  // reordering and omission of the tokens.
  bool IsStructuredNameMergeable(const NameInfo& newer) const;

  // Merges the structured name-information of |newer| into |this|.
  bool MergeStructuredName(const NameInfo& newer);

  // Merges the validation statuses of |newer| into |this|.
  // If two tokens of the same type have the exact same value, the validation
  // status is updated to the higher one.
  void MergeStructuredNameValidationStatuses(const NameInfo& newer);

  // Returns a constant reference to the structured name tree.
  const AddressComponent& GetStructuredName() const { return *name_; }

 private:
  // FormGroup:
  void GetSupportedTypes(FieldTypeSet* supported_types) const override;
  std::u16string GetInfoImpl(const AutofillType& type,
                             const std::string& app_locale) const override;

  bool SetInfoWithVerificationStatusImpl(const AutofillType& type,
                                         const std::u16string& value,
                                         const std::string& app_locale,
                                         VerificationStatus status) override;

  // Return the verification status of a structured name value.
  VerificationStatus GetVerificationStatusImpl(FieldType type) const override;

  // This data structure stores the more-structured representation of the name
  // when |features::kAutofillEnableSupportForMoreStructureInNames| is enabled.
  const std::unique_ptr<AddressComponent> name_;
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
  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;

 private:
  // FormGroup:
  void GetSupportedTypes(FieldTypeSet* supported_types) const override;

  std::u16string email_;
};

class CompanyInfo : public FormGroup {
 public:
  CompanyInfo();
  CompanyInfo(const CompanyInfo& info);
  ~CompanyInfo() override;

  bool operator==(const CompanyInfo& other) const;
  bool operator!=(const CompanyInfo& other) const { return !operator==(other); }

  // FormGroup:
  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;

  // The `company_name_` is considered valid if it doesn't look like a birthdate
  // or social title. Only valid company names are considered for voting.
  bool IsValid() const;

 private:
  // FormGroup:
  void GetSupportedTypes(FieldTypeSet* supported_types) const override;
  void GetMatchingTypesWithProfileSources(
      const std::u16string& text,
      const std::string& app_locale,
      FieldTypeSet* matching_types,
      PossibleProfileValueSources* profile_value_sources) const override;

  std::u16string company_name_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CONTACT_INFO_H_
