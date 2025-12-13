// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/addresses/autofill_profile_import_process.h"

#include <algorithm>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/addresses/home_and_work_metadata_store.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_requirement_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

using UserDecision = AutofillClient::AddressPromptUserDecision;

// When the profile is observed without explicit country information, Autofill
// guesses it's country. Detecting a profile as a duplicate can fail if we guess
// incorrectly. This function checks if we have reason to believe that the
// country of `profile` was guessed incorrectly. It does so by checking whether
// any of the `existing_profiles` becomes mergeable after removing the country
// of `profile`.
// Comparisons are done using `comparator`. Note that for two countries to be
// mergeable, they must share the same address model.
bool ShouldCountryApproximationBeRemoved(
    const AutofillProfile& profile,
    const std::vector<const AutofillProfile*>& existing_profiles,
    const AutofillProfileComparator& comparator) {
  auto IsMergeableWithExistingProfiles = [&](const AutofillProfile& profile) {
    return std::ranges::any_of(existing_profiles, [&](auto* existing_profile) {
      return comparator.AreMergeable(profile, *existing_profile);
    });
  };
  if (IsMergeableWithExistingProfiles(profile)) {
    return false;
  }
  AutofillProfile without_country = profile;
  without_country.ClearFields({ADDRESS_HOME_COUNTRY});
  return IsMergeableWithExistingProfiles(without_country);
}

// Checks if `unedited_autofilled_profile_guids` set is populated in a way that
// allows for creating a profile that is a superset of both the
// `kAccountNameEmail` profile and one of the Home/Work profiles. This flow
// works only if there are 2 profiles in the set. One of them must be of type
// `kAccountNameEmail` and the remaining one must be either `kAccountHome` or
// `kAccountWork`.
bool CanCombineAccountNameEmailWithHomeWork(
    const ProfileImportMetadata& import_metadata,
    const AddressDataManager& address_data_manager) {
  if (import_metadata.unedited_autofilled_profile_guids.size() != 2) {
    return false;
  }

  bool account_name_email_was_filled = false;
  bool home_or_work_was_filled = false;
  for (const auto& guid : import_metadata.unedited_autofilled_profile_guids) {
    const AutofillProfile* profile =
        address_data_manager.GetProfileByGUID(guid);
    if (profile) {
      switch (profile->record_type()) {
        case AutofillProfile::RecordType::kAccountNameEmail:
          account_name_email_was_filled = true;
          break;
        case AutofillProfile::RecordType::kAccountHome:
        case AutofillProfile::RecordType::kAccountWork:
          home_or_work_was_filled = true;
          break;
        case AutofillProfile::RecordType::kAccount:
        case AutofillProfile::RecordType::kLocalOrSyncable:
          // These profile types cannot take part in this flow, hence return
          // false.
          return false;
      }
    }
  }
  return account_name_email_was_filled && home_or_work_was_filled;
}

}  // namespace

ProfileImportMetadata::ProfileImportMetadata() = default;
ProfileImportMetadata::ProfileImportMetadata(const ProfileImportMetadata&) =
    default;
ProfileImportMetadata::~ProfileImportMetadata() = default;

ProfileImportProcess::ProfileImportProcess(
    const AutofillProfile& observed_profile,
    const std::string& app_locale,
    const GURL& form_source_url,
    ukm::SourceId ukm_source_id,
    AddressDataManager* address_data_manager,
    bool allow_only_silent_updates,
    ProfileImportMetadata import_metadata)
    : observed_profile_(observed_profile),
      app_locale_(app_locale),
      form_source_url_(form_source_url),
      ukm_source_id_(ukm_source_id),
      address_data_manager_(CHECK_DEREF(address_data_manager)),
      allow_only_silent_updates_(allow_only_silent_updates),
      import_metadata_(import_metadata) {
  DetermineProfileImportType();
  DetermineSourceOfImportCandidate();
}

ProfileImportProcess::ProfileImportProcess(const ProfileImportProcess&) =
    default;

ProfileImportProcess& ProfileImportProcess::operator=(
    const ProfileImportProcess& other) = default;

ProfileImportProcess::~ProfileImportProcess() = default;

bool ProfileImportProcess::prompt_shown() const {
  return prompt_shown_;
}

