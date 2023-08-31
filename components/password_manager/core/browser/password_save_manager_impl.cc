// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_save_manager_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/base/gaia_id_hash.h"

using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormFieldData;

namespace password_manager {

namespace {

struct PendingCredentialsStates {
  PendingCredentialsState profile_store_state = PendingCredentialsState::NONE;
  PendingCredentialsState account_store_state = PendingCredentialsState::NONE;

  raw_ptr<const PasswordForm> similar_saved_form_from_profile_store = nullptr;
  raw_ptr<const PasswordForm> similar_saved_form_from_account_store = nullptr;
};

AlternativeElement PasswordToSave(const PasswordForm& form) {
  if (form.new_password_value.empty()) {
    DCHECK(!form.password_value.empty() || form.IsFederatedCredential());
    return {AlternativeElement::Value(form.password_value),
            form.password_element_renderer_id,
            AlternativeElement::Name(form.password_element)};
  }
  return {AlternativeElement::Value(form.new_password_value),
          form.new_password_element_renderer_id,
          AlternativeElement::Name(form.new_password_element)};
}

PasswordForm PendingCredentialsForNewCredentials(
    const PasswordForm& parsed_submitted_form,
    const FormData* observed_form,
    const std::u16string& password_element,
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

// Copies field properties masks from the form |from| to the form |to|.
void CopyFieldPropertiesMasks(const FormData& from, FormData* to) {
  // Skip copying if the number of fields is different.
  if (from.fields.size() != to->fields.size())
    return;

  for (size_t i = 0; i < from.fields.size(); ++i) {
    to->fields[i].properties_mask =
        to->fields[i].name == from.fields[i].name
            ? from.fields[i].properties_mask
            : autofill::FieldPropertiesFlags::kErrorOccurred;
  }
}

// Filter sensitive information, duplicates and |username_value| out from
// |form->all_alternative_usernames|.
void SanitizeAlternativeUsernames(PasswordForm* form) {
  auto& usernames = form->all_alternative_usernames;

  // Deduplicate.
  std::sort(usernames.begin(), usernames.end());
  usernames.erase(std::unique(usernames.begin(), usernames.end()),
                  usernames.end());

  // Filter out |form->username_value| and sensitive information.
  const std::u16string& username_value = form->username_value;
  base::EraseIf(usernames,
                [&username_value](const AlternativeElement& element) {
                  return element.value == username_value ||
                         autofill::IsValidCreditCardNumber(element.value) ||
                         autofill::IsSSN(element.value);
                });
}

std::vector<const PasswordForm*> MatchesInStore(
    const std::vector<const PasswordForm*>& matches,
    PasswordForm::Store store) {
  std::vector<const PasswordForm*> store_matches;
  for (const PasswordForm* match : matches) {
    DCHECK(match->in_store != PasswordForm::Store::kNotSet);
    if (match->in_store == store)
      store_matches.push_back(match);
  }
  return store_matches;
}

std::vector<const PasswordForm*> AccountStoreMatches(
    const std::vector<const PasswordForm*>& matches) {
  return MatchesInStore(matches, PasswordForm::Store::kAccountStore);
}

std::vector<const PasswordForm*> ProfileStoreMatches(
    const std::vector<const PasswordForm*>& matches) {
  return MatchesInStore(matches, PasswordForm::Store::kProfileStore);
}

bool AccountStoreMatchesContainForm(
    const std::vector<const PasswordForm*>& matches,
    const PasswordForm& form) {
  DCHECK(base::ranges::all_of(matches, &PasswordForm::IsUsingAccountStore));
  return base::ranges::any_of(matches, [&form](const PasswordForm* match) {
    return ArePasswordFormUniqueKeysEqual(*match, form) &&
           match->password_value == form.password_value;
  });
}

PendingCredentialsState ComputePendingCredentialsState(
    const PasswordForm& parsed_submitted_form,
    const PasswordForm* similar_saved_form) {
  AlternativeElement password_to_save(PasswordToSave(parsed_submitted_form));
  // Check if there are previously saved credentials (that were available to
  // autofilling) matching the actually submitted credentials.
  if (!similar_saved_form) {
    return PendingCredentialsState::NEW_LOGIN;
  }

  // A similar credential exists in the store already.
  if (similar_saved_form->password_value != password_to_save.value) {
    return PendingCredentialsState::UPDATE;
  }

  // If the autofilled credentials were a PSL match, store a copy with the
  // current origin and signon realm. This ensures that on the next visit, a
  // precise match is found.
  if (password_manager_util::GetMatchType(*similar_saved_form) ==
      password_manager_util::GetLoginMatchType::kPSL) {
    return PendingCredentialsState::AUTOMATIC_SAVE;
  }

  return PendingCredentialsState::EQUAL_TO_SAVED_MATCH;
}

PendingCredentialsStates ComputePendingCredentialsStates(
    const PasswordForm& parsed_submitted_form,
    const std::vector<const PasswordForm*>& matches,
    bool username_updated_in_bubble) {
  PendingCredentialsStates result;

  // Try to find a similar existing saved form from each of the stores.
  result.similar_saved_form_from_profile_store =
      password_manager_util::GetMatchForUpdating(parsed_submitted_form,
                                                 ProfileStoreMatches(matches),
                                                 username_updated_in_bubble);
  result.similar_saved_form_from_account_store =
      password_manager_util::GetMatchForUpdating(parsed_submitted_form,
                                                 AccountStoreMatches(matches),
                                                 username_updated_in_bubble);

  // Compute the PendingCredentialsState (i.e. what to do - save, update, silent
  // update) separately for the two stores.
  result.profile_store_state = ComputePendingCredentialsState(
      parsed_submitted_form, result.similar_saved_form_from_profile_store);
  result.account_store_state = ComputePendingCredentialsState(
      parsed_submitted_form, result.similar_saved_form_from_account_store);

  return result;
}

PendingCredentialsState ResolvePendingCredentialsStates(
    PendingCredentialsState profile_state,
    PendingCredentialsState account_state) {
  // The result of this resolution will be used to decide whether to show a
  // save or update prompt to the user. Resolve the two states to a single
  // "canonical" one according to the following hierarchy:
  // AUTOMATIC_SAVE > EQUAL_TO_SAVED_MATCH > UPDATE > NEW_LOGIN
  // Note that UPDATE or NEW_LOGIN will result in an Update or Save bubble to
  // be shown (unless heuristics determined that we see non-password fields),
  // while AUTOMATIC_SAVE and EQUAL_TO_SAVED_MATCH will result in a silent
  // save/update.
  // Some interesting cases:
  // NEW_LOGIN means that store doesn't know about the credential yet. If the
  // other store knows anything at all, then that always wins.
  // EQUAL_TO_SAVED_MATCH vs UPDATE: This means one store had a match, the other
  // had a mismatch (same username but different password). The mismatch should
  // be updated silently, so resolve to EQUAL so that there's no visible prompt.
  // AUTOMATIC_SAVE vs EQUAL_TO_SAVED_MATCH: These are both silent, so it
  // doesn't really matter to which one we resolve.
  // AUTOMATIC_SAVE vs UPDATE: Similar to EQUAL_TO_SAVED_MATCH vs UPDATE, the
  // mismatch should be silently updated.
  if (profile_state == PendingCredentialsState::AUTOMATIC_SAVE ||
      account_state == PendingCredentialsState::AUTOMATIC_SAVE) {
    return PendingCredentialsState::AUTOMATIC_SAVE;
  }
  if (profile_state == PendingCredentialsState::EQUAL_TO_SAVED_MATCH ||
      account_state == PendingCredentialsState::EQUAL_TO_SAVED_MATCH) {
    return PendingCredentialsState::EQUAL_TO_SAVED_MATCH;
  }
  if (profile_state == PendingCredentialsState::UPDATE ||
      account_state == PendingCredentialsState::UPDATE) {
    return PendingCredentialsState::UPDATE;
  }
  if (profile_state == PendingCredentialsState::NEW_LOGIN ||
      account_state == PendingCredentialsState::NEW_LOGIN) {
    return PendingCredentialsState::NEW_LOGIN;
  }
  NOTREACHED();
  return PendingCredentialsState::NONE;
}

// Returns a PasswordForm that has all fields taken from |update| except
// date_created, times_used_in_html_form and moving_blocked_for_list that are
// taken from |original_form|.
PasswordForm UpdateFormPreservingDifferentFieldsAcrossStores(
    const PasswordForm& original_form,
    const PasswordForm& update) {
  PasswordForm result(update);
  result.date_created = original_form.date_created;
  result.times_used_in_html_form = original_form.times_used_in_html_form;
  result.moving_blocked_for_list = original_form.moving_blocked_for_list;
  return result;
}

bool AlternativeElementsContainValue(const AlternativeElementVector& elements,
                                     const std::u16string& value) {
  return base::ranges::any_of(elements,
                              [&value](const AlternativeElement& element) {
                                return element.value == value;
                              });
}

void PopulateAlternativeUsernames(
    const std::vector<const PasswordForm*>& best_matches,
    PasswordForm& form) {
  for (const PasswordForm* match : best_matches) {
    if ((match->username_value != form.username_value) &&
        !AlternativeElementsContainValue(form.all_alternative_usernames,
                                         match->username_value)) {
      form.all_alternative_usernames.emplace_back(
          AlternativeElement::Value(match->username_value));
    }
  }
}

}  // namespace

PasswordSaveManagerImpl::PasswordSaveManagerImpl(
    std::unique_ptr<FormSaver> profile_form_saver,
    std::unique_ptr<FormSaver> account_form_saver)
    : profile_store_form_saver_(std::move(profile_form_saver)),
      account_store_form_saver_(std::move(account_form_saver)) {}

PasswordSaveManagerImpl::PasswordSaveManagerImpl(
    const PasswordManagerClient* client)
    : PasswordSaveManagerImpl(
          std::make_unique<FormSaverImpl>(client->GetProfilePasswordStore()),
          client->GetAccountPasswordStore()
              ? std::make_unique<FormSaverImpl>(
                    client->GetAccountPasswordStore())
              : nullptr) {}

PasswordSaveManagerImpl::~PasswordSaveManagerImpl() = default;

const PasswordForm& PasswordSaveManagerImpl::GetPendingCredentials() const {
  return pending_credentials_;
}

const std::u16string& PasswordSaveManagerImpl::GetGeneratedPassword() const {
  DCHECK(generation_manager_);
  return generation_manager_->generated_password();
}

FormSaver* PasswordSaveManagerImpl::GetProfileStoreFormSaverForTesting() const {
  return profile_store_form_saver_.get();
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
  pending_credentials_ = BuildPendingCredentials(
      parsed_submitted_form, observed_form, submitted_form, is_http_auth,
      is_credential_api_save);

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
    pending_credentials_.type = PasswordForm::Type::kFormSubmission;
  }

