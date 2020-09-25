// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_save_manager_impl.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/validation.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/multi_store_password_save_manager.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormStructure;
using autofill::ValueElementPair;

namespace password_manager {

namespace {

ValueElementPair PasswordToSave(const PasswordForm& form) {
  if (form.new_password_value.empty()) {
    DCHECK(!form.password_value.empty() || form.IsFederatedCredential());
    return {form.password_value, form.password_element};
  }
  return {form.new_password_value, form.new_password_element};
}

PasswordForm PendingCredentialsForNewCredentials(
    const PasswordForm& parsed_submitted_form,
    const FormData* observed_form,
    const base::string16& password_element,
    bool is_http_auth,
    bool is_credential_api_save) {
  if (is_http_auth || is_credential_api_save)
    return parsed_submitted_form;

  PasswordForm pending_credentials = parsed_submitted_form;
  if (observed_form)
    pending_credentials.form_data = *observed_form;
  // The password value will be filled in later, remove any garbage for now.
  pending_credentials.password_value.clear();
  // The password element should be determined earlier in |PasswordToSave|.
  pending_credentials.password_element = password_element;
  // The new password's value and element name should be empty.
  pending_credentials.new_password_value.clear();
  pending_credentials.new_password_element.clear();
  pending_credentials.new_password_element_renderer_id = FieldRendererId();
  return pending_credentials;
}

// Helper to get the platform specific identifier by which autofill and password
// manager refer to a field. See http://crbug.com/896594
base::string16 GetPlatformSpecificIdentifier(const FormFieldData& field) {
#if defined(OS_IOS)
  return field.unique_id;
#else
  return field.name;
#endif
}

// Copies field properties masks from the form |from| to the form |to|.
void CopyFieldPropertiesMasks(const FormData& from, FormData* to) {
  // Skip copying if the number of fields is different.
  if (from.fields.size() != to->fields.size())
    return;

  for (size_t i = 0; i < from.fields.size(); ++i) {
    to->fields[i].properties_mask =
        GetPlatformSpecificIdentifier(to->fields[i]) ==
                GetPlatformSpecificIdentifier(from.fields[i])
            ? from.fields[i].properties_mask
            : autofill::FieldPropertiesFlags::kErrorOccurred;
  }
}

// Filter sensitive information, duplicates and |username_value| out from
// |form->all_possible_usernames|.
void SanitizePossibleUsernames(PasswordForm* form) {
  auto& usernames = form->all_possible_usernames;

  // Deduplicate.
  std::sort(usernames.begin(), usernames.end());
  usernames.erase(std::unique(usernames.begin(), usernames.end()),
                  usernames.end());

  // Filter out |form->username_value| and sensitive information.
  const base::string16& username_value = form->username_value;
  base::EraseIf(usernames, [&username_value](const ValueElementPair& pair) {
    return pair.first == username_value ||
           autofill::IsValidCreditCardNumber(pair.first) ||
           autofill::IsSSN(pair.first);
  });
}

}  // namespace

// static
std::unique_ptr<PasswordSaveManagerImpl>
PasswordSaveManagerImpl::CreatePasswordSaveManagerImpl(
    const PasswordManagerClient* client) {
  auto profile_form_saver =
      std::make_unique<FormSaverImpl>(client->GetProfilePasswordStore());

  return base::FeatureList::IsEnabled(
             password_manager::features::kEnablePasswordsAccountStorage)
             ? std::make_unique<MultiStorePasswordSaveManager>(
                   std::move(profile_form_saver),
                   std::make_unique<FormSaverImpl>(
                       client->GetAccountPasswordStore()))
             : std::make_unique<PasswordSaveManagerImpl>(
                   std::move(profile_form_saver));
}

PasswordSaveManagerImpl::PasswordSaveManagerImpl(
    std::unique_ptr<FormSaver> form_saver)
    : form_saver_(std::move(form_saver)) {}

PasswordSaveManagerImpl::~PasswordSaveManagerImpl() = default;

const PasswordForm& PasswordSaveManagerImpl::GetPendingCredentials() const {
  return pending_credentials_;
}

const base::string16& PasswordSaveManagerImpl::GetGeneratedPassword() const {
  DCHECK(generation_manager_);
  return generation_manager_->generated_password();
}

FormSaver* PasswordSaveManagerImpl::GetFormSaver() const {
  return form_saver_.get();
}

void PasswordSaveManagerImpl::Init(
    PasswordManagerClient* client,
    const FormFetcher* form_fetcher,
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder,
    VotesUploader* votes_uploader) {
  client_ = client;
  form_fetcher_ = form_fetcher;
  metrics_recorder_ = metrics_recorder;
  votes_uploader_ = votes_uploader;
}

void PasswordSaveManagerImpl::CreatePendingCredentials(
    const PasswordForm& parsed_submitted_form,
    const FormData* observed_form,
    const FormData& submitted_form,
    bool is_http_auth,
    bool is_credential_api_save) {
  const PasswordForm* similar_saved_form = nullptr;
  std::tie(similar_saved_form, pending_credentials_state_) =
      FindSimilarSavedFormAndComputeState(parsed_submitted_form);

  base::Optional<base::string16> generated_password;
  if (HasGeneratedPassword())
    generated_password = generation_manager_->generated_password();

  pending_credentials_ = BuildPendingCredentials(
      pending_credentials_state_, parsed_submitted_form, observed_form,
      submitted_form, generated_password, is_http_auth, is_credential_api_save,
      similar_saved_form);

  if (votes_uploader_)
    SetVotesAndRecordMetricsForPendingCredentials(parsed_submitted_form);
}

void PasswordSaveManagerImpl::SetVotesAndRecordMetricsForPendingCredentials(
    const PasswordForm& parsed_submitted_form) {
  DCHECK(votes_uploader_);
  votes_uploader_->set_password_overridden(false);
  switch (pending_credentials_state_) {
    case PendingCredentialsState::NEW_LOGIN: {
      // Generate username correction votes.
      bool username_correction_found =
          votes_uploader_->FindCorrectedUsernameElement(
              form_fetcher_->GetAllRelevantMatches(),
              parsed_submitted_form.username_value,
              parsed_submitted_form.password_value);
      UMA_HISTOGRAM_BOOLEAN("PasswordManager.UsernameCorrectionFound",
                            username_correction_found);
      if (username_correction_found) {
        metrics_recorder_->RecordDetailedUserAction(
            password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
                kCorrectedUsernameInForm);
      }
      break;
    }
    case PendingCredentialsState::UPDATE:
      votes_uploader_->set_password_overridden(true);
      break;
    case PendingCredentialsState::NONE:
    case PendingCredentialsState::AUTOMATIC_SAVE:
    case PendingCredentialsState::EQUAL_TO_SAVED_MATCH:
      // Nothing to be done in these cases.
      break;
  }
}

void PasswordSaveManagerImpl::ResetPendingCredentials() {
  pending_credentials_ = PasswordForm();
  pending_credentials_state_ = PendingCredentialsState::NONE;
}

void PasswordSaveManagerImpl::Save(const FormData* observed_form,
                                   const PasswordForm& parsed_submitted_form) {
  if (IsPasswordUpdate() &&
      pending_credentials_.type == PasswordForm::Type::kGenerated &&
      !HasGeneratedPassword()) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_OVERRIDDEN);
    pending_credentials_.type = PasswordForm::Type::kManual;
  }

  if (IsNewLogin()) {
    SanitizePossibleUsernames(&pending_credentials_);
    pending_credentials_.date_created = base::Time::Now();
  }

  SavePendingToStore(observed_form, parsed_submitted_form);

  if (pending_credentials_.times_used == 1 &&
      pending_credentials_.type == PasswordForm::Type::kGenerated) {
    // This also includes PSL matched credentials.
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_USED);
  }
}