bool ProfileImportProcess::UserDeclined() const {
  return user_decision_ == UserDecision::kDeclined ||
         user_decision_ == UserDecision::kEditDeclined ||
         user_decision_ == UserDecision::kMessageDeclined ||
         user_decision_ == UserDecision::kNever;
}

bool ProfileImportProcess::UserAccepted() const {
  return user_decision_ == UserDecision::kAccepted ||
         user_decision_ == UserDecision::kEditAccepted;
}

void ProfileImportProcess::DetermineProfileImportType() {
  AutofillProfileComparator comparator(app_locale_);
  // If there is reason to believe that the `observed_profile_`'s country was
  // complemented incorrectly, remove the country.
  if (import_metadata_.did_complement_country &&
      ShouldCountryApproximationBeRemoved(observed_profile_,
                                          address_data_manager_->GetProfiles(),
                                          comparator)) {
    observed_profile_.ClearFields({ADDRESS_HOME_COUNTRY});
    import_metadata_.did_complement_country = false;
  }

  // Existing profiles that are not mergeable with the `observed_profile_`
  // cannot be altered by this import. If none remain, no update prompts can be
  // shown and the import corresponds to a new profile.
  std::vector<const AutofillProfile*> mergeable_profiles =
      address_data_manager_->GetProfiles(
          AddressDataManager::ProfileOrder::kMostRecentlyUsedFirstDesc);
  std::erase_if(mergeable_profiles, [&](const AutofillProfile* p) {
    return !comparator.AreMergeable(*p, observed_profile_);
  });
  if (mergeable_profiles.empty()) {
    DetermineNewProfileImportType();
    return;
  }

  // The `observed_profile_` is mergeable with some existing profiles. Create
  // import candidates by merging `observed_profile_` into `mergeable_profiles`.
  // Note that `mergeable_profiles`'s frecency ordering is retained.
  std::vector<ImportCandidate> candidates =
      GetImportCandidates(mergeable_profiles);

  // Attempt to show an update prompt (prioritized over migrations).
  // Assume the user autofilled the submitted form. In this case, Chrome
  // shouldn't show an update prompt, even if a profile exists for which
  // `QualifiesForUpdateProfilePrompt()` is true. It would be surprising to see
  // an update prompt for data that was just autofilled and is therefore already
  // known to Autofill.
  // In general, if any mergeable profile exists with a change type other than
  // `kSettingVisibleChange`, the submitted data already exists in Autofill and
  // the prompt should be suppressed. With the next milestone release, the
  // deduplication logic will remove any profiles that would have caused the
  // update prompt without special handling.
  auto update_candidate_it = std::ranges::find_if(
      candidates, [&](auto& c) { return QualifiesForUpdateProfilePrompt(c); });
  if (update_candidate_it != candidates.end() &&
      std::ranges::all_of(candidates, [](auto& c) {
        return c.change == ImportCandidate::Change::kSettingVisibleChange;
      })) {
    DetermineUpdateProfileImportType(*update_candidate_it);
    return;
  }

  // Collect all silently updatable profiles. Due to the condition about setting
  // visibility for update prompts, silent updates are only possible if no
  // update prompt is shown.
  for (const ImportCandidate& candidate : candidates) {
    if (QualifiesForSilentUpdate(candidate)) {
      silently_updated_profiles_.push_back(candidate.merged_profile);
    }
  }

  // Attempt to show a migration prompt.
  auto migrate_candidate_it = std::ranges::find_if(
      candidates, [&](auto& c) { return QualifiesForMigrateProfilePrompt(c); });
  if (migrate_candidate_it != candidates.end()) {
    DetermineMigrateProfileImportType(migrate_candidate_it->merged_profile);
    return;
  }

  // Neither an update nor a migration prompt could be shown.
  DetermineSuppressedImportType(candidates);
}

void ProfileImportProcess::DetermineNewProfileImportType() {
  if (allow_only_silent_updates_ ||
      address_data_manager_->IsNewProfileImportBlockedForDomain(
          form_source_url_)) {
    import_type_ = AutofillProfileImportType::kSuppressedNewProfile;
    return;
  }
  import_candidate_ = observed_profile();
  import_type_ = AutofillProfileImportType::kNewProfile;
}

