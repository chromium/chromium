// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_IMPORT_PROCESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_IMPORT_PROCESS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "url/origin.h"

namespace autofill {

class AddressDataManager;

// Specifies the type of a profile form import.
enum class AutofillProfileImportType {
  // Type is unspecified.
  kImportTypeUnspecified,
  // The observed profile corresponds to a new profile because there are no
  // mergeable or updatable profiles.
  kNewProfile,
  // The imported profile is a subset of an already existing profile.
  kDuplicateImport,
  // The imported profile can be integrated into an already existing profile
  // without any changes to settings-visible values.
  kSilentUpdate,
  // The imported profile changes settings-visible values which is only imported
  // after explicit user confirmation.
  kConfirmableMerge,
  // The observed profile corresponds to a new profile because there are no
  // mergeable or updatable profiles but imports are suppressed for this
  // domain.
  kSuppressedNewProfile,
  // The observed profile resulted both in a confirmable merge and in a silent
  // update.
  kConfirmableMergeAndSilentUpdate,
  // The observed profile resulted in one or more confirmable merges that are
  // all suppressed with no additional silent updates.
  kSuppressedConfirmableMerge,
  // The observed profile resulted in one or more suppressed confirmable merges
  // but with additional silent updates.
  kSuppressedConfirmableMergeAndSilentUpdate,
  // Indicates that a silent update was the result of importing an incomplete
  // profile.
  kSilentUpdateForIncompleteProfile,
  // Indicates that even though the incomplete profile contained structured
  // information, it could not be used for a silent update.
  kUnusableIncompleteProfile,
  // The observed profile corresponds to an existing kLocalOrSyncable profile,
  // which can be migrated to the account profile storage.
  kProfileMigration,
  // Like `kProfileMigration`, but additionally the migration candidate and
  // other stored profiles can be silently updated. These silent updates happen
  // even if the user declines the migration.
  kProfileMigrationAndSilentUpdate,
  kMaxValue = kProfileMigrationAndSilentUpdate
};

// Specifies the status of the imported phone number.
enum class PhoneImportStatus {
  // Phone number is not present. Default.
  kNone,
  // User imported the phone number as it was.
  kValid,
  // The phone number was removed from the profile import as it was invalid.
  kInvalid,
  kMaxValue = kInvalid
};

// Metadata about the import, which is passed through from FormDataImporter to
// ProfileImportProcess. This is required to do metric collection, depending on
// the user's decision to (not) import, based on how we construct the candidate
// profile in FormDataImporter.
// Besides metrics, it is also required to avoid creating obvious quasi
// duplicates after autofilling a profile.
struct ProfileImportMetadata {
  ProfileImportMetadata();
  ProfileImportMetadata(const ProfileImportMetadata&);
  ~ProfileImportMetadata();

  // Tracks if the form section contains an invalid country.
  bool observed_invalid_country = false;
  // Whether the profile's country was complemented automatically.
  bool did_complement_country = false;
  // Whether the form originally contained a phone number and if that phone
  // number is considered valid by libphonenumber.
  PhoneImportStatus phone_import_status = PhoneImportStatus::kNone;
  // Whether the profile import from any field that contained an unrecognized
  // autocomplete attribute.
  bool did_import_from_unrecognized_autocomplete_field = false;
  // The origin that the form was submitted on.
  url::Origin origin;
  // Contains an entry for every type observed in the form, which was used to
  // construct the candidate profile. The value indicates GUID of the profile
  // that was used to autofill the corresponding field - or nullopt, if the
  // field was not autofilled with address data at submission.
  base::flat_map<FieldType, std::optional<std::string>>
      filled_types_to_autofill_guid;
};

// This class holds the state associated with the import of an AutofillProfile
// observed in a form submission and should be used as the follows:
//
// * An instance is created by supplying the observed profile, all already
//   existing profiles and the used locale.
//
// * Now, the import process awaits either a user decision or a
//   confirmation that the user wasn't prompted at all. This confirmation is
//   supplied by either calling `AcceptWithoutPrompt()`, `AcceptWithoutEdits()`,
//   `AcceptWithEdits()`, `Declined()` or `Ignore()`.
//
// * Finally, `ImportAffectedProfiles()` should be used to update the
//   profiles in the `AddressDataManager`.
//
// The instance of this class should contain all information needed to record
// metrics once an import process is finished.
class ProfileImportProcess {
 public:
  ProfileImportProcess(const AutofillProfile& observed_profile,
                       const std::string& app_locale,
                       const GURL& form_source_url,
                       AddressDataManager* address_data_manager,
                       bool allow_only_silent_updates,
                       ProfileImportMetadata import_metadata = {});

