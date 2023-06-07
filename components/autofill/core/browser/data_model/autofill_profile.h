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
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/address.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/birthdate.h"
#include "components/autofill/core/browser/data_model/contact_info.h"
#include "components/autofill/core/browser/data_model/phone_number.h"

namespace autofill {

class AutofillProfileComparator;

struct AutofillMetadata;

// A collection of FormGroups stored in a profile.  AutofillProfile also
// implements the FormGroup interface so that owners of this object can request
// form information from the profile, and the profile will delegate the request
// to the requested form group type.
class AutofillProfile : public AutofillDataModel {
 public:
  // `RecordType` is deprecated and `SERVER_PROFILE` essentially unused.
  // TODO(crbug.com/1177366): Remove
  enum RecordType {
    // A profile stored and editable locally.
    LOCAL_PROFILE,
    // A profile synced down from the server. These are read-only locally.
    SERVER_PROFILE,
  };

  // Describes where the profile is stored and how it is synced.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.autofill
  enum class Source {
    // Not synced at all or synced through the `AutofillProfileSyncBridge`. This
    // corresponds to profiles that local to Autofill only.
    kLocalOrSyncable = 0,
    // Synced through the `ContactInfoSyncBridge`. This corresponds to profiles
    // that are shared beyond Autofill across different services.
    kAccount = 1,
    kMaxValue = kAccount,
  };

  // The values used to represent Autofill in the `initial_creator_id()` and
  // `last_modifier_id()`.
  static constexpr int kInitialCreatorOrModifierChrome = 70073;

  AutofillProfile();
  explicit AutofillProfile(const std::string& guid,
                           Source source = Source::kLocalOrSyncable);
  explicit AutofillProfile(Source source);

  // Server profile constructor. The type must be SERVER_PROFILE (this serves
  // to differentiate this constructor). |server_id| can be empty. If empty,
  // callers should invoke GenerateServerProfileIdentifier after setting data.
  AutofillProfile(RecordType type, const std::string& server_id);

  AutofillProfile(const AutofillProfile& profile);
  ~AutofillProfile() override;

  AutofillProfile& operator=(const AutofillProfile& profile);

  // AutofillDataModel:
  AutofillMetadata GetMetadata() const override;
  double GetRankingScore(base::Time current_time) const override;
  bool SetMetadata(const AutofillMetadata& metadata) override;

  // FormGroup:
  void GetMatchingTypes(const std::u16string& text,
                        const std::string& app_locale,
                        ServerFieldTypeSet* matching_types) const override;

  std::u16string GetRawInfo(ServerFieldType type) const override;

  int GetRawInfoAsInt(ServerFieldType type) const override;

  void SetRawInfoWithVerificationStatus(ServerFieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;

  void SetRawInfoAsIntWithVerificationStatus(
      ServerFieldType type,
      int value,
      VerificationStatus status) override;

  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;

  // How this card is stored.
  RecordType record_type() const { return record_type_; }
  void set_record_type(RecordType type) { record_type_ = type; }

  // Returns true if there are no values (field types) set.
  bool IsEmpty(const std::string& app_locale) const;

  // Returns true if the |type| of data in this profile is present, but invalid.
  // Otherwise returns false.
  bool IsPresentButInvalid(ServerFieldType type) const;

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
  bool EqualsForSyncPurposes(const AutofillProfile& profile) const;

  // Returns true if |new_profile| and this are considered equal for updating
  // purposes, meaning that if equal we do not need to update this profile to
  // the |new_profile|.
  bool EqualsForUpdatePurposes(const AutofillProfile& new_profile) const;

  // Same as operator==, but cares about differences in usage stats.
  bool EqualsIncludingUsageStatsForTesting(
      const AutofillProfile& profile) const;

  // Equality operators compare GUIDs, origins, language code, and the contents
  // in the comparison. Usage metadata (use count, use date, modification date)
  // are NOT compared.
  bool operator==(const AutofillProfile& profile) const;
  virtual bool operator!=(const AutofillProfile& profile) const;

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
                             const ServerFieldTypeSet& types) const;

  // Like `IsSubsetOf()`, but for strict superset instead of subset.
  bool IsStrictSupersetOf(const AutofillProfileComparator& comparator,
                          const AutofillProfile& profile) const;

  // Overwrites the data of |this| profile with data from the given |profile|.
  // Expects that the profiles have the same guid.
  void OverwriteDataFrom(const AutofillProfile& profile);

  // Merges the data from |this| profile and the given |profile| into |this|
  // profile. Expects that |this| and |profile| have already been deemed
  // mergeable by an AutofillProfileComparator.
  bool MergeDataFrom(const AutofillProfile& profile,
                     const std::string& app_locale);

  // Saves info from |profile| into |this|, provided |this| and |profile| do not
  // have any direct conflicts (i.e. data is present but different).
  // Returns true if |this| and |profile| are similar.
  bool SaveAdditionalInfo(const AutofillProfile& profile,
                          const std::string& app_locale);

  // Creates a differentiating label for each of the |profiles|.
  // Labels consist of the minimal differentiating combination of:
  // 1. Full name.
  // 2. Address.
  // 3. E-mail.
  // 4. Phone.
  // 5. Company name.
  static void CreateDifferentiatingLabels(
      const std::vector<AutofillProfile*>& profiles,
      const std::string& app_locale,
      std::vector<std::u16string>* labels);