void ProfileImportProcess::DetermineSuppressedImportType(
    base::span<const ImportCandidate> candidates) {
  CHECK(!candidates.empty());
  // Even though the `observed_profile_` is mergeable with some existing
  // profiles, none of the `candidates` qualified for an update or a migration
  // prompt. From a user's perspective, nothing will happen. For metrics,
  // break down further why the import was suppressed.
  if (std::ranges::any_of(candidates, [](auto& c) {
        return c.change == ImportCandidate::Change::kSettingVisibleChange;
      })) {
    // At least one of the `candidates` changed in a setting-visibile way. The
    // fact that `DetermineSuppressedImportType()` was called means that an
    // update prompt was suppressed, for example because of the strike database.
    import_type_ = silently_updated_profiles_.empty()
                       ? AutofillProfileImportType::kSuppressedConfirmableMerge
                       : AutofillProfileImportType::
                             kSuppressedConfirmableMergeAndSilentUpdate;
  } else {
    // Either all updates could be applied silently or a migration prompt was
    // suppressed. No distinction is made for metrics.
    import_type_ = silently_updated_profiles_.empty()
                       ? AutofillProfileImportType::kDuplicateImport
                       : AutofillProfileImportType::kSilentUpdate;
  }
}

void ProfileImportProcess::DetermineUpdateProfileImportType(
    const ImportCandidate& update_candidate) {
  import_candidate_ = update_candidate.merged_profile;
  // By setting the `merge_candidate_`, an update prompt will be shown that
  // displays the diff between the `import_candidate_` and the
  // `merge_candidate_`. In some cases, this intentionally doesn't happen so
  // that the new profile UI is triggered instead. This is done when the
  // underlying profile that should get updated is read-only (e.g. the
  // kAccountNameEmail profile). To the user, the flow feels like an update.
  switch (update_candidate.existing_profile.record_type()) {
    case AutofillProfile::RecordType::kAccountHome:
    case AutofillProfile::RecordType::kAccountWork:
      if (CanCombineAccountNameEmailWithHomeWork(import_metadata(),
                                                 *address_data_manager_)) {
        import_type_ = AutofillProfileImportType::kHomeWorkNameEmailMerge;
      } else {
        import_type_ = AutofillProfileImportType::kHomeAndWorkSuperset;
        merge_candidate_ = update_candidate.existing_profile;
      }
      break;
    case AutofillProfile::RecordType::kAccountNameEmail:
      import_type_ = CanCombineAccountNameEmailWithHomeWork(
                         import_metadata(), *address_data_manager_)
                         ? AutofillProfileImportType::kHomeWorkNameEmailMerge
                         : AutofillProfileImportType::kNameEmailSuperset;
      break;
    case AutofillProfile::RecordType::kAccount:
    case AutofillProfile::RecordType::kLocalOrSyncable:
      import_type_ =
          silently_updated_profiles_.empty()
              ? AutofillProfileImportType::kConfirmableMerge
              : AutofillProfileImportType::kConfirmableMergeAndSilentUpdate;
      merge_candidate_ = update_candidate.existing_profile;
      break;
  }
}

void ProfileImportProcess::DetermineMigrateProfileImportType(
    const AutofillProfile& migration_candidate) {
  import_candidate_ = migration_candidate;
  import_type_ =
      silently_updated_profiles_.empty()
          ? AutofillProfileImportType::kProfileMigration
          : AutofillProfileImportType::kProfileMigrationAndSilentUpdate;
}

bool ProfileImportProcess::QualifiesForSilentUpdate(
    const ImportCandidate& candidate) const {
  return candidate.change ==
             ImportCandidate::Change::kNonSettingVisibleChange &&
         !base::FeatureList::IsEnabled(
             features::debug::kAutofillDisableSilentProfileUpdates);
}

bool ProfileImportProcess::QualifiesForUpdateProfilePrompt(
    const ImportCandidate& candidate) const {
  return candidate.change == ImportCandidate::Change::kSettingVisibleChange &&
         !allow_only_silent_updates_ &&
         !address_data_manager_->IsProfileUpdateBlocked(
             candidate.existing_profile.guid()) &&
         !base::FeatureList::IsEnabled(
             features::debug::kAutofillDisableProfileUpdates);
}