  ProfileImportProcess(const ProfileImportProcess&);
  ProfileImportProcess& operator=(const ProfileImportProcess& other);

  ~ProfileImportProcess();

  // Returns true if showing the prompt was initiated for this import process.
  bool prompt_shown() const;

  const std::optional<AutofillProfile>& import_candidate() const {
    return import_candidate_;
  }

  const std::optional<AutofillProfile>& confirmed_import_candidate() const {
    return confirmed_import_candidate_;
  }

  const std::optional<AutofillProfile>& merge_candidate() const {
    return merge_candidate_;
  }

  const std::vector<AutofillProfile>& silently_updated_profiles() const {
    return silently_updated_profiles_;
  }

  const AutofillProfile& observed_profile() const { return observed_profile_; }

  AutofillProfileImportType import_type() const { return import_type_; }

  bool is_confirmable_update() const {
    return import_type_ == AutofillProfileImportType::kConfirmableMerge ||
           import_type_ ==
               AutofillProfileImportType::kConfirmableMergeAndSilentUpdate;
  }

  bool is_migration() const {
    return import_type_ == AutofillProfileImportType::kProfileMigration ||
           import_type_ ==
               AutofillProfileImportType::kProfileMigrationAndSilentUpdate;
  }

  const ProfileImportMetadata& import_metadata() const {
    return import_metadata_;
  }

  AutofillClient::AddressPromptUserDecision user_decision() const {
    return user_decision_;
  }

  // Returns true if the user actively declined the save of update without
  // differentiating between the actual type of decline.
  // If no decision is available yet, return false.
  bool UserDeclined() const;

  // Returns true if the user actively accepted the save of update without
  // differentiating if there have been additional edits by the user.
  // If no decision is available yet, return false.
  bool UserAccepted() const;

  const GURL& form_source_url() const { return form_source_url_; }

  // Adds and updates all profiles affected by the import process in the
  // `address_data_manager_`. The affected profiles correspond to the
  // `silently_updated_profiles_` and depending on the import type, the
  // `confirmed_import_candidate_`.
  void ApplyImport();

  // Returns false if the import does not result in any change to the stored
  // profiles. This function can only be evaluated after a decision was
  // supplied. Note that this function allows for a false positive if a user
  // accepts a merge, but edits the profile back to its initial state.
  bool ProfilesChanged() const;

  // No prompt is shown to the user.
  void AcceptWithoutPrompt();

  // The import is accepted by the user without additional edits.
  void AcceptWithoutEdits();

  // The import is accepted but only with additional edits contained in
  // `edited_profile`.
  void AcceptWithEdits(AutofillProfile edited_profile);

  // The import was declined.
  void Declined();

  // The prompt was ignored.
  void Ignore();

  // Set the prompt as being shown.
  void set_prompt_was_shown();

  // Supply a user |decision| for the import process. The option
  // |edited_profile| reflect user edits to the import candidate.
  void SetUserDecision(
      AutofillClient::AddressPromptUserDecision decision,
      base::optional_ref<const AutofillProfile> edited_profile = std::nullopt);