void PasswordSaveManagerImpl::Update(
    const PasswordForm& credentials_to_update,
    const FormData* observed_form,
    const PasswordForm& parsed_submitted_form) {
  base::string16 password_to_save = pending_credentials_.password_value;
  bool skip_zero_click = pending_credentials_.skip_zero_click;
  pending_credentials_ = credentials_to_update;
  pending_credentials_.password_value = password_to_save;
  pending_credentials_.skip_zero_click = skip_zero_click;
  pending_credentials_.date_last_used = base::Time::Now();

  pending_credentials_state_ = PendingCredentialsState::UPDATE;

  SavePendingToStore(observed_form, parsed_submitted_form);
}

void PasswordSaveManagerImpl::PermanentlyBlacklist(
    const PasswordStore::FormDigest& form_digest) {
  DCHECK(!client_->IsIncognito());
  form_saver_->PermanentlyBlacklist(form_digest);
}

void PasswordSaveManagerImpl::Unblacklist(
    const PasswordStore::FormDigest& form_digest) {
  form_saver_->Unblacklist(form_digest);
}

void PasswordSaveManagerImpl::PresaveGeneratedPassword(
    PasswordForm parsed_form) {
  if (!HasGeneratedPassword()) {
    generation_manager_ = std::make_unique<PasswordGenerationManager>(client_);
    votes_uploader_->set_generated_password_changed(false);
    metrics_recorder_->SetGeneratedPasswordStatus(
        PasswordFormMetricsRecorder::GeneratedPasswordStatus::
            kPasswordAccepted);
  } else {
    // If the password is already generated and a new value to presave differs
    // from the presaved one, then mark that the generated password was
    // changed. If a user recovers the original generated password, it will be
    // recorded as a password change.
    if (generation_manager_->generated_password() !=
        parsed_form.password_value) {
      votes_uploader_->set_generated_password_changed(true);
      metrics_recorder_->SetGeneratedPasswordStatus(
          PasswordFormMetricsRecorder::GeneratedPasswordStatus::
              kPasswordEdited);
    }
  }
  votes_uploader_->set_has_generated_password(true);

  generation_manager_->PresaveGeneratedPassword(
      std::move(parsed_form),
      GetRelevantMatchesForGeneration(form_fetcher_->GetAllRelevantMatches()),
      GetFormSaverForGeneration());
}

