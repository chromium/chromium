// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_IMPORT_PROCESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_IMPORT_PROCESS_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/id_type.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace autofill {

// The id of an ongoing profile import process.
using AutofillProfileImportId =
    ::base::IdTypeU64<class AutofillProfileImportIdMarker>;

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
  kMaxValue = kUnusableIncompleteProfile
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
struct ProfileImportMetadata {
  // Whether the profile's country was complemented automatically.
  bool did_complement_country = false;
  // Whether the form original contained an invalid country that was ignored
  // due to AutofillOverwriteInvalidCountryOnImport.
  // TODO(crbug.com/1362472): Cleanup when launched.
  bool did_ignore_invalid_country = false;
  // Whether the form originally contained a phone number and if that phone
  // number is considered valid by libphonenumber.
  PhoneImportStatus phone_import_status = PhoneImportStatus::kNone;
  // Whether the profile import from any field that contained an unrecognized
  // autocomplete attribute.
  bool did_import_from_unrecognized_autocomplete_field = false;
  // The origin that the form was submitted on.
  url::Origin origin;
  // The number of fields with unrecognized autocomplete attribute that used to
  // construct the observed profile.
  // TODO(crbug.com/1301721): Remove.
  int num_autocomplete_unrecognized_fields = 0;
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
// * Finally, `GetResultingProfiles()` should be used to get the complete set of
//   resulting AutofillProfiles.
//
// The instance of this class should contain all information needed to record
// metrics once an import process is finished.
class ProfileImportProcess {
 public:
  ProfileImportProcess(const AutofillProfile& observed_profile,
                       const std::string& app_locale,
                       const GURL& form_source_url,
                       const PersonalDataManager* personal_data_manager,
                       bool allow_only_silent_updates,
                       ProfileImportMetadata import_metadata = {});

  ProfileImportProcess(const ProfileImportProcess&);
  ProfileImportProcess& operator=(const ProfileImportProcess& other);

  ~ProfileImportProcess();

  // Returns true if showing the prompt was initiated for this import process.
  bool prompt_shown() const;

  const absl::optional<AutofillProfile>& import_candidate() const {
    return import_candidate_;
  }

  const absl::optional<AutofillProfile>& confirmed_import_candidate() const {
    return confirmed_import_candidate_;
  }

  const absl::optional<AutofillProfile>& merge_candidate() const {
    return merge_candidate_;
  }

  const std::vector<AutofillProfile>& updated_profiles() const {
    return updated_profiles_;
  }

  const AutofillProfileImportId& import_id() const { return import_id_; }

  const AutofillProfile& observed_profile() const { return observed_profile_; }

  AutofillProfileImportType import_type() const { return import_type_; }

  const ProfileImportMetadata& import_metadata() const {
    return import_metadata_;
  }

  AutofillClient::SaveAddressProfileOfferUserDecision user_decision() const {
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

  // Returns a vector containing all unchanged, updated, merged and new
  // profiles.
  std::vector<AutofillProfile> GetResultingProfiles();

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
      AutofillClient::SaveAddressProfileOfferUserDecision decision,
      absl::optional<AutofillProfile> edited_profile = absl::nullopt);

  // Records UMA and UKM metrics. Should only be called after a user decision
  // was supplied or a silent update happens.
  void CollectMetrics(ukm::UkmRecorder* ukm_recorder,
                      ukm::SourceId source_id) const;

 private:
  // Determines the import type of |observed_profile_| with respect to
  // |existing_profiles|. Only the first profile in |existing_profiles| becomes
  // a merge candidate in case there is a confirmable merge.
  void DetermineProfileImportType();

  // An id to identify an import request.
  AutofillProfileImportId import_id_;

  // Indicates if the user is already prompted.
  bool prompt_shown_{false};

  // The profile as it has been observed on form submission.
  AutofillProfile observed_profile_;

  // Profiles that are silently updatable with the observed profile.
  std::vector<AutofillProfile> updated_profiles_;

  // A profile in its original state that can be merged with the observed
  // profile.
  absl::optional<AutofillProfile> merge_candidate_;

  // The import candidate that is presented to the user.
  absl::optional<AutofillProfile> import_candidate_;

  // The type of the import indicates if the profile is just a duplicate of an
  // existing profile, if an existing profile can be silently updated, or if
  // the user must be prompted either because a merge would alter stored values,
  // or because the profile is completely new.
  AutofillProfileImportType import_type_{
      AutofillProfileImportType::kImportTypeUnspecified};

  // The profile as it was confirmed by the user or as it should be imported if
  // user interactions are disabled.
  absl::optional<AutofillProfile> confirmed_import_candidate_;

  // The decision the user made when prompted.
  AutofillClient::SaveAddressProfileOfferUserDecision user_decision_{
      AutofillClient::SaveAddressProfileOfferUserDecision::kUndefined};

  // The application locale used for this import process.
  std::string app_locale_;

  // The url of the form.
  GURL form_source_url_;

  // Indicates if saving a new profile is blocked for the domain the profile
  // was observed on.
  bool new_profiles_suppressed_for_domain_;

  // A pointer to the persona data manager that is used to retrieve additional
  // information about existing profiles.
  raw_ptr<const PersonalDataManager> personal_data_manager_;

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