bool ProfileImportProcess::QualifiesForMigrateProfilePrompt(
    const ImportCandidate& candidate) const {
  return candidate.change != ImportCandidate::Change::kSettingVisibleChange &&
         !allow_only_silent_updates_ &&
         IsEligibleForMigrationToAccount(*address_data_manager_,
                                         candidate.existing_profile);
}

std::vector<ProfileImportProcess::ImportCandidate>
ProfileImportProcess::GetImportCandidates(
    base::span<const AutofillProfile*> mergeable_profiles) const {
  std::vector<ImportCandidate> result;
  result.reserve(mergeable_profiles.size());
  for (const AutofillProfile* merge_candidate : mergeable_profiles) {
    AutofillProfile merged_profile = *merge_candidate;
    const bool was_profile_altered =
        merged_profile.MergeDataFrom(observed_profile_, app_locale_);
    ImportCandidate::Change change_type = [&] {
      if (!was_profile_altered) {
        return ImportCandidate::Change::kNoChange;
      } else if (AutofillProfileComparator::
                     ProfilesHaveDifferentSettingsVisibleValues(
                         *merge_candidate, merged_profile, app_locale_)) {
        return ImportCandidate::Change::kSettingVisibleChange;
      } else {
        return ImportCandidate::Change::kNonSettingVisibleChange;
      }
    }();
    result.push_back(
        ImportCandidate{.change = change_type,
                        .existing_profile = *merge_candidate,
                        .merged_profile = std::move(merged_profile)});
  }
  return result;
}

void ProfileImportProcess::DetermineSourceOfImportCandidate() {
  // The flow signified by `kNameEmailSuperset` leads to the save profile
  // prompt. Since the `kAccountNameEmail` profile is available both to users
  // eligible and ineligible for account address storage, `import_candidate_`
  // (which has record type `kAccountNameEmail` in this case) must be converted
  // to either `kAccount` (eligible) or `kLocalOrSyncable` (ineligible).
  if (import_type_ == AutofillProfileImportType::kNameEmailSuperset) {
    CHECK(import_candidate_);
    import_candidate_ =
        address_data_manager_->IsEligibleForAddressAccountStorage()
            ? import_candidate_->ConvertToAccountProfile()
            : import_candidate_->ConvertToLocalOrSyncableProfile();
    return;
  }
  // kHomeAndWorkSuperset prompts use the "Update profile" UI, but store a new
  // profile under the hood, since Home & Work is read-only. This makes sure
  // that the profile created is an account profile, since Home & Work is only
  // available for users eligible to account address storage.
  //
  // Merging the `kAccountNameEmail` profile with a H/W profile also results in
  // creation of a new profile. Since, H/W is available only for users eligible
  // for account address storage, the profile created as a result of this flow
  // should also be of type `kAccount`.
  if (import_type_ != AutofillProfileImportType::kNewProfile &&
      import_type_ != AutofillProfileImportType::kHomeAndWorkSuperset &&
      import_type_ != AutofillProfileImportType::kHomeWorkNameEmailMerge) {
    return;
  }
  CHECK(import_candidate_);
  if (address_data_manager_->IsEligibleForAddressAccountStorage()) {
    import_candidate_ = import_candidate_->ConvertToAccountProfile();
  }
}

bool ProfileImportProcess::requires_user_prompt() const {
  switch (import_type_) {
    case AutofillProfileImportType::kNewProfile:
    case AutofillProfileImportType::kConfirmableMerge:
    case AutofillProfileImportType::kConfirmableMergeAndSilentUpdate:
    case AutofillProfileImportType::kProfileMigration:
    case AutofillProfileImportType::kProfileMigrationAndSilentUpdate:
    case AutofillProfileImportType::kHomeAndWorkSuperset:
    case AutofillProfileImportType::kNameEmailSuperset:
    case AutofillProfileImportType::kHomeWorkNameEmailMerge:
      return true;
    case AutofillProfileImportType::kDuplicateImport:
    case AutofillProfileImportType::kSilentUpdate:
    case AutofillProfileImportType::kSuppressedNewProfile:
    case AutofillProfileImportType::kSuppressedConfirmableMergeAndSilentUpdate:
    case AutofillProfileImportType::kSuppressedConfirmableMerge:
      return false;
    case AutofillProfileImportType::kImportTypeUnspecified:
      NOTREACHED();
  }
}