void PasswordSaveManagerImpl::GeneratedPasswordAccepted(
    PasswordForm parsed_form,
    base::WeakPtr<PasswordManagerDriver> driver) {
  generation_manager_ = std::make_unique<PasswordGenerationManager>(client_);
  generation_manager_->GeneratedPasswordAccepted(
      std::move(parsed_form),
      GetRelevantMatchesForGeneration(form_fetcher_->GetNonFederatedMatches()),
      GetRelevantMatchesForGeneration(form_fetcher_->GetFederatedMatches()),
      driver);
}

void PasswordSaveManagerImpl::PasswordNoLongerGenerated() {
  DCHECK(generation_manager_);
  generation_manager_->PasswordNoLongerGenerated(GetFormSaverForGeneration());
  generation_manager_.reset();

  votes_uploader_->set_has_generated_password(false);
  votes_uploader_->set_generated_password_changed(false);
  metrics_recorder_->SetGeneratedPasswordStatus(
      PasswordFormMetricsRecorder::GeneratedPasswordStatus::kPasswordDeleted);
}

void PasswordSaveManagerImpl::MoveCredentialsToAccountStore(
    metrics_util::MoveToAccountStoreTrigger) {
  // Moving credentials is only supported in MultiStorePasswordSaveManager.
  NOTREACHED();
}

void PasswordSaveManagerImpl::BlockMovingToAccountStoreFor(
    const autofill::GaiaIdHash& gaia_id_hash) {
  // Moving credentials is only supported in MultiStorePasswordSaveManager.
  NOTREACHED();
}

bool PasswordSaveManagerImpl::IsNewLogin() const {
  return pending_credentials_state_ == PendingCredentialsState::NEW_LOGIN ||
         pending_credentials_state_ == PendingCredentialsState::AUTOMATIC_SAVE;
}

bool PasswordSaveManagerImpl::IsPasswordUpdate() const {
  return pending_credentials_state_ == PendingCredentialsState::UPDATE;
}

bool PasswordSaveManagerImpl::HasGeneratedPassword() const {
  return generation_manager_ && generation_manager_->HasGeneratedPassword();
}

std::unique_ptr<PasswordSaveManager> PasswordSaveManagerImpl::Clone() {
  auto result = std::make_unique<PasswordSaveManagerImpl>(form_saver_->Clone());
  CloneInto(result.get());
  return result;
}

