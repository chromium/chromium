// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_profile_save_manager.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

AddressProfileSaveManager::AddressProfileSaveManager(
    AutofillClient* client,
    PersonalDataManager* personal_data_manager)
    : client_(client), personal_data_manager_(personal_data_manager) {}

AddressProfileSaveManager::~AddressProfileSaveManager() = default;

void AddressProfileSaveManager::ImportProfileFromForm(
    const AutofillProfile& observed_profile,
    const std::string& app_locale,
    const GURL& url) {
  // Without a personal data manager, profile storage is not possible.
  if (!personal_data_manager_)
    return;

  // If the explicit save prompts are not enabled, revert back to the legacy
  // behavior and directly import the observed profile without recording any
  // additional metrics.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAddressProfileSavePrompt)) {
    personal_data_manager_->SaveImportedProfile(observed_profile);
    return;
  }

  // Otherwise, check if there is already an import process started.
  if (pending_import_.has_value()) {
    // If either the observed profile is the same or if the prompt has already
    // been shown to the user, do nothing and return.
    if (pending_import_->observed_profile() == observed_profile ||
        pending_import_->prompt_shown()) {
      return;
    }
  }

  // Create a new pending import process. If there was already an import
  // process, it is only overwritten if the UI request was not initialized yet.
  pending_import_ = ProfileImportProcess(
      observed_profile, personal_data_manager_->GetProfiles(), app_locale, url,
      personal_data_manager_->IsNewProfileImportBlockedForDomain(url));

  MaybeOfferSavePrompt();
}

void AddressProfileSaveManager::MaybeOfferSavePrompt() {
  DCHECK(pending_import_.has_value());

  switch (pending_import_->import_type()) {
    // If the import was a duplicate, only results in silent updates or if the
    // import of a new profile is blocked on the used domain, finish the process
    // without initiating a user prompt
    case AutofillProfileImportType::kDuplicateImport:
    case AutofillProfileImportType::kSilentUpdate:
    case AutofillProfileImportType::kSuppressedNewProfile:
      pending_import_->AcceptWithoutPrompt();
      FinalizeProfileImport();
      break;

    // Both the import of a new profile, or a merge with an existing profile
    // that changes a settings-visible value of an existing profile triggers a
    // user prompt.
    case AutofillProfileImportType::kNewProfile:
    case AutofillProfileImportType::kConfirmableMerge:
      OfferSavePrompt();
      break;

    case AutofillProfileImportType::kImportTypeUnspecified:
      NOTREACHED();
      break;
  }
}

void AddressProfileSaveManager::OfferSavePrompt() {
  DCHECK(pending_import_.has_value());
  // The prompt should not have been shown yet.
  DCHECK(!pending_import_->prompt_shown());

  // TODO(crbug.com/1175693): Pass the correct SaveAddressProfilePromptOptions
  // below.

  // TODO(crbug.com/1175693): Check pending_import_->set_prompt_was_shown() is
  // always correct even in cases where it conflicts with
  // SaveAddressProfilePromptOptions

  // Initiate the prompt and mark it as shown.
  pending_import_->set_prompt_was_shown();
  client_->ConfirmSaveAddressProfile(
      pending_import_->import_candidate().value(),
      base::OptionalOrNullptr(pending_import_->merge_candidate()),
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      base::BindOnce(&AddressProfileSaveManager::OnUserDecision,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AddressProfileSaveManager::OnUserDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision,
    AutofillProfile edited_profile) {
  DCHECK(pending_import_.has_value());
  DCHECK(pending_import_->prompt_shown());

  pending_import_->SetUserDecision(decision, edited_profile);
  FinalizeProfileImport();
}

void AddressProfileSaveManager::FinalizeProfileImport() {
  DCHECK(pending_import_.has_value());
  DCHECK(personal_data_manager_);

  // If the profiles changed at all, reset the full list of AutofillProfiles in
  // the personal data manager.
  if (pending_import_->ProfilesChanged()) {
    std::vector<AutofillProfile> resulting_profiles =
        pending_import_->GetResultingProfiles();
    personal_data_manager_->SetProfiles(&resulting_profiles);
  }

  // If the import of a new profile was declined, add a strike for this source
  // url. If it was accepted, reset the potentially existing strikes.
  if (pending_import_->import_type() ==
      AutofillProfileImportType::kNewProfile) {
    if (pending_import_->user_decision() ==
        AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined) {
      personal_data_manager_->AddStrikeToBlockNewProfileImportForDomain(
          pending_import()->form_source_url());
    } else if (pending_import_->user_decision() ==
               AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted) {
      personal_data_manager_->RemoveStrikesToBlockNewProfileImportForDomain(
          pending_import()->form_source_url());
    }
  }

  pending_import_->CollectMetrics();
  ClearPendingImport();
}

void AddressProfileSaveManager::ClearPendingImport() {
  pending_import_.reset();
}

}  // namespace autofill
