// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_CONTACT_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_CONTACT_INFO_H_

#include <memory>
#include <string>
#include <string_view>

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
  explicit NameInfo(bool alternative_names_supported);
  NameInfo(const NameInfo& info);
  NameInfo(std::unique_ptr<NameFull> name,
           std::unique_ptr<AlternativeFullName> alternative_name);
  NameInfo& operator=(const NameInfo& info);
  ~NameInfo() override;

  // Populates `result_name_info` with the result of merging the names in
  // `new_name_info` and `old_name_info`. Returns true if successful. Expects
  // that `new_name_info` and `old_name_info` have already been found to be
  // mergeable. Regular names are merged first, after they are done, merging of
  // alternative names starts.
  // TODO(crbug.com/359768803): Make this function non-static when NameInfo
  // becomes CountryCode aware.
  static bool MergeNames(const NameInfo& new_name_info,
                         AddressCountryCode new_country_code,
                         const NameInfo& old_name_info,
                         AddressCountryCode old_country_code,
                         NameInfo& result_name_info);

  // Returns true if `name_info_1` and `name_info_2` names are mergeable,
  // that is one of the names is empty, the names are the same, or one name is a
  // variation of the other. The comparison is insensitive to case, punctuation
  // and diacritics.
  // TODO(crbug.com/359768803): Make this function non-static when NameInfo
  // becomes CountryCode aware.
  static bool AreNamesMergeable(const NameInfo& name_info_1,
                                const AddressCountryCode country_code_1,
                                const NameInfo& name_info_2,
                                const AddressCountryCode country_code_2);

  // Returns true if `name_info_1` and `name_info_2` alternative names are
  // mergeable, that is one of the alternative names is empty, alternative names
  // are the same, or one alternative name is a variation of the other. The
  // comparison is insensitive to case, punctuation and diacritics.
  // TODO(crbug.com/359768803): Make this function non-static when NameInfo
  // becomes CountryCode aware.
  static bool AreAlternativeNamesMergeable(
      const NameInfo& name_info_1,
      const AddressCountryCode country_code_1,
      const NameInfo& name_info_2,
      const AddressCountryCode country_code_2);

  bool operator==(const NameInfo& other) const;

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

  // Returns true if `full_name` is a variant of name stored in `this`.
  //
  // This function generates all variations of `full_name` and returns true if
  // one of these variants is equal to the one stored in `this`. For example,
  // this function will return true if `this` holds "john q public" and
  // `full_name` is "john quincy public" because full_name hold by `this` can be
  // derived from `full_name` by using the middle initial. Note that the reverse
  // is not true, "john quincy public" is not a name variant of "john q public".
  bool IsNameVariantOf(std::u16string_view full_name,
                       std::string_view app_locale) const;

  // Returns the storable type of `type`, if it exists.
  // It should only be used for `FieldTypeGroup::kName` types.
  std::optional<FieldType> GetStorableTypeOf(FieldType type) const;

  // Returns true if the regular name should be migrated to a phonetic name.
  // The incorrect assignment happened in the past when we did not have proper
  // support for phonetic names.
  // TODO(crbug.com/359768803): Remove this method once the migration is done.
  bool HasNameEligibleForPhoneticNameMigration() const;

  // Moves the regular name data to the phonetic name fields and clears the
  // regular name tree.
  // TODO(crbug.com/359768803): Remove this method once the migration is done.
  void MigrateRegularNameToPhoneticName();

  // Convenience method to get the value of `field_type` to be used for
  // comparison. Returns an empty string if `field_type` is not
  // supported. `common_country_code` of this and the component it's being
  // compared against is required for consistent application of rewriting rules.
  std::u16string GetValueForComparisonForType(
      FieldType field_type,
      const AddressCountryCode& common_country_code) const;

  // Returns the root node of either `name_` or `alternative_name_`
  // depending on the `type`.
  // This node is unique by definition.
  // TODO(crbug.com/359768803): Remove this method once merging functions
  // become non-static.
  const AddressComponent* GetRootForType(FieldType type) const;

  // Informs `this` that the country code of the owning `AutofillProfile`
  // changed. When called creates or destroys the alternative name tree
  // (depending on the support).
  void OnCountryChange(const AddressCountryCode& new_country_code);

 private:
  // Returns the root node of either `name_` or `alternative_name_`
  // depending on the `type`.
  // This node is unique by definition.
  AddressComponent* GetRootForType(FieldType type);

  // Returns if the `alternative_name_` tree exists.
  bool IsAlternativeNameSupported() const;

  void CreateAlternativeNameTree();
  void DeleteAlternativeNameTree();

  // Returns true if `this` and `other` have matching support for alternative
  // names (i.e. both support them, or neither supports them), returns false
  // otherwise.
  bool HaveSimilarAlternativeNameSupport(const NameInfo& other) const;

  // This data structures store structured representation of the name and
  // alternative (e.g. phonetic) name.
  const std::unique_ptr<NameFull> name_;
  // Exists only if `this` supports alternative names. Currently it is
  // only used for japanese profiles.
  std::unique_ptr<AlternativeFullName> alternative_name_;
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

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_CONTACT_INFO_H_