  // Records UMA and UKM metrics after the import was applied. Should only be
  // called after a user decision was supplied or a silent update happens.
  // `existing_profiles` are the profiles before the import was applied.
  void CollectMetrics(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      const std::vector<const AutofillProfile*>& existing_profiles) const;

 private:
  // Determines the import type of |observed_profile_| with respect to
  // |existing_profiles|. Only the first profile in |existing_profiles| becomes
  // a merge candidate in case there is a confirmable merge.
  void DetermineProfileImportType();

  // For new profile imports, sets the source of the `import_candidate_`
  // correctly, depending on the user's account storage eligiblity.
  void DetermineSourceOfImportCandidate();

  // Determines whether the values of the `observed_profile_` were autofilled
  // with exactly one profile, except for an edit in a low-quality token. In
  // this case, the autofilled profile qualifies for an update (rather than a
  // new profile import).
  // If the above situation applies, returns true and sets the import and merge
  // candidates to offer updating the low quality token.
  bool IsObservedProfileAutofilledQuasiDuplicate(
      const AutofillProfileComparator& comparator);

  // If the observed profile is a duplicate (modulo silent updates) of an
  // existing `kLocalOrSyncable` profile, eligible users are prompted to change
  // its storage location to `kAccount`.
  // This function checks whether the `profile` qualifies for migration and sets
  // the `migration_candidate` accordingly. The conditions are:
  // - `migration_candidate` not set yet.
  // - The User eligible for account profile storage.
  // - `profile` is of source `kLocalOrSyncable` and not blocked for migration.
  // - The `profile`'s country isn't set to an unsupported country.
  // - Not only silent updates are allowed.
  void MaybeSetMigrationCandidate(
      std::optional<AutofillProfile>& migration_candidate,
      const AutofillProfile& profile) const;

  // Computes the settings-visible profile difference between the
  // `import_candidate_` and the `confirmed_import_candidate_`. Logs all edited
  // types, depending on the import type. Returns the number of edited fields.
  // If the user didn't edit any fields (or wasn't prompted), this is a no-op.
  int CollectedEditedTypeHistograms() const;

  // Indicates if the user is already prompted.
  bool prompt_shown_{false};

  // The profile as it has been observed on form submission.
  AutofillProfile observed_profile_;

  // Profiles that are silently updatable with the observed profile.
  std::vector<AutofillProfile> silently_updated_profiles_;

  // A profile in its original state that can be merged with the observed
  // profile.
  std::optional<AutofillProfile> merge_candidate_;

  // The import candidate that is presented to the user. In case of a migration,
  // this is an existing profile.
  std::optional<AutofillProfile> import_candidate_;

  // The type of the import indicates if the profile is just a duplicate of an
  // existing profile, if an existing profile can be silently updated, or if
  // the user must be prompted either because a merge would alter stored values,
  // or because the profile is completely new.
  AutofillProfileImportType import_type_{
      AutofillProfileImportType::kImportTypeUnspecified};

  // The profile as it was confirmed by the user or as it should be imported if
  // user interactions are disabled.
  std::optional<AutofillProfile> confirmed_import_candidate_;

  // The decision the user made when prompted.
  AutofillClient::AddressPromptUserDecision user_decision_{
      AutofillClient::AddressPromptUserDecision::kUndefined};

  // The application locale used for this import process.
  std::string app_locale_;

  // The url of the form.
  GURL form_source_url_;

  // Indicates if saving a new profile is blocked for the domain the profile
  // was observed on.
  bool new_profiles_suppressed_for_domain_;

  // A reference to the address data manager that is used to retrieve additional
  // information about existing profiles and save/update imported profiles.
  raw_ref<AddressDataManager> address_data_manager_;

  // Counts the number of blocked profile updates.
  int number_of_blocked_profile_updates_{0};

  // If true, denotes that the import process allows only silent updates.
  bool allow_only_silent_updates_;

  // Metadata about the import, used for metric collection after the user's
  // decision.
  ProfileImportMetadata import_metadata_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_IMPORT_PROCESS_H_
