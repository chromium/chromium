// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_import_process.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"

namespace autofill {

namespace {

using UserDecision = AutofillClient::SaveAddressProfileOfferUserDecision;

// Returns a unique import id.
AutofillProfileImportId GetImportId() {
  static AutofillProfileImportId next_import_id(0);
  next_import_id.value()++;
  return next_import_id;
}

}  // namespace

ProfileImportProcess::ProfileImportProcess(
    const AutofillProfile& observed_profile,
    const std::vector<AutofillProfile*>& existing_profiles,
    const std::string& app_locale,
    const GURL& form_source_url,
    bool new_profiles_suppressed_for_domain)
    : import_id_(GetImportId()),
      observed_profile_(observed_profile),
      app_locale_(app_locale),
      form_source_url_(form_source_url),
      new_profiles_suppressed_for_domain_(new_profiles_suppressed_for_domain) {
  DetermineProfileImportType(existing_profiles, app_locale);
}

ProfileImportProcess::ProfileImportProcess(const ProfileImportProcess&) =
    default;

ProfileImportProcess& ProfileImportProcess::operator=(
    const ProfileImportProcess& other) = default;

ProfileImportProcess::~ProfileImportProcess() = default;

bool ProfileImportProcess::prompt_shown() const {
  return prompt_shown_;
}

bool ProfileImportProcess::ImportIsNewProfile() const {
  return import_type_ == AutofillProfileImportType::kNewProfile;
}

bool ProfileImportProcess::ImportIsSilentUpdate() const {
  return import_type_ == AutofillProfileImportType::kSilentUpdate;
}

bool ProfileImportProcess::ImportIsMerge() const {
  return import_type_ == AutofillProfileImportType::kConfirmableMerge;
}

void ProfileImportProcess::DetermineProfileImportType(
    const std::vector<AutofillProfile*>& existing_profiles,
    const std::string& app_locale) {
  AutofillProfileComparator comparator(app_locale);
  bool is_mergeable_with_existing_profile = false;

  for (const auto* existing_profile : existing_profiles) {
    // If the existing profile is not mergeable with the observed profile, the
    // existing profile is not altered by this import.
    if (!comparator.AreMergeable(*existing_profile, observed_profile_)) {
      unchanged_profiles_.emplace_back(*existing_profile);
      continue;
    }

    // The observed profile is mergeable with an existing profile.
    // This information is used to determine if the observed profile classifies
    // as an import of a new profile or the import of a duplicate profile.
    is_mergeable_with_existing_profile = true;

    // Make a copy of the existing profile and merge it with the observation.
    // The return value of |MergeDataFrom()| indicates if the existing profile
    // was changed at all during that merge.
    AutofillProfile merged_profile = *existing_profile;
    if (!merged_profile.MergeDataFrom(observed_profile_, app_locale)) {
      unchanged_profiles_.emplace_back(*existing_profile);
      continue;
    }

    // At this point, the observed profile was merged with the existing profile
    // which changed in some way.
    // Now, determine if the merge alters any settings-visible value, or if the
    // merge can  be considered as a silent update that does not need to get
    // user confirmation.
    if (AutofillProfileComparator::ProfilesHaveDifferentSettingsVisibleValues(
            *existing_profile, merged_profile)) {
      // If a settings-visible value changed, the existing profile is the merge
      // candidate if no other merge candidate has already been found.
      if (!merge_candidate_.has_value()) {
        merge_candidate_ = *existing_profile;
        import_candidate_ = merged_profile;
        import_type_ = AutofillProfileImportType::kConfirmableMerge;
      } else {
        // If there is already a merge candidate, the existing profile is not
        // supposed to be changed.
        unchanged_profiles_.emplace_back(*existing_profile);
      }
      continue;
    }
    // If the profile changed but all settings-visible values are maintained,
    // the profile can be updated silently.
    updated_profiles_.emplace_back(merged_profile);
  }

  // If the profile is not mergeable with an existing profile, the import
  // corresponds to a new profile.
  if (!is_mergeable_with_existing_profile) {
    // There should be no import candidate yet.
    DCHECK(!import_candidate_.has_value());
    if (new_profiles_suppressed_for_domain_) {
      import_type_ = AutofillProfileImportType::kSuppressedNewProfile;
    } else {
      import_type_ = AutofillProfileImportType::kNewProfile;
      import_candidate_ = observed_profile();
    }
  } else if (!merge_candidate_.has_value()) {
    // When the observed profile is mergeable with an existing profile but there
    // is no merge candidate this means that either the import was a duplicate
    // of an existing profile or that there are only silent updates.
    import_type_ = updated_profiles_.size() > 0
                       ? AutofillProfileImportType::kSilentUpdate
                       : AutofillProfileImportType::kDuplicateImport;
  }

  // At this point, all existing profiles are either unchanged, updated and/or
  // one is the merge candidate.
  DCHECK_EQ(existing_profiles.size(),
            unchanged_profiles_.size() + updated_profiles_.size() +
                (merge_candidate_.has_value() ? 1 : 0));
  DCHECK_NE(import_type_, AutofillProfileImportType::kImportTypeUnspecified);
}

std::vector<AutofillProfile> ProfileImportProcess::GetResultingProfiles() {
  std::vector<AutofillProfile> resulting_profiles;

  // At this point, a user decision must have been supplied.
  DCHECK_NE(user_decision_, UserDecision::kUndefined);

  // The unchanged and updated profiles should be added unconditionally.
  resulting_profiles.insert(resulting_profiles.end(),
                            unchanged_profiles_.begin(),
                            unchanged_profiles_.end());
  resulting_profiles.insert(resulting_profiles.end(), updated_profiles_.begin(),
                            updated_profiles_.end());

  // If there is a confirmed import candidate, add it.
  if (confirmed_import_candidate_.has_value()) {
    resulting_profiles.emplace_back(confirmed_import_candidate_.value());
  }

  return resulting_profiles;
}

void ProfileImportProcess::SetUserDecision(
    UserDecision decision,
    absl::optional<AutofillProfile> edited_profile) {
  // A user decision should only be supplied once.
  DCHECK_EQ(user_decision_, UserDecision::kUndefined);
  DCHECK(!confirmed_import_candidate_.has_value());

  user_decision_ = decision;
  switch (user_decision_) {
    // If the import was accepted either with or without a prompt, the import
    // candidate gets confired.
    case UserDecision::kUserNotAsked:
    case UserDecision::kAccepted:
      confirmed_import_candidate_ = import_candidate_;
      break;

    case UserDecision::kEdited:
      // If the import candidate is supplied, the 'edited_profile' must be
      // supplied.
      DCHECK(edited_profile.has_value());
      // The `edited_profile` has to have the same `guid` as the original import
      // candidate.
      DCHECK_EQ(import_candidate_.value().guid(), edited_profile->guid());
      confirmed_import_candidate_ = std::move(edited_profile);
      break;

    // If the confirmable merge was declided or ignored, the original merge
    // candidate should be maintined. Note that the decline/ignore does not mean
    // that silent updates are not performed.
    case UserDecision::kDeclined:
    case UserDecision::kIgnored:
      confirmed_import_candidate_ = merge_candidate_;
      break;

    case UserDecision::kNever:
      confirmed_import_candidate_ = merge_candidate_;
      break;

    case UserDecision::kUndefined:
      NOTREACHED();
      break;
  }
}

void ProfileImportProcess::AcceptWithoutPrompt() {
  SetUserDecision(UserDecision::kUserNotAsked);
}

void ProfileImportProcess::AcceptWithoutEdits() {
  SetUserDecision(UserDecision::kAccepted);
}

void ProfileImportProcess::AcceptWithEdits(AutofillProfile edited_profile) {
  SetUserDecision(UserDecision::kEdited, absl::make_optional(edited_profile));
}

void ProfileImportProcess::Declined() {
  SetUserDecision(UserDecision::kDeclined);
}

void ProfileImportProcess::Ignore() {
  SetUserDecision(UserDecision::kIgnored);
}

bool ProfileImportProcess::ProfilesChanged() const {
  // At this point, a user decision must have been supplied.
  DCHECK_NE(user_decision_, UserDecision::kUndefined);

  // If there are any updated profiles, return true.
  if (updated_profiles_.size() > 0) {
    return true;
  }

  // If there is no confirmed import candidate there are no changes.
  if (!confirmed_import_candidate_.has_value()) {
    return false;
  }

  // If the import was accepted, return true.
  if (user_decision_ == UserDecision::kAccepted ||
      user_decision_ == UserDecision::kEdited ||
      user_decision_ == UserDecision::kUserNotAsked) {
    return true;
  }

  return false;
}

void ProfileImportProcess::set_prompt_was_shown() {
  prompt_shown_ = true;
}

void ProfileImportProcess::CollectMetrics() const {
  // Metrics should only be recorded after a user decision was supplied.
  DCHECK_NE(user_decision_, UserDecision::kUndefined);

  // For any finished import process record the type of the import.
  AutofillMetrics::LogProfileImportType(import_type_);

  // For an import process that involves prompting the user, record the
  // decision.
  if (ImportIsNewProfile()) {
    AutofillMetrics::LogNewProfileImportDecision(user_decision_);
  } else if (ImportIsMerge()) {
    AutofillMetrics::LogProfileUpdateImportDecision(user_decision_);
  }

  // If the profile was edited by the user, record a histogram of edited types.
  if (user_decision_ == UserDecision::kEdited) {
    for (const auto& difference :
         AutofillProfileComparator::GetSettingsVisibleProfileDifference(
             import_candidate_.value(), confirmed_import_candidate_.value(),
             app_locale_)) {
      if (ImportIsNewProfile()) {
        AutofillMetrics::LogNewProfileEditedType(difference.type);
      } else {
        AutofillMetrics::LogProfileUpdateEditedType(difference.type);
      }
    }
  }
}

}  // namespace autofill
