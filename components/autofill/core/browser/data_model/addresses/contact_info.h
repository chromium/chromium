// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_CONTACT_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_CONTACT_INFO_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// A form group that stores name information.
class NameInfo : public FormGroup {
 public:
  // See `AutofillProfile::kDatabaseStoredTypes` for a documentation of the
  // purpose of this constant.
  static constexpr FieldTypeSet kDatabaseStoredTypes{NAME_HONORIFIC_PREFIX,
                                                     NAME_FIRST,
                                                     NAME_MIDDLE,
                                                     NAME_LAST_FIRST,
                                                     NAME_LAST_CONJUNCTION,
                                                     NAME_LAST_SECOND,
                                                     NAME_LAST_PREFIX,
                                                     NAME_LAST_CORE,
                                                     NAME_LAST,
                                                     NAME_FULL,
                                                     ALTERNATIVE_FULL_NAME,
                                                     ALTERNATIVE_GIVEN_NAME,
                                                     ALTERNATIVE_FAMILY_NAME};
  NameInfo();
  NameInfo(const NameInfo& info);
  NameInfo(std::unique_ptr<NameFull> name,
           std::unique_ptr<AlternativeFullName> alternative_name);
  NameInfo& operator=(const NameInfo& info);
  ~NameInfo() override;

  bool operator==(const NameInfo& other) const;

  // FormGroup:
  std::u16string GetInfo(const AutofillType& type,
                         const std::string& app_locale) const override;
  std::u16string GetRawInfo(FieldType type) const override;

  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  bool SetInfoWithVerificationStatus(const AutofillType& type,
                                     const std::u16string& value,
                                     const std::string& app_locale,
                                     VerificationStatus status) override;
  // Return the verification status of a structured name value.
  VerificationStatus GetVerificationStatus(FieldType type) const override;
  FieldTypeSet GetSupportedTypes() const override;

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

  // Returns a constant reference to the structured alternative name tree.
  const AddressComponent& GetStructuredAlternativeName() const {
    return *alternative_name_;
  }

  // Returns the root node of either `name_` or `alternative_name_`
  // depending on the `type`.
  // This node is unique by definition.
  const AddressComponent* GetRootForType(FieldType type) const;

 private:
  // Returns the root node of either `name_` or `alternative_name_`
  // depending on the `type`.
  // This node is unique by definition.
  AddressComponent* GetRootForType(FieldType type);

  // This data structures store structured representation of the name and
  // alternative (e.g. phonetic) name.
  const std::unique_ptr<NameFull> name_;
  const std::unique_ptr<AlternativeFullName> alternative_name_;
};

class EmailInfo : public FormGroup {
 public:
  // See `AutofillProfile::kDatabaseStoredTypes` for a documentation of the
  // purpose of this constant.
  static constexpr FieldTypeSet kDatabaseStoredTypes{EMAIL_ADDRESS};
  EmailInfo();
  EmailInfo(const EmailInfo& info);
  EmailInfo& operator=(const EmailInfo& info);
  ~EmailInfo() override;

  bool operator==(const EmailInfo& other) const;

  // FormGroup:
  std::u16string GetInfo(const AutofillType& type,
                         const std::string& app_locale) const override;
  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  bool SetInfoWithVerificationStatus(const AutofillType& type,
                                     const std::u16string& value,
                                     const std::string& app_locale,
                                     const VerificationStatus status) override;
  VerificationStatus GetVerificationStatus(FieldType type) const override;

 private:
  // FormGroup:
  FieldTypeSet GetSupportedTypes() const override;

  std::u16string email_;
};

class CompanyInfo : public FormGroup {
 public:
  // See `AutofillProfile::kDatabaseStoredTypes` for a documentation of the
  // purpose of this constant.
  static constexpr FieldTypeSet kDatabaseStoredTypes{COMPANY_NAME};
  CompanyInfo();
  CompanyInfo(const CompanyInfo& info);
  ~CompanyInfo() override;

  bool operator==(const CompanyInfo& other) const;

  // FormGroup:
  std::u16string GetInfo(const AutofillType& type,
                         const std::string& app_locale) const override;
  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  bool SetInfoWithVerificationStatus(const AutofillType& type,
                                     const std::u16string& value,
                                     const std::string& locale,
                                     VerificationStatus status) override;

  VerificationStatus GetVerificationStatus(FieldType type) const override;

  // The `company_name_` is considered valid if it doesn't look like a birthdate
  // or social title. Only valid company names are considered for voting.
  bool IsValid() const;

 private:
  // FormGroup:
  FieldTypeSet GetSupportedTypes() const override;
  void GetMatchingTypes(const std::u16string& text,
                        const std::string& app_locale,
                        FieldTypeSet* matching_types) const override;

  std::u16string company_name_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_CONTACT_INFO_H_
