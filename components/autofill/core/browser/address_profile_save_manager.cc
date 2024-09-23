// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_profile_save_manager.h"

#include "base/check_deref.h"
#include "base/types/optional_util.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

// Sufficiently complete profiles are added to the `form_data_importer`s
// multi-step import candidates. This enables complementing them in later steps
// with additional optional information.
// This function adds the imported profile as a candidate. This is only done
// after the user decision to incorporate manual edits.
void AddMultiStepComplementCandidate(FormDataImporter* form_data_importer,
                                     const AutofillProfile& profile,
                                     const url::Origin& origin) {
  if (!form_data_importer) {
    return;
  }
  // Metrics depending on `import_process.import_metadata()` are collected
  // for the `confirmed_import_candidate`. E.g. whether the removal of an
  // invalid phone number made the import possible. Just like regular updates,
  // future multi-step updates shouldn't claim impact of this feature again.
  // The `import_metadata` is thus initialized to a neutral element.
  ProfileImportMetadata import_metadata;
  import_metadata.origin = origin;
  form_data_importer->AddMultiStepImportCandidate(profile, import_metadata,
                                                  /*is_imported=*/true);
}

}  // namespace

using UserDecision = AutofillClient::AddressPromptUserDecision;

AddressProfileSaveManager::AddressProfileSaveManager(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

AddressProfileSaveManager::~AddressProfileSaveManager() = default;

void AddressProfileSaveManager::ImportProfileFromForm(
    const AutofillProfile& observed_profile,
    const std::string& app_locale,
    const GURL& url,
    bool allow_only_silent_updates,
    ProfileImportMetadata import_metadata) {
  MaybeOfferSavePrompt(std::make_unique<ProfileImportProcess>(
      observed_profile, app_locale, url, &address_data_manager(),
      allow_only_silent_updates, import_metadata));
}

void AddressProfileSaveManager::MaybeOfferSavePrompt(
    std::unique_ptr<ProfileImportProcess> import_process) {
  switch (import_process->import_type()) {
    // If the import was a duplicate, only results in silent updates or if the
    // import of a new profile or a profile update is blocked, finish the
    // process without initiating a user prompt
    case AutofillProfileImportType::kDuplicateImport:
    case AutofillProfileImportType::kSilentUpdate:
    case AutofillProfileImportType::kSilentUpdateForIncompleteProfile:
    case AutofillProfileImportType::kSuppressedNewProfile:
    case AutofillProfileImportType::kSuppressedConfirmableMergeAndSilentUpdate:
    case AutofillProfileImportType::kSuppressedConfirmableMerge:
    case AutofillProfileImportType::kUnusableIncompleteProfile:
      import_process->AcceptWithoutPrompt();
      FinalizeProfileImport(std::move(import_process));
      return;

    // The import of a new profile, a merge with an existing profile that
    // changes a settings-visible value of an existing profile, or a profile
    // migration triggers a user prompt.
    case AutofillProfileImportType::kNewProfile:
    case AutofillProfileImportType::kConfirmableMerge:
    case AutofillProfileImportType::kConfirmableMergeAndSilentUpdate:
    case AutofillProfileImportType::kProfileMigration:
    case AutofillProfileImportType::kProfileMigrationAndSilentUpdate:
      if (address_data_manager().auto_accept_address_imports_for_testing()) {
        import_process->AcceptWithoutEdits();
        FinalizeProfileImport(std::move(import_process));
        return;
      }
      OfferSavePrompt(std::move(import_process));
      return;

    case AutofillProfileImportType::kImportTypeUnspecified:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

void AddressProfileSaveManager::OfferSavePrompt(
    std::unique_ptr<ProfileImportProcess> import_process) {
  // The prompt should not have been shown yet.
  DCHECK(!import_process->prompt_shown());

  // Initiate the prompt and mark it as shown.
  // The import process that carries to state of the current import process is
  // attached to the callback.
  import_process->set_prompt_was_shown();
  ProfileImportProcess* process_ptr = import_process.get();
  client_->ConfirmSaveAddressProfile(
      process_ptr->import_candidate().value(),
      base::OptionalToPtr(process_ptr->merge_candidate()),
      process_ptr->is_migration(),
      base::BindOnce(&AddressProfileSaveManager::OnUserDecision,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(import_process)));
}

void AddressProfileSaveManager::OnUserDecision(
    std::unique_ptr<ProfileImportProcess> import_process,
    UserDecision decision,
    base::optional_ref<const AutofillProfile> edited_profile) {
  DCHECK(import_process->prompt_shown());

  import_process->SetUserDecision(decision, edited_profile);
  FinalizeProfileImport(std::move(import_process));
}

void AddressProfileSaveManager::FinalizeProfileImport(
    std::unique_ptr<ProfileImportProcess> import_process) {
  import_process->CollectMetrics(client_->GetUkmRecorder(),
                                 client_->GetUkmSourceId(),
                                 address_data_manager().GetProfiles());
  import_process->ApplyImport();

  AdjustNewProfileStrikes(*import_process);
  AdjustUpdateProfileStrikes(*import_process);
  AdjustMigrateProfileStrikes(*import_process);

  if (import_process->UserAccepted()) {
    const std::optional<AutofillProfile>& confirmed_import_candidate =
        import_process->confirmed_import_candidate();
    DCHECK(confirmed_import_candidate);
    AddMultiStepComplementCandidate(client_->GetFormDataImporter(),
                                    *confirmed_import_candidate,
                                    import_process->import_metadata().origin);
  }

  ClearPendingImport(std::move(import_process));
}

void AddressProfileSaveManager::AdjustNewProfileStrikes(
    ProfileImportProcess& import_process) {
  if (import_process.import_type() != AutofillProfileImportType::kNewProfile) {
    return;
  }
  const GURL& url = import_process.form_source_url();
  if (import_process.UserDeclined()) {
    address_data_manager().AddStrikeToBlockNewProfileImportForDomain(url);
  } else if (import_process.UserAccepted()) {
    address_data_manager().RemoveStrikesToBlockNewProfileImportForDomain(url);
  }
}

void AddressProfileSaveManager::AdjustUpdateProfileStrikes(
    ProfileImportProcess& import_process) {
  if (!import_process.is_confirmable_update()) {
    return;
  }
  CHECK(import_process.merge_candidate().has_value());
  const std::string& candidate_guid = import_process.import_candidate()->guid();
  if (import_process.UserDeclined()) {
    address_data_manager().AddStrikeToBlockProfileUpdate(candidate_guid);
  } else if (import_process.UserAccepted()) {
    address_data_manager().RemoveStrikesToBlockProfileUpdate(candidate_guid);
  }
}

void AddressProfileSaveManager::AdjustMigrateProfileStrikes(
    ProfileImportProcess& import_process) {
  if (!import_process.is_migration()) {
    return;
  }
  CHECK(import_process.import_candidate().has_value());
  const std::string& candidate_guid = import_process.import_candidate()->guid();
  if (import_process.UserAccepted()) {
    // Even though the profile to migrate changes GUID after a migration, the
    // original GUID should still be freed up from strikes.
    address_data_manager().RemoveStrikesToBlockProfileMigration(candidate_guid);
  } else if (import_process.UserDeclined()) {
    if (import_process.user_decision() == UserDecision::kNever) {
      address_data_manager().AddMaxStrikesToBlockProfileMigration(
          candidate_guid);
    } else {
      address_data_manager().AddStrikeToBlockProfileMigration(candidate_guid);
    }
  }
}

void AddressProfileSaveManager::ClearPendingImport(
    std::unique_ptr<ProfileImportProcess> import_process) {}

AddressDataManager& AddressProfileSaveManager::address_data_manager() {
  return client_->GetPersonalDataManager()->address_data_manager();
}

}  // namespace autofill