  // Creates inferred labels for |profiles|, according to the rules above and
  // stores them in |created_labels|. If |suggested_fields| is not NULL, the
  // resulting label fields are drawn from |suggested_fields|, except excluding
  // |excluded_field|. Otherwise, the label fields are drawn from a default set,
  // and |excluded_field| is ignored; by convention, it should be of
  // |UNKNOWN_TYPE| when |suggested_fields| is NULL. Each label includes at
  // least |minimal_fields_shown| fields, if possible.
  static void CreateInferredLabels(
      const std::vector<AutofillProfile*>& profiles,
      const std::vector<ServerFieldType>* suggested_fields,
      ServerFieldType excluded_field,
      size_t minimal_fields_shown,
      const std::string& app_locale,
      std::vector<std::u16string>* labels);

  // Builds inferred label from the first |num_fields_to_include| non-empty
  // fields in |label_fields|. Uses as many fields as possible if there are not
  // enough non-empty fields.
  std::u16string ConstructInferredLabel(const ServerFieldType* label_fields,
                                        const size_t label_fields_size,
                                        size_t num_fields_to_include,
                                        const std::string& app_locale) const;

  const std::string& language_code() const { return language_code_; }
  void set_language_code(const std::string& language_code) {
    language_code_ = language_code;
  }

  // Nonempty only when type() == SERVER_PROFILE. base::kSHA1Length bytes long.
  // Not necessarily valid UTF-8.
  const std::string& server_id() const { return server_id_; }

  // Creates an identifier and saves it as |server_id_|. Only used for
  // server credit cards. The server doesn't attach an identifier so Chrome
  // creates its own. The ID is a hash of the data contained in the profile.
  void GenerateServerProfileIdentifier();

  // Logs the number of days since the profile was last used, records its
  // use and updates |previous_use_date_| to the last value of |use_date_|.
  // Also initiates the logging of the structured token verification statuses.
  void RecordAndLogUse();

  // Logs the verification status of non-empty structured name and address
  // tokens. Should be called when a profile is used to fill a form.
  void LogVerificationStatuses();

  const base::Time& previous_use_date() const { return previous_use_date_; }
  void set_previous_use_date(const base::Time& time) {
    previous_use_date_ = time;
  }

  // Valid only when |record_type()| == |SERVER_PROFILE|.
  bool has_converted() const { return has_converted_; }
  void set_has_converted(bool has_converted) { has_converted_ = has_converted; }

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
  bool HasStructuredData();

  // Returns a constant reference to the |name_| field.
  const NameInfo& GetNameInfo() const { return name_; }

  // Returns a constant reference to the |address_| field.
  const Address& GetAddress() const { return address_; }

  // Returns the label of the profile.
  const std::string& profile_label() const { return profile_label_; }

  // Sets the label of the profile.
  void set_profile_label(const std::string& label) { profile_label_ = label; }

  Source source() const { return source_; }
  void set_source_for_testing(AutofillProfile::Source source) {
    source_ = source;
  }

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
  // and with `source_` kAccount. Additional kAccount-specific metadata is set.
  AutofillProfile ConvertToAccountProfile() const;

  // Checks for non-empty setting-inaccessible fields and returns all that were
  // found.
  ServerFieldTypeSet FindInaccessibleProfileValues() const;

  // Clears all specified |fields| from the profile.
  void ClearFields(const ServerFieldTypeSet& fields);

 private:
  // FormGroup:
  std::u16string GetInfoImpl(const AutofillType& type,
                             const std::string& app_locale) const override;

  VerificationStatus GetVerificationStatusImpl(
      const ServerFieldType type) const override;

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
      const std::vector<AutofillProfile*>& profiles,
      const std::list<size_t>& indices,
      const std::vector<ServerFieldType>& fields,
      size_t num_fields_to_include,
      const std::string& app_locale,
      std::vector<std::u16string>* labels);

  // Utilities for listing and lookup of the data members that constitute
  // user-visible profile information.
  std::array<const FormGroup*, 6> FormGroups() const {
    // Adjust the return type size as necessary.
    return {&name_, &email_, &company_, &phone_number_, &address_, &birthdate_};
  }

  const FormGroup* FormGroupForType(const AutofillType& type) const;
  FormGroup* MutableFormGroupForType(const AutofillType& type);

  // Same as operator==, but ignores differences in GUID.
  bool EqualsSansGuid(const AutofillProfile& profile) const;

  // Personal information for this profile.
  NameInfo name_;
  EmailInfo email_;
  CompanyInfo company_;
  PhoneNumber phone_number_;
  Address address_;
  Birthdate birthdate_;

  // The label is chosen by the user and can contain an arbitrary value.
  // However, there are two labels that play a special role to indicate that an
  // address is either a 'HOME' or a 'WORK' address. In this case, the value of
  // the label is '$HOME$' or '$WORK$', respectively.
  std::string profile_label_;

  // The BCP 47 language code that can be used to format |address_| for display.
  std::string language_code_;

  // ID used for identifying this profile. Only set for SERVER_PROFILEs. This is
  // a hash of the contents.
  std::string server_id_;

  // Penultimate time model was used, not persisted to database.
  base::Time previous_use_date_;

  RecordType record_type_;

  // Only useful for SERVER_PROFILEs. Whether this server profile has been
  // converted to a local profile.
  bool has_converted_;

  Source source_;

  // Indicates the application that initially created the profile and the
  // application that performed the last non-metadata modification of it.
  // Only relevant for `source_ == kAccount` profiles, since `kLocalOrSyncable`
  // profiles are only used within Autofill.
  // The integer values represent a server-side enum `BillableService`, which is
  // not duplicated in Chromium. For Autofill, the exact application that
  // created/modified the profile is thus opaque. However, Autofill is
  // represented by the value `kInitialCreatorOrModifierChrome`.
  int initial_creator_id_ = 0;
  int last_modifier_id_ = 0;
};

// So we can compare AutofillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillProfile& profile);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_PROFILE_H_