// static
PendingCredentialsState PasswordSaveManagerImpl::ComputePendingCredentialsState(
    const PasswordForm& parsed_submitted_form,
    const PasswordForm* similar_saved_form) {
  ValueElementPair password_to_save(PasswordToSave(parsed_submitted_form));
  // Check if there are previously saved credentials (that were available to
  // autofilling) matching the actually submitted credentials.
  if (!similar_saved_form)
    return PendingCredentialsState::NEW_LOGIN;

  // A similar credential exists in the store already.
  if (similar_saved_form->password_value != password_to_save.first)
    return PendingCredentialsState::UPDATE;

  // If the autofilled credentials were a PSL match, store a copy with the
  // current origin and signon realm. This ensures that on the next visit, a
  // precise match is found.
  if (similar_saved_form->is_public_suffix_match)
    return PendingCredentialsState::AUTOMATIC_SAVE;

  return PendingCredentialsState::EQUAL_TO_SAVED_MATCH;
}

// static
PasswordForm PasswordSaveManagerImpl::BuildPendingCredentials(
    PendingCredentialsState pending_credentials_state,
    const PasswordForm& parsed_submitted_form,
    const FormData* observed_form,
    const FormData& submitted_form,
    const base::Optional<base::string16>& generated_password,
    bool is_http_auth,
    bool is_credential_api_save,
    const PasswordForm* similar_saved_form) {
  PasswordForm pending_credentials;

  ValueElementPair password_to_save(PasswordToSave(parsed_submitted_form));

  switch (pending_credentials_state) {
    case PendingCredentialsState::NEW_LOGIN:
      // No stored credentials can be matched to the submitted form. Offer to
      // save new credentials.
      pending_credentials = PendingCredentialsForNewCredentials(
          parsed_submitted_form, observed_form, password_to_save.second,
          is_http_auth, is_credential_api_save);
      break;
    case PendingCredentialsState::EQUAL_TO_SAVED_MATCH:
    case PendingCredentialsState::UPDATE:
      pending_credentials = *similar_saved_form;
      break;
    case PendingCredentialsState::AUTOMATIC_SAVE:
      pending_credentials = *similar_saved_form;

      // Update credential to reflect that it has been used for submission.
      // If this isn't updated, then password generation uploads are off for
      // sites where PSL matching is required to fill the login form, as two
      // PASSWORD votes are uploaded per saved password instead of one.
      password_manager_util::UpdateMetadataForUsage(&pending_credentials);

      // Update |pending_credentials| in order to be able correctly save it.
      pending_credentials.url = parsed_submitted_form.url;
      pending_credentials.signon_realm = parsed_submitted_form.signon_realm;
      pending_credentials.action = parsed_submitted_form.action;
      break;
    case PendingCredentialsState::NONE:
      NOTREACHED();
      break;
  }

  pending_credentials.password_value =
      generated_password.value_or(password_to_save.first);
  pending_credentials.date_last_used = base::Time::Now();
  pending_credentials.form_has_autofilled_value =
      parsed_submitted_form.form_has_autofilled_value;
  pending_credentials.all_possible_passwords =
      parsed_submitted_form.all_possible_passwords;
  CopyFieldPropertiesMasks(submitted_form, &pending_credentials.form_data);

  // If we're dealing with an API-driven provisionally saved form, then take
  // the server provided values. We don't do this for non-API forms, as
  // those will never have those members set.
  if (parsed_submitted_form.type == PasswordForm::Type::kApi) {
    pending_credentials.skip_zero_click = parsed_submitted_form.skip_zero_click;
    pending_credentials.display_name = parsed_submitted_form.display_name;
    pending_credentials.federation_origin =
        parsed_submitted_form.federation_origin;
    pending_credentials.icon_url = parsed_submitted_form.icon_url;
    // It's important to override |signon_realm| for federated credentials
    // because it has format "federation://" + origin_host + "/" +
    // federation_host
    pending_credentials.signon_realm = parsed_submitted_form.signon_realm;
  }

  if (generated_password.has_value())
    pending_credentials.type = PasswordForm::Type::kGenerated;

  return pending_credentials;
}

std::pair<const PasswordForm*, PendingCredentialsState>
PasswordSaveManagerImpl::FindSimilarSavedFormAndComputeState(
    const PasswordForm& parsed_submitted_form) const {
  const PasswordForm* similar_saved_form =
      password_manager_util::GetMatchForUpdating(
          parsed_submitted_form, form_fetcher_->GetBestMatches());
  return std::make_pair(similar_saved_form,
                        ComputePendingCredentialsState(parsed_submitted_form,
                                                       similar_saved_form));
}