void ProfileImportProcess::ApplyImport() {
  // At this point, a user decision must have been supplied.
  DCHECK_NE(user_decision_, UserDecision::kUndefined);
  if (!ProfilesChanged()) {
    return;
  }

  // Apply silent updates.
  HomeAndWorkMetadataStore* home_and_work_metadata_store =
      address_data_manager_->home_and_work_metadata_store();
  for (const AutofillProfile& updated_profile : silently_updated_profiles_) {
    address_data_manager_->UpdateProfile(updated_profile);
    if (home_and_work_metadata_store) {
      home_and_work_metadata_store->RecordSilentUpdate(updated_profile);
    }
  }

  if (!confirmed_import_candidate_.has_value()) {
    return;
  }
  // In case a new profile is created, make sure the modification date is
  // updated correctly. Note that for update profile cases (such as the silent
  // updates above), this is not necessary, since the AddressDataManager already
  // takes care of it.
  confirmed_import_candidate_->usage_history().set_modification_date(
      base::Time::Now());
  // Handle bubble-showing autofill profile import types.
  switch (import_type()) {
    case AutofillProfileImportType::kNewProfile:
      address_data_manager_->AddProfile(*confirmed_import_candidate_);
      break;
    case AutofillProfileImportType::kConfirmableMerge:
    case AutofillProfileImportType::kConfirmableMergeAndSilentUpdate:
      address_data_manager_->UpdateProfile(*confirmed_import_candidate_);
      break;
    case AutofillProfileImportType::kProfileMigration:
    case AutofillProfileImportType::kProfileMigrationAndSilentUpdate:
      address_data_manager_->MigrateProfileToAccount(
          *confirmed_import_candidate_);
      break;
    case AutofillProfileImportType::kHomeAndWorkSuperset:
      address_data_manager_->AddProfile(*confirmed_import_candidate_);
      // Remove the original H/W profile since a superset was just saved.
      CHECK(merge_candidate_->IsHomeAndWorkProfile());
      address_data_manager_->RemoveProfile(merge_candidate_->guid());
      break;
    case AutofillProfileImportType::kNameEmailSuperset: {
      address_data_manager_->AddProfile(*confirmed_import_candidate_);
      // Remove the original `kAccountNameEmail` profile since a superset was
      // just saved.
      const std::vector<const AutofillProfile*> account_name_email_profiles =
          address_data_manager_->GetProfilesByRecordType(
              AutofillProfile::RecordType::kAccountNameEmail);

      if (account_name_email_profiles.size() == 1) {
        address_data_manager_->RemoveProfile(
            account_name_email_profiles[0]->guid());
      }
    } break;
    case AutofillProfileImportType::kHomeWorkNameEmailMerge:
      address_data_manager_->AddProfile(*confirmed_import_candidate_);
      CHECK_EQ(import_metadata_.unedited_autofilled_profile_guids.size(), 2u);
      // Remove both original `kAccountNameEmail` and
      // `kAccountHome`/`kAccountWork` profiles since a superset of them was
      // just saved.
      for (const std::string& guid :
           import_metadata_.unedited_autofilled_profile_guids) {
        address_data_manager_->RemoveProfile(guid);
      }
      break;

    // Those import types do not cause save/update/migrate/merge bubble to be
    // displayed.
    case AutofillProfileImportType::kDuplicateImport:
    case AutofillProfileImportType::kSilentUpdate:
    case AutofillProfileImportType::kSuppressedNewProfile:
    case AutofillProfileImportType::kSuppressedConfirmableMergeAndSilentUpdate:
    case AutofillProfileImportType::kSuppressedConfirmableMerge:
    case AutofillProfileImportType::kImportTypeUnspecified:
      NOTREACHED();
  }
}

