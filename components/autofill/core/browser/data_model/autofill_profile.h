// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_PROFILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_PROFILE_H_

#include <stddef.h>

#include <array>
#include <iosfwd>
#include <list>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/address.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/contact_info.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/profile_token_quality.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace autofill {

class AutofillProfileComparator;
class AutofillProfileTestApi;

// A collection of FormGroups stored in a profile.  AutofillProfile also
// implements the FormGroup interface so that owners of this object can request
// form information from the profile, and the profile will delegate the request
// to the requested form group type.
class AutofillProfile : public AutofillDataModel {
 public:
  // Describes where the profile is stored and how it is synced.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RecordType {
    // Not synced at all or synced through the `AutofillProfileSyncBridge`. This
    // corresponds to profiles that local to Autofill only.
    kLocalOrSyncable = 0,
    // Synced through the `ContactInfoSyncBridge`. This corresponds to profiles
    // that are shared beyond Autofill across different services.
    // kAccountHome and kAccountWork represent special account addresses, only a
    // single one of which can exist each.
    kAccount = 1,
    kAccountHome = 2,
    kAccountWork = 3,
    kMaxValue = kAccountWork,
  };

  // These fields are, by default, the only candidates for being added to the
  // list of profile labels. Note that the call to generate labels can specify a
  // custom set of fields, in which case such set would be used instead of this
  // one.
  // TODO(crbug.com/40285811): Change this into a FieldTypeSet once the priority
  // is not decided by the order of these entries anymore.
  static constexpr auto kDefaultDistinguishingFieldsForLabels =
      std::to_array<FieldType>(
          {NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2,
           ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_CITY,
           ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP, ADDRESS_HOME_SORTING_CODE,
           ADDRESS_HOME_COUNTRY, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER,
           COMPANY_NAME});

  // The values used to represent Autofill in the `initial_creator_id()` and
  // `last_modifier_id()`.
  static constexpr int kInitialCreatorOrModifierChrome = 70073;
  AutofillProfile(const std::string& guid,
                  RecordType record_type,
                  AddressCountryCode country_code);
  AutofillProfile(RecordType record_type, AddressCountryCode country_code);
  explicit AutofillProfile(AddressCountryCode country_code);

  AutofillProfile(const AutofillProfile& profile);
  ~AutofillProfile() override;

  AutofillProfile& operator=(const AutofillProfile& profile);

  std::string guid() const { return guid_; }
  void set_guid(std::string_view guid) { guid_ = guid; }

  // Android/Java API.
#if BUILDFLAG(IS_ANDROID)
  // Create a new Java AutofillProfile instance.
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObject(
      const std::string& app_locale) const;

  // Given a Java AutofillProfile object, create an equivalent C++ instance.
  // Java profile can represent either a new or an existing address profile
  // depending on whether `existing_profile` is set or not. If this is a new
  // address profile, Java fields are set to the newly created AutofillProfile.
  // Otherwise, `existing_profile` is copied and Java fields are set to it.
  // Setting fields to `existing_profile` is done to avoid loosing address
  // substructure by creating AutofillProfile from scratch based only on the
  // available Java fields.
  static AutofillProfile CreateFromJavaObject(
      const base::android::JavaParamRef<jobject>& jprofile,
      const AutofillProfile* existing_profile,
      const std::string& app_locale);