void PasswordSaveManagerImpl::SavePendingToStore(
    const FormData* observed_form,
    const PasswordForm& parsed_submitted_form) {
  UploadVotesAndMetrics(observed_form, parsed_submitted_form);

  if (HasGeneratedPassword()) {
    generation_manager_->CommitGeneratedPassword(
        pending_credentials_, form_fetcher_->GetAllRelevantMatches(),
        GetOldPassword(parsed_submitted_form), GetFormSaverForGeneration());
  } else {
    SavePendingToStoreImpl(parsed_submitted_form);
  }
}

void PasswordSaveManagerImpl::SavePendingToStoreImpl(
    const PasswordForm& parsed_submitted_form) {
  auto matches = form_fetcher_->GetAllRelevantMatches();
  base::string16 old_password = GetOldPassword(parsed_submitted_form);
  if (IsNewLogin()) {
    form_saver_->Save(pending_credentials_, matches, old_password);
  } else {
    // It sounds wrong that we still update even if the state is NONE. We
    // should double check if this actually necessary. Currently some tests
    // depend on this behavior.
    form_saver_->Update(pending_credentials_, matches, old_password);
  }
}

base::string16 PasswordSaveManagerImpl::GetOldPassword(
    const PasswordForm& parsed_submitted_form) const {
  const PasswordForm* similar_saved_form =
      FindSimilarSavedFormAndComputeState(parsed_submitted_form).first;
  return similar_saved_form ? similar_saved_form->password_value
                            : base::string16();
}

void PasswordSaveManagerImpl::UploadVotesAndMetrics(
    const FormData* observed_form,
    const PasswordForm& parsed_submitted_form) {
  if (IsNewLogin()) {
    metrics_util::LogNewlySavedPasswordIsGenerated(
        pending_credentials_.type == PasswordForm::Type::kGenerated,
        client_->GetPasswordFeatureManager()
            ->ComputePasswordAccountStorageUsageLevel());
    // Don't send votes if there was no observed form.
    if (observed_form) {
      votes_uploader_->SendVotesOnSave(*observed_form, parsed_submitted_form,
                                       form_fetcher_->GetBestMatches(),
                                       &pending_credentials_);
    }
    return;
  }

  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  DCHECK(form_fetcher_->GetPreferredMatch() ||
         pending_credentials_.IsFederatedCredential());
  // If we're doing an Update, we either autofilled correctly and need to
  // update the stats, or the user typed in a new password for autofilled
  // username.
  DCHECK(!client_->IsIncognito());

  password_manager_util::UpdateMetadataForUsage(&pending_credentials_);

  base::RecordAction(
      base::UserMetricsAction("PasswordManager_LoginFollowingAutofill"));

  // Check to see if this form is a candidate for password generation.
  // Do not send votes if there was no observed form. Furthermore, don't send
  // votes on change password forms, since they were already sent in Update()
  // method.
  if (observed_form && !parsed_submitted_form.IsPossibleChangePasswordForm()) {
    votes_uploader_->SendVoteOnCredentialsReuse(
        *observed_form, parsed_submitted_form, &pending_credentials_);
  }
  if (IsPasswordUpdate()) {
    votes_uploader_->UploadPasswordVote(
        parsed_submitted_form, parsed_submitted_form, autofill::NEW_PASSWORD,
        FormStructure(pending_credentials_.form_data).FormSignatureAsStr());
  }

  if (pending_credentials_.times_used == 1) {
    votes_uploader_->UploadFirstLoginVotes(form_fetcher_->GetBestMatches(),
                                           pending_credentials_,
                                           parsed_submitted_form);
  }
}

FormSaver* PasswordSaveManagerImpl::GetFormSaverForGeneration() {
  DCHECK(form_saver_);
  return form_saver_.get();
}

std::vector<const PasswordForm*>
PasswordSaveManagerImpl::GetRelevantMatchesForGeneration(
    const std::vector<const PasswordForm*>& matches) {
  return matches;
}

void PasswordSaveManagerImpl::CloneInto(PasswordSaveManagerImpl* clone) {
  DCHECK(clone);
  if (generation_manager_)
    clone->generation_manager_ = generation_manager_->Clone();

  clone->pending_credentials_ = pending_credentials_;
  clone->pending_credentials_state_ = pending_credentials_state_;
}

}  // namespace password_manager