void ProfileImportProcess::SetUserDecision(
    UserDecision decision,
    base::optional_ref<const AutofillProfile> edited_profile) {
  // A user decision should only be supplied once.
  DCHECK_EQ(user_decision_, UserDecision::kUndefined);
  DCHECK(!confirmed_import_candidate_.has_value());

  user_decision_ = decision;
  switch (user_decision_) {
    // If the import was accepted either with or without a prompt, the import
    // candidate gets confirmed.
    case UserDecision::kUserNotAsked:
    case UserDecision::kAccepted:
      confirmed_import_candidate_ = import_candidate_;
      break;

    case UserDecision::kEditAccepted:
      // If the import candidate is supplied, the 'edited_profile' must be
      // supplied.
      DCHECK(edited_profile.has_value());
      confirmed_import_candidate_ = edited_profile.value();

      // Make sure the verification status of all settings-visible non-empty
      // fields in the edited profile are set to kUserVerified.
      for (auto type : edited_profile->GetUserVisibleTypes()) {
        std::u16string value = confirmed_import_candidate_->GetRawInfo(type);
        if (!value.empty() &&
            confirmed_import_candidate_->GetVerificationStatus(type) ==
                VerificationStatus::kNoStatus) {
          confirmed_import_candidate_->SetRawInfoWithVerificationStatus(
              type, value, VerificationStatus::kUserVerified);
        }
      }

      confirmed_import_candidate_->FinalizeAfterImport();
      // The `confirmed_import_candidate_` has to have the same `guid` as the
      // original import candidate.
      DCHECK_EQ(import_candidate_->guid(), confirmed_import_candidate_->guid());
      break;

    // If the confirmable merge was declided or ignored, the original merge
    // candidate should be maintined. Note that the decline/ignore/never does
    // not mean that silent updates are not performed.
    case UserDecision::kDeclined:
    case UserDecision::kEditDeclined:
    case UserDecision::kMessageDeclined:
    case UserDecision::kMessageTimeout:
    case UserDecision::kIgnored:
    case UserDecision::kAutoDeclined:
    case UserDecision::kNever:
      confirmed_import_candidate_ = merge_candidate_;
      break;

    case UserDecision::kUndefined:
      NOTREACHED();
  }
}

void ProfileImportProcess::AcceptWithoutPrompt() {
  SetUserDecision(UserDecision::kUserNotAsked);
}

void ProfileImportProcess::AcceptWithoutEdits() {
  SetUserDecision(UserDecision::kAccepted);
}

void ProfileImportProcess::AcceptWithEdits(AutofillProfile edited_profile) {
  SetUserDecision(UserDecision::kEditAccepted,
                  std::make_optional(edited_profile));
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

  if (!silently_updated_profiles_.empty()) {
    return true;
  }

  // If there is no confirmed import candidate there are no changes.
  if (!confirmed_import_candidate_.has_value()) {
    return false;
  }

  // If the import was accepted, return true.
  if (user_decision_ == UserDecision::kAccepted ||
      user_decision_ == UserDecision::kEditAccepted ||
      user_decision_ == UserDecision::kUserNotAsked) {
    return true;
  }

  return false;
}

void ProfileImportProcess::set_prompt_was_shown() {
  prompt_shown_ = true;
}

void ProfileImportProcess::CollectMetrics(
    ukm::UkmRecorder* ukm_recorder,
    const std::vector<const AutofillProfile*>& existing_profiles) const {
  // Metrics should only be recorded after a user decision was supplied.
  DCHECK_NE(user_decision_, UserDecision::kUndefined);

  if (allow_only_silent_updates_) {
    // Record the import type for the silent updates.
    autofill_metrics::LogSilentUpdatesProfileImportType(import_type_);
    if (import_type_ == AutofillProfileImportType::kSilentUpdate) {
      LogUkmMetrics(ukm_recorder, existing_profiles);
    }
    return;
  }

  // For any finished import process record the type of the import.
  autofill_metrics::LogProfileImportType(import_type_);

  // Tracks number of edited fields by the user in the storage prompt. Zero if
  // the user didn't edit any fields.
  int num_edited_fields = CollectedEditedTypeHistograms();

  // For an import process that involves prompting the user, record the
  // decision.
  if (import_type_ == AutofillProfileImportType::kNewProfile) {
    LogNewProfileMetrics(existing_profiles);
    LogUkmMetrics(ukm_recorder, existing_profiles, num_edited_fields);
  } else if (import_type_ ==
             AutofillProfileImportType::kHomeWorkNameEmailMerge) {
    autofill_metrics::LogHomeWorkNameEmailMergeImportDecision(user_decision_);
  } else if (import_type_ == AutofillProfileImportType::kHomeAndWorkSuperset) {
    LogHomeAndWorkSupersetMetrics();
  } else if (import_type_ == AutofillProfileImportType::kNameEmailSuperset) {
    autofill_metrics::LogNameEmailSupersetImportDecision(user_decision_);
  } else if (is_confirmable_update()) {
    LogConfirmableProfileUpdateMetrics(existing_profiles);
    LogUkmMetrics(ukm_recorder, existing_profiles, num_edited_fields);
  } else if (import_type_ == AutofillProfileImportType::kSilentUpdate) {
    LogUkmMetrics(ukm_recorder, existing_profiles);
  } else if (is_migration()) {
    autofill_metrics::LogProfileMigrationImportDecision(user_decision_);
    LogUkmMetrics(ukm_recorder, existing_profiles, num_edited_fields);
  }
}