#endif  // BUILDFLAG(IS_ANDROID)

  // FormGroup:
  void GetMatchingTypesWithProfileSources(
      const std::u16string& text,
      const std::string& app_locale,
      FieldTypeSet* matching_types,
      PossibleProfileValueSources* profile_value_sources) const override;

  std::u16string GetRawInfo(FieldType type) const override;

  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;

  void GetSupportedTypes(FieldTypeSet* supported_types) const override;

  // Calculates the ranking score used for ranking the profile suggestion. If
  // `use_frecency` is true we use the new ranking algorithm.
  double GetRankingScore(base::Time current_time,
                         bool use_frecency = false) const;

  // Compares two profiles and returns if the current profile has a greater
  // ranking score than `other`.
  bool HasGreaterRankingThan(const AutofillProfile* other,
                             base::Time comparison_time,
                             bool use_frecency = false) const;

  // Every `GetSupportedType()` is either a storable type or has a corresponding
  // storable type. For example, ADDRESS_HOME_LINE1 corresponds to the storable
  // type ADDRESS_HOME_STREET_ADDRESS.
  // This function returns the storable type of the given `type`.
  FieldType GetStorableTypeOf(FieldType type) const;

  // Returns true if there are no values (field types) set.
  bool IsEmpty(const std::string& app_locale) const;

  // Returns true if the |type| of data in this profile is present, but invalid.
  // Otherwise returns false.
  bool IsPresentButInvalid(FieldType type) const;

  // Comparison for Sync.  Returns 0 if the profile is the same as |this|,
  // or < 0, or > 0 if it is different.  The implied ordering can be used for
  // culling duplicates.  The ordering is based on collation order of the
  // textual contents of the fields. Full profile comparison, comparison
  // includes multi-valued fields.
  //
  // GUIDs, origins, and language codes are not compared, only the contents
  // themselves.
  int Compare(const AutofillProfile& profile) const;

  // Same as operator==, but ignores differences in guid and cares about
  // differences in usage stats.
  bool EqualsForLegacySyncPurposes(const AutofillProfile& profile) const;

  // Returns true if |new_profile| and this are considered equal for updating
  // purposes, meaning that if equal we do not need to update this profile to
  // the |new_profile|.
  bool EqualsForUpdatePurposes(const AutofillProfile& new_profile) const;

  // Equality operators compare GUIDs, origins, language code, and the contents
  // in the comparison. Usage metadata (use count, use date, modification date)
  // are NOT compared.
  bool operator==(const AutofillProfile& profile) const;

  // Tests that for every supported type of AutofillProfile, the values of
  // `this` and `profile` either agree or the value of `*this` is empty (meaning
  // that `this` is a subset of `profile`).
  // Note that a profile is considered a subset of itself.
  // Comparisons are done using the `comparator`.
  bool IsSubsetOf(const AutofillProfileComparator& comparator,
                  const AutofillProfile& profile) const;

  // Like `IsSubsetOf()`, but considers only the given `types`.
  bool IsSubsetOfForFieldSet(const AutofillProfileComparator& comparator,
                             const AutofillProfile& profile,
                             const FieldTypeSet& types) const;

  // Like `IsSubsetOf()`, but for strict superset instead of subset.
  bool IsStrictSupersetOf(const AutofillProfileComparator& comparator,
                          const AutofillProfile& profile) const;

  // Overwrites the data of |this| profile with data from the given |profile|.
  // Expects that the profiles have the same guid.
  void OverwriteDataFromForLegacySync(const AutofillProfile& profile);

  // Merges the data from |this| profile and the given |profile| into |this|
  // profile. Expects that |this| and |profile| have already been deemed
  // mergeable by an AutofillProfileComparator.
  bool MergeDataFrom(const AutofillProfile& profile,
                     const std::string& app_locale);

  // Creates a differentiating label for each of the |profiles|.
  // Labels consist of the minimal differentiating combination of:
  // 1. Full name.
  // 2. Address.
  // 3. E-mail.
  // 4. Phone.
  // 5. Company name.
  static void CreateDifferentiatingLabels(
      const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
          profiles,
      const std::string& app_locale,
      std::vector<std::u16string>* labels);

  // Creates inferred labels for `profiles`, according to the rules above and
  // stores them in `labels`. The inferred labels both provide a way to
  // identify a profile and also make sure to differentiate them if
  // necessary. Therefore this method first adds label information to allow
  // users to recognize a profile (like their full name) and a possible second
  // label if this leads to two profiles having the same label, for example if
  // there are two profiles with the same full name, it might add their email
  // data to differentiate them. In this context the `triggering_field_type` is
  // used to help deciding whether the differentiate label is needed. If the
  // profile value for `triggering_field_type` is unique (and therefore the
  // `Suggestion::main_text`), no differentiating label will be added. If
  // `suggested_fields` is not nullopt, the resulting label fields are drawn
  // from it minus those in `excluded_fields`. Otherwise, the label fields are
  // drawn from a default set. Each label includes at least
  // `minimal_fields_shown` fields, if possible.
  // TODO(crbug.com/40285811): Make `suggested_fields` non-optional.
  static void CreateInferredLabels(
      const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
          profiles,
      const std::optional<FieldTypeSet> suggested_fields,
      std::optional<FieldType> triggering_field_type,
      FieldTypeSet excluded_fields,
      size_t minimal_fields_shown,
      const std::string& app_locale,
      std::vector<std::u16string>* labels,
      bool use_improved_labels_order = false);

  // Builds inferred label from the first |num_fields_to_include| non-empty
  // fields in |label_fields|. Uses as many fields as possible if there are not
  // enough non-empty fields.
  std::u16string ConstructInferredLabel(
      base::span<const FieldType> label_fields,
      size_t num_fields_to_include,
      const std::string& app_locale) const;

  const std::string& language_code() const { return language_code_; }
  void set_language_code(const std::string& language_code) {
    language_code_ = language_code;
  }

  // Logs the number of days since the profile was last used and records its
  // use.
  // Also initiates the logging of the structured token verification statuses.
  void RecordAndLogUse();

  // Logs the verification status of non-empty structured name and address
  // tokens. Should be called when a profile is used to fill a form.
  void LogVerificationStatuses();

  // Calls |FinalizeAfterImport()| on all |FormGroup| members that are
  // implemented using the hybrid-structure |AddressComponent|.
  // If possible, this will initiate the completion of the structure tree to
  // derive all missing values either by parsing their parent node if assigned,
  // or by formatting the value from their child nodes.
  // Returns true if all calls yielded true.
  bool FinalizeAfterImport();

  // Returns true if the profile contains any structured data. This can be any
  // name type but the full name, or for addresses, the street name or house
  // number.
  bool HasStructuredData() const;

  // Returns a constant reference to the |name_| field.
  const NameInfo& GetNameInfo() const { return name_; }

  // Returns a constant reference to the |address_| field.
  const Address& GetAddress() const { return address_; }

  // Returns the profile country code.
  AddressCountryCode GetAddressCountryCode() const;

  // Returns the label of the profile.
  const std::string& profile_label() const { return profile_label_; }

  // Sets the label of the profile.
  void set_profile_label(const std::string& label) { profile_label_ = label; }

  RecordType record_type() const { return record_type_; }

  // Returns true if the profile is stored in the user's account. Non-account
  // profiles are considered local profiles.
  bool IsAccountProfile() const;

  int initial_creator_id() const { return initial_creator_id_; }
  void set_initial_creator_id(int creator_id) {
    initial_creator_id_ = creator_id;
  }

  int last_modifier_id() const { return last_modifier_id_; }
  void set_last_modifier_id(int modifier_id) {
    last_modifier_id_ = modifier_id;
  }

  // Converts a kLocalOrSyncable profile to a kAccount profile and returns it.
  // The converted profile shares the same content, but with a different GUID
  // and with `record_type` kAccount. Additional kAccount-specific metadata is
  // set.
  AutofillProfile ConvertToAccountProfile() const;

  // Converts a kAccount(Home|Work) address back to a regular kAccount address.
  // This is necessary to resolve inconsistencies between server and client, to
  // ensure that only a single H/W address can exist each.
  AutofillProfile DowngradeToAccountProfile() const;

  // Checks for non-empty setting-inaccessible fields and returns all that were
  // found.
  FieldTypeSet FindInaccessibleProfileValues() const;

  // Clears all specified |fields| from the profile.
  void ClearFields(const FieldTypeSet& fields);

  const ProfileTokenQuality& token_quality() const { return token_quality_; }
  ProfileTokenQuality& token_quality() { return token_quality_; }

  // Returns the type that should be used to fill a field given `field_type`.
  // It is possible that this type is not necessarily `field_type`, if it does
  // not yield a value for filling.
  // TODO(crbug.com/40264633): Pass and return a `FieldType` instead of
  // `AutofillType`.
  AutofillType GetFillingType(AutofillType field_type) const;

 private:
  friend class AutofillProfileTestApi;

  // FormGroup:
  std::u16string GetInfoImpl(const AutofillType& type,
                             const std::string& app_locale) const override;

  VerificationStatus GetVerificationStatusImpl(
      const FieldType type) const override;

  bool SetInfoWithVerificationStatusImpl(const AutofillType& type,
                                         const std::u16string& value,
                                         const std::string& app_locale,
                                         VerificationStatus status) override;

  // Creates inferred labels for |profiles| at indices corresponding to
  // |indices|, and stores the results to the corresponding elements of
  // |labels|. These labels include enough fields to differentiate among the
  // profiles, if possible; and also at least |num_fields_to_include| fields, if
  // possible. The label fields are drawn from |fields|.
  static void CreateInferredLabelsHelper(
      const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
          profiles,
      const std::list<size_t>& indices,
      const std::vector<FieldType>& field_types,
      size_t num_fields_to_include,
      const std::string& app_locale,
      std::vector<std::u16string>* labels);

  // Utilities for listing and lookup of the data members that constitute
  // user-visible profile information.
  std::array<const FormGroup*, 5> FormGroups() const {
    // Adjust the return type size as necessary.
    return {&name_, &email_, &company_, &phone_number_, &address_};
  }

  const FormGroup* FormGroupForType(FieldType type) const;
  FormGroup* MutableFormGroupForType(FieldType type);

  // Same as operator==, but ignores differences in GUID.
  bool EqualsSansGuid(const AutofillProfile& profile) const;

  // Merging two AutofillProfiles is done by merging their `FormGroups()`. While
  // doing so, the `token_quality_` needs to be merged too. This function is
  // responsible for carrying over or resetting the token quality of all
  // supported types of the `merged_group`.
  // `merged_group` represents the merged form group of `*this` with the same
  // form group of `other_profile`.
  // By calling this function, `token_quality_` is updated to match the
  // information represented by the `merged_group`.
  void MergeFormGroupTokenQuality(const FormGroup& merged_group,
                                  const AutofillProfile& other_profile);

  // A globally unique ID for this object. It identifies the profile across
  // browser restarts and is used as the primary key in the database.
  // The `guid_` is unique across profile record types.
  std::string guid_;

  // Personal information for this profile.
  NameInfo name_;
  EmailInfo email_;
  CompanyInfo company_;
  PhoneNumber phone_number_;
  Address address_;

  // A label intended to be chosen by the user. This was however never
  // implemented and is currently unused.
  std::string profile_label_;

  // The BCP 47 language code that can be used to format |address_| for display.
  std::string language_code_;

  RecordType record_type_;

  // Indicates the application that initially created the profile and the
  // application that performed the last non-metadata modification of it.
  // Only relevant for `record_type_ == kAccount` profiles, since
  // `kLocalOrSyncable` profiles are only used within Autofill. The integer
  // values represent a server-side enum `BillableService`, which is not
  // duplicated in Chromium. For Autofill, the exact application that
  // created/modified the profile is thus opaque. However, Autofill is
  // represented by the value `kInitialCreatorOrModifierChrome`.
  int initial_creator_id_ = 0;
  int last_modifier_id_ = 0;

  // Stores information about the quality of this profile's stored types.
  // Only used when `kAutofillTrackProfileTokenQuality` is enabled.
  // TODO(crbug.com/40271999): Clean-up comment.
  ProfileTokenQuality token_quality_;
};

// So we can compare AutofillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillProfile& profile);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_PROFILE_H_