  if (IsNewLogin()) {
    SanitizeAlternativeUsernames(&pending_credentials_);
    pending_credentials_.date_created = base::Time::Now();
  }

  SavePendingToStore(observed_form, parsed_submitted_form);

  if (pending_credentials_.times_used_in_html_form == 1 &&
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
  std::u16string password_to_save = pending_credentials_.password_value;
  bool skip_zero_click = pending_credentials_.skip_zero_click;
  pending_credentials_ = credentials_to_update;
  pending_credentials_.password_value = password_to_save;
  pending_credentials_.skip_zero_click = skip_zero_click;
  pending_credentials_.date_last_used = base::Time::Now();

  pending_credentials_state_ = PendingCredentialsState::UPDATE;

  SavePendingToStore(observed_form, parsed_submitted_form);
}

void PasswordSaveManagerImpl::Blocklist(const PasswordFormDigest& form_digest) {
  CHECK(!client_->IsOffTheRecord());
  if (account_store_form_saver_ && IsOptedInForAccountStorage() &&
      AccountStoreIsDefault()) {
    account_store_form_saver_->Blocklist(form_digest);
  } else {
    // For users who aren't yet opted-in to the account storage, we store their
    // blocklisted entries in the profile store.
    profile_store_form_saver_->Blocklist(form_digest);
  }
}

void PasswordSaveManagerImpl::Unblocklist(
    const PasswordFormDigest& form_digest) {
  // Try to unblocklist in both stores anyway because if credentials don't
  // exist, the unblocklist operation is no-op.
  profile_store_form_saver_->Unblocklist(form_digest);
  if (account_store_form_saver_ && IsOptedInForAccountStorage())
    account_store_form_saver_->Unblocklist(form_digest);
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
    metrics_util::MoveToAccountStoreTrigger trigger) {
  DCHECK(account_store_form_saver_);

  base::UmaHistogramEnumeration(
      "PasswordManager.AccountStorage.MoveToAccountStoreFlowAccepted2",
      trigger);

  // TODO(crbug.com/1032992): Moving credentials upon an update. FormFetch will
  // have an outdated credentials. Fix it if this turns out to be a product
  // requirement.

  std::vector<const PasswordForm*> account_store_matches =
      AccountStoreMatches(form_fetcher_->GetNonFederatedMatches());
  const std::vector<const PasswordForm*> account_store_federated_matches =
      AccountStoreMatches(form_fetcher_->GetFederatedMatches());
  account_store_matches.insert(account_store_matches.end(),
                               account_store_federated_matches.begin(),
                               account_store_federated_matches.end());

  std::vector<const PasswordForm*> profile_store_matches =
      ProfileStoreMatches(form_fetcher_->GetNonFederatedMatches());
  const std::vector<const PasswordForm*> profile_store_federated_matches =
      ProfileStoreMatches(form_fetcher_->GetFederatedMatches());
  profile_store_matches.insert(profile_store_matches.end(),
                               profile_store_federated_matches.begin(),
                               profile_store_federated_matches.end());

  for (const PasswordForm* match : profile_store_matches) {
    DCHECK(!match->IsUsingAccountStore());
    // Ignore credentials matches for other usernames.
    if (match->username_value != pending_credentials_.username_value)
      continue;

    // Don't call Save() if the credential already exists in the account
    // store, 1) to avoid unnecessary sync cycles, 2) to avoid potential
    // last_used_date update.
    if (!AccountStoreMatchesContainForm(account_store_matches, *match)) {
      PasswordForm match_copy = *match;
      match_copy.moving_blocked_for_list.clear();
      account_store_form_saver_->Save(match_copy, account_store_matches,
                                      /*old_password=*/std::u16string());
    }
    profile_store_form_saver_->Remove(*match);
  }
}

void PasswordSaveManagerImpl::BlockMovingToAccountStoreFor(
    const signin::GaiaIdHash& gaia_id_hash) {
  // TODO(crbug.com/1032992): This doesn't work if moving is offered upon update
  // prompts.

  // We offer moving credentials to the account store only upon successful
  // login. This entails that the credentials must exist in the profile store.
  PendingCredentialsStates states = ComputePendingCredentialsStates(
      pending_credentials_, form_fetcher_->GetAllRelevantMatches(),
      username_updated_in_bubble_);
  DCHECK(states.similar_saved_form_from_profile_store);
  DCHECK_EQ(PendingCredentialsState::EQUAL_TO_SAVED_MATCH,
            states.profile_store_state);

  // If the submitted credentials exists in both stores, .|pending_credentials_|
  // might be from the account store (and thus not have a
  // moving_blocked_for_list). We need to preserve any existing list, so
  // explicitly copy it over from the profile store match.
  PasswordForm form_to_block(pending_credentials_);
  form_to_block.moving_blocked_for_list =
      states.similar_saved_form_from_profile_store->moving_blocked_for_list;
  form_to_block.moving_blocked_for_list.push_back(gaia_id_hash);

  // No need to pass matches to Update(). It's only used for post processing
  // (e.g. updating the password for other credentials with the same
  // old password).
  profile_store_form_saver_->Update(form_to_block, /*matches=*/{},
                                    form_to_block.password_value);
}

void PasswordSaveManagerImpl::UpdateSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event) {
  pending_credentials_.form_data.submission_event = event;
  pending_credentials_.submission_event = event;
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
  auto result = std::make_unique<PasswordSaveManagerImpl>(
      profile_store_form_saver_->Clone(),
      account_store_form_saver_ ? account_store_form_saver_->Clone() : nullptr);
  CloneInto(result.get());
  return result;
}

PasswordForm PasswordSaveManagerImpl::BuildPendingCredentials(
    const PasswordForm& parsed_submitted_form,
    const FormData* observed_form,
    const FormData& submitted_form,
    bool is_http_auth,
    bool is_credential_api_save) {
  PasswordForm pending_credentials;
  AlternativeElement password_to_save(PasswordToSave(parsed_submitted_form));

  const PasswordForm* similar_saved_form = nullptr;
  std::tie(similar_saved_form, pending_credentials_state_) =
      FindSimilarSavedFormAndComputeState(parsed_submitted_form);

  switch (pending_credentials_state_) {
    case PendingCredentialsState::NEW_LOGIN:
      // No stored credentials can be matched to the submitted form. Offer to
      // save new credentials.
      pending_credentials = PendingCredentialsForNewCredentials(
          parsed_submitted_form, observed_form, password_to_save.name,
          is_http_auth, is_credential_api_save);
      break;
    case PendingCredentialsState::UPDATE:
      pending_credentials = *similar_saved_form;
      // Propagate heuristics decision on whether to show update bubble.
      pending_credentials.only_for_fallback =
          parsed_submitted_form.only_for_fallback;
      break;
    case PendingCredentialsState::EQUAL_TO_SAVED_MATCH:
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
      HasGeneratedPassword() ? generation_manager_->generated_password()
                             : password_to_save.value;
  pending_credentials.date_last_used = base::Time::Now();
  pending_credentials.form_has_autofilled_value =
      parsed_submitted_form.form_has_autofilled_value;
  pending_credentials.all_alternative_passwords =
      parsed_submitted_form.all_alternative_passwords;
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

  // Add previously saved usernames as alternatives.
  PopulateAlternativeUsernames(form_fetcher_->GetBestMatches(),
                               pending_credentials);

  if (HasGeneratedPassword()) {
    pending_credentials.type = PasswordForm::Type::kGenerated;
  }

  return pending_credentials;
}

std::pair<const PasswordForm*, PendingCredentialsState>
PasswordSaveManagerImpl::FindSimilarSavedFormAndComputeState(
    const PasswordForm& parsed_submitted_form) const {
  PendingCredentialsStates states = ComputePendingCredentialsStates(
      parsed_submitted_form, form_fetcher_->GetBestMatches(),
      username_updated_in_bubble_);

  // Resolve the two states to a single canonical one. This will be used to
  // decide what UI bubble (if any) to show to the user.
  PendingCredentialsState resolved_state = ResolvePendingCredentialsStates(
      states.profile_store_state, states.account_store_state);

  // Choose which of the saved forms (if any) to use as the base for updating,
  // based on which of the two states won the resolution.
  // Note that if we got the same state for both stores, then it doesn't really
  // matter which one we pick for updating, since the result will be the same
  // anyway.
  const PasswordForm* resolved_similar_saved_form = nullptr;
  if (resolved_state == states.profile_store_state)
    resolved_similar_saved_form = states.similar_saved_form_from_profile_store;
  else if (resolved_state == states.account_store_state)
    resolved_similar_saved_form = states.similar_saved_form_from_account_store;

  return std::make_pair(resolved_similar_saved_form, resolved_state);
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
  PendingCredentialsStates states = ComputePendingCredentialsStates(
      parsed_submitted_form, matches, username_updated_in_bubble_);

  auto account_matches = AccountStoreMatches(matches);
  auto profile_matches = ProfileStoreMatches(matches);

  std::u16string old_account_password =
      states.similar_saved_form_from_account_store
          ? states.similar_saved_form_from_account_store->password_value
          : std::u16string();
  std::u16string old_profile_password =
      states.similar_saved_form_from_profile_store
          ? states.similar_saved_form_from_profile_store->password_value
          : std::u16string();

  if (states.profile_store_state == PendingCredentialsState::NEW_LOGIN &&
      states.account_store_state == PendingCredentialsState::NEW_LOGIN) {
    // If the credential is new to both stores, store it only in the default
    // store.
    if (account_store_form_saver_ && AccountStoreIsDefault()) {
      // TODO(crbug.com/1012203): Record UMA for how many passwords get dropped
      // here. In rare cases it could happen that the user *was* opted in when
      // the save dialog was shown, but now isn't anymore.
      if (IsOptedInForAccountStorage()) {
        account_store_form_saver_->Save(pending_credentials_, account_matches,
                                        old_account_password);
      }
    } else {
      profile_store_form_saver_->Save(pending_credentials_, profile_matches,
                                      old_profile_password);
    }
    return;
  }

  switch (states.profile_store_state) {
    case PendingCredentialsState::AUTOMATIC_SAVE:
      profile_store_form_saver_->Save(pending_credentials_, profile_matches,
                                      old_profile_password);
      break;
    case PendingCredentialsState::UPDATE:
    case PendingCredentialsState::EQUAL_TO_SAVED_MATCH: {
      // If the submitted credentials exists in both stores,
      // |pending_credentials_| might be from the account store (and thus not
      // have a moving_blocked_for_list). We need to preserve any existing list.
      // Same applies for other fields. Check the comment on
      // UpdateFormPreservingDifferentFieldsAcrossStores().
      PasswordForm form_to_update =
          UpdateFormPreservingDifferentFieldsAcrossStores(
              *states.similar_saved_form_from_profile_store,
              pending_credentials_);
      // For other cases, |pending_credentials_.times_used_in_html_form| is
      // updated in UpdateMetadataForUsage() invoked from
      // UploadVotesAndMetrics().
      // UpdateFormPreservingDifferentFieldsAcrossStores() preserved the
      // original times_used_in_html_form, and hence we should increment it
      // here.
      if (form_to_update.scheme == PasswordForm::Scheme::kHtml) {
        form_to_update.times_used_in_html_form++;
      }
      // Password saving is mostly a result of a user action interacting with
      // the password, (e.g. using a password to sign-in, results in updating
      // the last_used_timestamp). Since the user interacts with the password,
      // this counts as the user has been notified of the shared password and no
      // need to display further notifications to the user.
      if (form_to_update.type == PasswordForm::Type::kReceivedViaSharing) {
        form_to_update.sharing_notification_displayed = true;
      }
      profile_store_form_saver_->Update(form_to_update, profile_matches,
                                        old_profile_password);
    } break;
    // The NEW_LOGIN case was already handled separately above.
    case PendingCredentialsState::NEW_LOGIN:
    case PendingCredentialsState::NONE:
      break;
  }

  // TODO(crbug.com/1012203): Record UMA for how many passwords get dropped
  // here. In rare cases it could happen that the user *was* opted in when
  // the save dialog was shown, but now isn't anymore.
  if (account_store_form_saver_ && IsOptedInForAccountStorage()) {
    switch (states.account_store_state) {
      case PendingCredentialsState::AUTOMATIC_SAVE:
        account_store_form_saver_->Save(pending_credentials_, account_matches,
                                        old_account_password);
        break;
      case PendingCredentialsState::UPDATE:
      case PendingCredentialsState::EQUAL_TO_SAVED_MATCH: {
        // If the submitted credentials exists in both stores,
        // .|pending_credentials_| might be from the profile store (and thus
        // has a moving_blocked_for_list). We need to preserve any existing
        // values. Same applies for other fields. Check the comment on
        // UpdateFormPreservingDifferentFieldsAcrossStores().
        PasswordForm form_to_update =
            UpdateFormPreservingDifferentFieldsAcrossStores(
                *states.similar_saved_form_from_account_store,
                pending_credentials_);
        // For other cases, |pending_credentials_.times_used_in_html_form| is
        // updated in UpdateMetadataForUsage() invoked from
        // UploadVotesAndMetrics().
        // UpdateFormPreservingDifferentFieldsAcrossStores() preserved the
        // original times_used_in_html_form, and hence we should increment it
        // here.
        if (form_to_update.scheme == PasswordForm::Scheme::kHtml) {
          form_to_update.times_used_in_html_form++;
        }
        account_store_form_saver_->Update(form_to_update, account_matches,
                                          old_account_password);
      } break;
      // The NEW_LOGIN case was already handled separately above.
      case PendingCredentialsState::NEW_LOGIN:
      case PendingCredentialsState::NONE:
        break;
    }
  }
}

std::u16string PasswordSaveManagerImpl::GetOldPassword(
    const PasswordForm& parsed_submitted_form) const {
  const PasswordForm* similar_saved_form =
      FindSimilarSavedFormAndComputeState(parsed_submitted_form).first;
  return similar_saved_form ? similar_saved_form->password_value
                            : std::u16string();
}

void PasswordSaveManagerImpl::UploadVotesAndMetrics(
    const FormData* observed_form,
    const PasswordForm& parsed_submitted_form) {
  metrics_util::LogPasswordAcceptedSaveUpdateSubmissionIndicatorEvent(
      parsed_submitted_form.submission_event);
  metrics_recorder_->SetSubmissionIndicatorEvent(
      parsed_submitted_form.submission_event);
  // TODO(crbug.com/959776): Get rid of this method, by passing
  // |pending_credentials_| directly to MaybeSendSingleUsernameVotes.
  votes_uploader_->CalculateUsernamePromptEditState(
      /*saved_username=*/pending_credentials_.username_value);

  if (IsNewLogin()) {
    metrics_util::LogNewlySavedPasswordMetrics(
        pending_credentials_.type == PasswordForm::Type::kGenerated,
        pending_credentials_.username_value.empty(),
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
  CHECK(!client_->IsOffTheRecord());

  password_manager_util::UpdateMetadataForUsage(&pending_credentials_);

  // Check to see if this form is a candidate for password generation.
  // Do not send votes if there was no observed form. Furthermore, don't send
  // votes on change password forms, since they were already sent in Update()
  // method.
  if (observed_form && !parsed_submitted_form.HasNewPasswordElement()) {
    votes_uploader_->SendVoteOnCredentialsReuse(
        *observed_form, parsed_submitted_form, &pending_credentials_);
  }
  if (IsPasswordUpdate()) {
    votes_uploader_->MaybeSendSingleUsernameVotes();
    votes_uploader_->UploadPasswordVote(
        parsed_submitted_form, parsed_submitted_form, autofill::NEW_PASSWORD,
        base::NumberToString(
            *autofill::CalculateFormSignature(pending_credentials_.form_data)));
  }

  if (pending_credentials_.times_used_in_html_form == 1) {
    votes_uploader_->UploadFirstLoginVotes(form_fetcher_->GetBestMatches(),
                                           pending_credentials_,
                                           parsed_submitted_form);
  }
}

FormSaver* PasswordSaveManagerImpl::GetFormSaverForGeneration() {
  return (ShouldStoreGeneratedPasswordsInAccountStore())
             ? account_store_form_saver_.get()
             : profile_store_form_saver_.get();
}

std::vector<const PasswordForm*>
PasswordSaveManagerImpl::GetRelevantMatchesForGeneration(
    const std::vector<const PasswordForm*>& matches) {
  //  For account store users, only matches in the account store should be
  //  considered for conflict resolution during generation.
  return (ShouldStoreGeneratedPasswordsInAccountStore())
             ? MatchesInStore(matches, PasswordForm::Store::kAccountStore)
             : matches;
}

void PasswordSaveManagerImpl::CloneInto(PasswordSaveManagerImpl* clone) {
  DCHECK(clone);
  if (generation_manager_)
    clone->generation_manager_ = generation_manager_->Clone();

  clone->pending_credentials_ = pending_credentials_;
  clone->pending_credentials_state_ = pending_credentials_state_;
}

bool PasswordSaveManagerImpl::IsOptedInForAccountStorage() const {
  return client_->GetPasswordFeatureManager()->IsOptedInForAccountStorage();
}

bool PasswordSaveManagerImpl::AccountStoreIsDefault() const {
  return client_->GetPasswordFeatureManager()->GetDefaultPasswordStore() ==
         PasswordForm::Store::kAccountStore;
}

bool PasswordSaveManagerImpl::ShouldStoreGeneratedPasswordsInAccountStore()
    const {
  if (account_store_form_saver_ &&
      client_->GetPasswordFeatureManager()
              ->ComputePasswordAccountStorageUsageLevel() ==
          metrics_util::PasswordAccountStorageUsageLevel::
              kUsingAccountStorage) {
    return true;
  }
  return false;
}

void PasswordSaveManagerImpl::UsernameUpdatedInBubble() {
  username_updated_in_bubble_ = true;
}

}  // namespace password_manager
