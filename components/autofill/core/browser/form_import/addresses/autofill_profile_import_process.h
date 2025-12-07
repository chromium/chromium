// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_AUTOFILL_PROFILE_IMPORT_PROCESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_AUTOFILL_PROFILE_IMPORT_PROCESS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

namespace autofill {

class AddressDataManager;

// Specifies the type of a profile form import. The type is used for logging but
// also for deciding which UI to show.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutofillProfileImportType {
  // Type is unspecified.
  kImportTypeUnspecified = 0,
  // The observed profile corresponds to a new profile because there are no
  // mergeable or updatable profiles.
  kNewProfile = 1,
  // The imported profile is a subset of an already existing profile.
  kDuplicateImport = 2,
  // The imported profile can be integrated into an already existing profile
  // without any changes to settings-visible values.
  kSilentUpdate = 3,
  // The imported profile changes settings-visible values which is only imported
  // after explicit user confirmation.
  kConfirmableMerge = 4,
  // The observed profile corresponds to a new profile, but either imports are
  // suppressed for this domain or only silent updates are allowed.
  kSuppressedNewProfile = 5,
  // The observed profile resulted both in a confirmable merge and in a silent
  // update.
  kConfirmableMergeAndSilentUpdate = 6,
  // The observed profile resulted in one or more confirmable merges that are
  // all suppressed with no additional silent updates.
  kSuppressedConfirmableMerge = 7,
  // The observed profile resulted in one or more suppressed confirmable merges
  // but with additional silent updates.
  kSuppressedConfirmableMergeAndSilentUpdate = 8,
  // Only updates on complete profiles are performed.
  // kSilentUpdateForIncompleteProfile = 9,
  // kUnusableIncompleteProfile = 10,
  // The observed profile corresponds to an existing kLocalOrSyncable profile,
  // which can be migrated to the account profile storage.
  kProfileMigration = 11,
  // Like `kProfileMigration`, but additionally the migration candidate and
  // other stored profiles can be silently updated. These silent updates happen
  // even if the user declines the migration.
  kProfileMigrationAndSilentUpdate = 12,
  // A superset of a Home and Work address was submitted and no other non-Home
  // and Work profile qualified for an update. This will trigger an update
  // prompt, but accepting this prompt creates a new H/W superset profile
  // under the hood since H/W is read-only.
  kHomeAndWorkSuperset = 13,
  // A profile that is a superset of an existing `kAccountNameEmail` profile was
  // submitted. This triggers a prompt to save the submitted profile as a new,
  // more complete profile.
  kNameEmailSuperset = 14,
  // Import of a profile that is a superset of both a H/W profile and the
  // `kAccountNameEmail` profile, when form fields were not edited after
  // filling.
  kHomeWorkNameEmailMerge = 15,
  kMaxValue = kHomeWorkNameEmailMerge
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
  // GUIDs of `AutofillProfile`s that were used to fill the form. Empty if the
  // user edited any of the filled fields in the form.
  base::flat_set<std::string> unedited_autofilled_profile_guids;
  // Tracks if the submitted form contained non-empty split zip fields.
  bool observed_split_zip = false;
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
                       ukm::SourceId ukm_source_id,
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

  // Returns true if the import process requires a user prompt.
  bool requires_user_prompt() const;

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
      const std::vector<const AutofillProfile*>& existing_profiles) const;

 private:
  // Represents an existing profile and how it changes when merged with the
  // `observed_profile_`.
  struct ImportCandidate {
    // Describes how `existing_profile` and `merged_profile` differ.
    enum class Change {
      kNoChange = 0,
      kNonSettingVisibleChange = 1,
      kSettingVisibleChange = 2,
    } change;
    // An existing profile, as saved in Autofill prior to form submission.
    AutofillProfile existing_profile;
    // The `existing_profile`, merged with `observed_profile_`.
    AutofillProfile merged_profile;
  };

  // Determines the import type of `observed_profile_` with respect to
  // `existing_profiles` and updates `merge_candidate_`, `import_candidate_`
  // and/or `silently_updated_profiles_`.
  // Only one profile can be updated in a user-visible way at a time. Updates
  // are preferred over migrations and higher frecency profiles are preferred
  // over lower frecency ones.
  void DetermineProfileImportType();

  // Helper functions for `DetermineProfileImportType()` that set the
  // appropriate `import_type_` once the logic has determined that a certain
  // flow should happen (new profile import, suppressing an import, ...).
  // Also sets the `import_candidate_` and `merge_candidate_`, if necessary.
  void DetermineNewProfileImportType();
  void DetermineSuppressedImportType(
      base::span<const ImportCandidate> candidates);
  void DetermineUpdateProfileImportType(
      const ImportCandidate& update_candidate);
  void DetermineMigrateProfileImportType(
      const AutofillProfile& migration_candidate);

  // Predicates to classify whether a `candidates` qualifies for a certain flow.
  // This checks for conditions like address completeness or strikes from
  // repeatedly rejecting prompts.
  bool QualifiesForSilentUpdate(const ImportCandidate& candidate) const;
  bool QualifiesForUpdateProfilePrompt(const ImportCandidate& candidate) const;
  bool QualifiesForMigrateProfilePrompt(const ImportCandidate& profile) const;

  // Merges the `mergeable_profiles` with the `observed_profile_` and determines
  // how the merged profile differs from the original one (see
  // `ImportCandidate::Change`). Constructs one `ImportCandidate` per
  // `mergeable_profiles` and returns the result, preserving relative order.
  // Assumes that mergeability has already been determined.
  std::vector<ImportCandidate> GetImportCandidates(
      base::span<const AutofillProfile*> mergeable_profiles) const;

  // For new profile imports, sets the source of the `import_candidate_`
  // correctly, depending on the user's account storage eligiblity.
  void DetermineSourceOfImportCandidate();

  // Computes the settings-visible profile difference between the
  // `import_candidate_` and the `confirmed_import_candidate_`. Logs all edited
  // types, depending on the import type. Returns the number of edited fields.
  // If the user didn't edit any fields (or wasn't prompted), this is a no-op.
  int CollectedEditedTypeHistograms() const;

  // Records UKM metrics after the import was applied.
  void LogUkmMetrics(
      ukm::UkmRecorder* ukm_recorder,
      const std::vector<const AutofillProfile*>& existing_profiles,
      int num_edited_fields = 0) const;

  // Records new profile import metrics after the import was applied.
  void LogNewProfileMetrics(
      const std::vector<const AutofillProfile*>& existing_profiles) const;

  // Records confirmable profile update metrics after the import was applied.
  void LogConfirmableProfileUpdateMetrics(
      const std::vector<const AutofillProfile*>& existing_profiles) const;

  // Records Home and Work superset metrics after the import was applied.
  void LogHomeAndWorkSupersetMetrics() const;

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

  // The UKM source ID of the page whose form is imported.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // A reference to the address data manager that is used to retrieve additional
  // information about existing profiles and save/update imported profiles.
  raw_ref<AddressDataManager> address_data_manager_;

  // If true, denotes that the import process allows only silent updates.
  bool allow_only_silent_updates_;

  // Metadata about the import, used for metric collection after the user's
  // decision.
  ProfileImportMetadata import_metadata_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_AUTOFILL_PROFILE_IMPORT_PROCESS_H_