int ProfileImportProcess::CollectedEditedTypeHistograms() const {
  if (user_decision_ != UserDecision::kEditAccepted) {
    return 0;
  }
  // Compute the number of edited settings-visible fields.
  std::vector<ProfileValueDifference> edit_difference =
      AutofillProfileComparator::GetSettingsVisibleProfileDifference(
          *import_candidate_, *confirmed_import_candidate_, app_locale_);
  // Log edited types.
  for (const ProfileValueDifference& difference : edit_difference) {
    autofill_metrics::LogProfileImportTypeEditedType(import_type(),
                                                     difference.type);
  }

  return edit_difference.size();
}

void ProfileImportProcess::LogUkmMetrics(
    ukm::UkmRecorder* ukm_recorder,
    const std::vector<const AutofillProfile*>& existing_profiles,
    int num_edited_fields) const {
  autofill_metrics::LogAddressProfileImportUkm(
      ukm_recorder, ukm_source_id_, import_type_, user_decision_,
      import_metadata_, num_edited_fields,
      UserAccepted() ? confirmed_import_candidate_ : import_candidate_,
      existing_profiles, app_locale_);
}

void ProfileImportProcess::LogNewProfileMetrics(
    const std::vector<const AutofillProfile*>& existing_profiles) const {
  autofill_metrics::LogNewProfileImportDecision(
      user_decision_, import_metadata_, existing_profiles,
      UserAccepted() ? *confirmed_import_candidate_ : *import_candidate_,
      app_locale_);
  if (UserAccepted()) {
    autofill_metrics::LogNewProfileStorageLocation(
        *confirmed_import_candidate_);
  }
}

void ProfileImportProcess::LogConfirmableProfileUpdateMetrics(
    const std::vector<const AutofillProfile*>& existing_profiles) const {
  autofill_metrics::LogProfileUpdateImportDecision(
      user_decision_, existing_profiles,
      UserAccepted() ? *confirmed_import_candidate_ : *import_candidate_,
      app_locale_);

  DCHECK(merge_candidate_.has_value() && import_candidate_.has_value());
  // For all update prompts, log the field types and total number of fields
  // that would change due to the update. Note that this does not include
  // additional manual edits the user can perform in the storage dialog.
  // Those are covered separately below.
  const std::vector<ProfileValueDifference> merge_difference =
      AutofillProfileComparator::GetSettingsVisibleProfileDifference(
          import_candidate_.value(), merge_candidate_.value(), app_locale_);

  for (const auto& difference : merge_difference) {
    autofill_metrics::LogProfileUpdateAffectedType(difference.type,
                                                   user_decision_);
  }
  autofill_metrics::LogUpdateProfileNumberOfAffectedFields(
      merge_difference.size(), user_decision_);
}

void ProfileImportProcess::LogHomeAndWorkSupersetMetrics() const {
  autofill_metrics::LogHomeAndWorkSupersetImportDecision(user_decision_);
  CHECK(merge_candidate_.has_value() && import_candidate_.has_value());
  // Log the types that triggered the prompt.
  for (const ProfileValueDifference& difference :
       AutofillProfileComparator::GetSettingsVisibleProfileDifference(
           import_candidate_.value(), merge_candidate_.value(), app_locale_)) {
    autofill_metrics::LogHomeAndWorkSupersetAffectedType(difference.type);
  }
}

}  // namespace autofill
