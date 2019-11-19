// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/possible_username_data.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::PasswordForm;
using autofill::ValueElementPair;
using base::TimeDelta;
using base::TimeTicks;

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

bool PasswordFormManager::wait_for_server_predictions_for_filling_ = true;

namespace {

constexpr TimeDelta kMaxFillingDelayForServerPredictions =
    TimeDelta::FromMilliseconds(500);

// Helper to get the platform specific identifier by which autofill and password
// manager refer to a field. See http://crbug.com/896594
base::string16 GetPlatformSpecificIdentifier(const FormFieldData& field) {
#if defined(OS_IOS)
  return field.unique_id;
#else
  return field.name;
#endif
}

ValueElementPair PasswordToSave(const PasswordForm& form) {
  if (form.new_password_value.empty()) {
    DCHECK(!form.password_value.empty() || form.IsFederatedCredential());
    return {form.password_value, form.password_element};
  }
  return {form.new_password_value, form.new_password_element};
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
            : autofill::FieldPropertiesFlags::ERROR_OCCURRED;
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

// Returns bit masks with differences in forms attributes which are important
// for parsing. Bits are set according to enum FormDataDifferences.
uint32_t FindFormsDifferences(const FormData& lhs, const FormData& rhs) {
  if (lhs.fields.size() != rhs.fields.size())
    return PasswordFormMetricsRecorder::kFieldsNumber;
  size_t differences_bitmask = 0;
  for (size_t i = 0; i < lhs.fields.size(); ++i) {
    const FormFieldData& lhs_field = lhs.fields[i];
    const FormFieldData& rhs_field = rhs.fields[i];

    if (lhs_field.unique_renderer_id != rhs_field.unique_renderer_id)
      differences_bitmask |= PasswordFormMetricsRecorder::kRendererFieldIDs;

    if (lhs_field.form_control_type != rhs_field.form_control_type)
      differences_bitmask |= PasswordFormMetricsRecorder::kFormControlTypes;

    if (lhs_field.autocomplete_attribute != rhs_field.autocomplete_attribute)
      differences_bitmask |=
          PasswordFormMetricsRecorder::kAutocompleteAttributes;
  }
  return differences_bitmask;
}

bool FormContainsFieldWithName(const FormData& form,
                               const base::string16& element) {
  if (element.empty())
    return false;
  for (const auto& field : form.fields) {
    if (base::EqualsCaseInsensitiveASCII(field.name, element))
      return true;
  }
  return false;
}

bool IsUsernameFirstFlowFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kUsernameFirstFlow);
}

}  // namespace

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const FormData& observed_form,
    FormFetcher* form_fetcher,
    std::unique_ptr<FormSaver> form_saver,
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder)
    : PasswordFormManager(client,
                          form_fetcher,
                          std::move(form_saver),
                          metrics_recorder,
                          PasswordStore::FormDigest(observed_form)) {
  driver_ = driver;
  if (driver_)
    driver_id_ = driver->GetId();

  observed_form_ = observed_form;
  metrics_recorder_->RecordFormSignature(CalculateFormSignature(observed_form));
  // Do not fetch saved credentials for Chrome sync form, since nor filling nor
  // saving are supported.
  if (owned_form_fetcher_ &&
      !observed_form_.is_gaia_with_skip_save_password_form) {
    owned_form_fetcher_->Fetch();
  }
  form_fetcher_->AddConsumer(this);
  votes_uploader_.StoreInitialFieldValues(observed_form);
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    PasswordStore::FormDigest observed_http_auth_digest,
    FormFetcher* form_fetcher,
    std::unique_ptr<FormSaver> form_saver)
    : PasswordFormManager(client,
                          form_fetcher,
                          std::move(form_saver),
                          nullptr /* metrics_recorder */,
                          observed_http_auth_digest) {
  observed_not_web_form_digest_ = std::move(observed_http_auth_digest);
  if (owned_form_fetcher_)
    owned_form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);
}

PasswordFormManager::~PasswordFormManager() {
  form_fetcher_->RemoveConsumer(this);
}

bool PasswordFormManager::DoesManage(
    const FormData& form,
    const PasswordManagerDriver* driver) const {
  if (driver != driver_.get())
    return false;

  if (observed_form_.is_form_tag != form.is_form_tag)
    return false;
  // All unowned input elements are considered as one synthetic form.
  if (!observed_form_.is_form_tag && !form.is_form_tag)
    return true;
#if defined(OS_IOS)
  // On iOS form name is used as the form identifier.
  return observed_form_.name == form.name;
#else
  return observed_form_.unique_renderer_id == form.unique_renderer_id;
#endif
}

bool PasswordFormManager::DoesManageAccordingToRendererId(
    uint32_t form_renderer_id,
    const PasswordManagerDriver* driver) const {
  if (driver != driver_.get())
    return false;
#if defined(OS_IOS)
  NOTREACHED();
  // On iOS form name is used as the form identifier.
  return false;
#else
  return observed_form_.unique_renderer_id == form_renderer_id;
#endif
}

bool PasswordFormManager::IsEqualToSubmittedForm(
    const autofill::FormData& form) const {
  if (!is_submitted_)
    return false;
  if (IsHttpAuth())
    return false;

  if (form.action.is_valid() && !form.is_action_empty &&
      !submitted_form_.is_action_empty &&
      submitted_form_.action == form.action) {
    return true;
  }

  // Match the form if username and password fields are same.
  if (FormContainsFieldWithName(form,
                                parsed_submitted_form_->username_element) &&
      FormContainsFieldWithName(form,
                                parsed_submitted_form_->password_element)) {
    return true;
  }

  // Match the form if the observed username field has the same value as in
  // the submitted form.
  if (!parsed_submitted_form_->username_value.empty()) {
    for (const auto& field : form.fields) {
      if (field.value == parsed_submitted_form_->username_value)
        return true;
    }
  }
  return false;
}

const GURL& PasswordFormManager::GetOrigin() const {
  return observed_not_web_form_digest_ ? observed_not_web_form_digest_->origin
                                       : observed_form_.url;
}

const std::vector<const PasswordForm*>& PasswordFormManager::GetBestMatches()
    const {
  return form_fetcher_->GetBestMatches();
}

std::vector<const autofill::PasswordForm*>
PasswordFormManager::GetFederatedMatches() const {
  return form_fetcher_->GetFederatedMatches();
}

const PasswordForm& PasswordFormManager::GetPendingCredentials() const {
  return pending_credentials_;
}

metrics_util::CredentialSourceType PasswordFormManager::GetCredentialSource()
    const {
  return metrics_util::CredentialSourceType::kPasswordManager;
}

PasswordFormMetricsRecorder* PasswordFormManager::GetMetricsRecorder() {
  return metrics_recorder_.get();
}

base::span<const InteractionsStats> PasswordFormManager::GetInteractionsStats()
    const {
  return base::make_span(form_fetcher_->GetInteractionsStats());
}

bool PasswordFormManager::IsBlacklisted() const {
  return form_fetcher_->IsBlacklisted() || newly_blacklisted_;
}

void PasswordFormManager::Save() {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  DCHECK(!client_->IsIncognito());
  if (IsBlacklisted()) {
    form_saver_->Unblacklist(ConstructObservedFormDigest());
    newly_blacklisted_ = false;
  }

  if (IsPasswordUpdate() &&
      pending_credentials_.type == PasswordForm::Type::kGenerated &&
      !HasGeneratedPassword()) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_OVERRIDDEN);
    pending_credentials_.type = PasswordForm::Type::kManual;
  }

  if (IsNewLogin()) {
    metrics_util::LogNewlySavedPasswordIsGenerated(
        pending_credentials_.type == PasswordForm::Type::kGenerated);
    SanitizePossibleUsernames(&pending_credentials_);
    pending_credentials_.date_created = base::Time::Now();
    votes_uploader_.SendVotesOnSave(observed_form_, *parsed_submitted_form_,
                                    GetBestMatches(), &pending_credentials_);
    SavePendingToStore(false /*update*/);
  } else {
    // It sounds wrong that we still update even if the
    // |pending_credentials_state_| is NONE. We should double check if this
    // actually necessary. Currently some tests depend on this behavior.
    ProcessUpdate();
    SavePendingToStore(true /*update*/);
  }

  if (pending_credentials_.times_used == 1 &&
      pending_credentials_.type == PasswordForm::Type::kGenerated) {
    // This also includes PSL matched credentials.
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_USED);
  }

  client_->UpdateFormManagers();
}

void PasswordFormManager::Update(const PasswordForm& credentials_to_update) {
  metrics_util::LogPasswordAcceptedSaveUpdateSubmissionIndicatorEvent(
      parsed_submitted_form_->submission_event);
  metrics_recorder_->SetSubmissionIndicatorEvent(
      parsed_submitted_form_->submission_event);

  base::string16 password_to_save = pending_credentials_.password_value;
  bool skip_zero_click = pending_credentials_.skip_zero_click;
  pending_credentials_ = credentials_to_update;
  pending_credentials_.password_value = password_to_save;
  pending_credentials_.skip_zero_click = skip_zero_click;
  pending_credentials_.preferred = true;
  pending_credentials_.date_last_used = base::Time::Now();
  pending_credentials_state_ = PendingCredentialsState::UPDATE;
  ProcessUpdate();
  SavePendingToStore(true /*update*/);

  client_->UpdateFormManagers();
}

void PasswordFormManager::OnUpdateUsernameFromPrompt(
    const base::string16& new_username) {
  DCHECK(parsed_submitted_form_);
  parsed_submitted_form_->username_value = new_username;
  parsed_submitted_form_->username_element.clear();

  metrics_recorder_->set_username_updated_in_bubble(true);

  // |has_username_edited_vote_| is true iff |new_username| was typed in another
  // field. Otherwise, |has_username_edited_vote_| is false and no vote will be
  // uploaded.
  votes_uploader_.set_has_username_edited_vote(false);
  if (!new_username.empty()) {
    // |all_possible_usernames| has all possible usernames.
    // TODO(crbug.com/831123): rename to |all_possible_usernames| when the old
    // parser is gone.
    for (const auto& possible_username :
         parsed_submitted_form_->all_possible_usernames) {
      if (possible_username.first == new_username) {
        parsed_submitted_form_->username_element = possible_username.second;
        votes_uploader_.set_has_username_edited_vote(true);
        break;
      }
    }
  }

  CreatePendingCredentials();
}

void PasswordFormManager::OnUpdatePasswordFromPrompt(
    const base::string16& new_password) {
  DCHECK(parsed_submitted_form_);
  parsed_submitted_form_->password_value = new_password;
  parsed_submitted_form_->password_element.clear();

  // The user updated a password from the prompt. It means that heuristics were
  // wrong. So clear new password, since it is likely wrong.
  parsed_submitted_form_->new_password_value.clear();
  parsed_submitted_form_->new_password_element.clear();

  for (const ValueElementPair& pair :
       parsed_submitted_form_->all_possible_passwords) {
    if (pair.first == new_password) {
      parsed_submitted_form_->password_element = pair.second;
      break;
    }
  }

  CreatePendingCredentials();
}

void PasswordFormManager::UpdateSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event) {
  parsed_submitted_form_->form_data.submission_event = event;
  parsed_submitted_form_->submission_event = event;
}

void PasswordFormManager::OnNopeUpdateClicked() {
  votes_uploader_.UploadPasswordVote(*parsed_submitted_form_,
                                     *parsed_submitted_form_,
                                     autofill::NOT_NEW_PASSWORD, std::string());
}

void PasswordFormManager::OnNeverClicked() {
  // |UNKNOWN_TYPE| is sent in order to record that a generation popup was
  // shown and ignored.
  votes_uploader_.UploadPasswordVote(*parsed_submitted_form_,
                                     *parsed_submitted_form_,
                                     autofill::UNKNOWN_TYPE, std::string());

  votes_uploader_.MaybeSendSingleUsernameVote(false /* credentials_saved */);
  PermanentlyBlacklist();
}

void PasswordFormManager::OnNoInteraction(bool is_update) {
  // |UNKNOWN_TYPE| is sent in order to record that a generation popup was
  // shown and ignored.
  votes_uploader_.UploadPasswordVote(
      *parsed_submitted_form_, *parsed_submitted_form_,
      is_update ? autofill::PROBABLY_NEW_PASSWORD : autofill::UNKNOWN_TYPE,
      std::string());

  votes_uploader_.MaybeSendSingleUsernameVote(false /* credentials_saved */);
}

void PasswordFormManager::PermanentlyBlacklist() {
  DCHECK(!client_->IsIncognito());
  form_saver_->PermanentlyBlacklist(ConstructObservedFormDigest());
  newly_blacklisted_ = true;
}

PasswordStore::FormDigest PasswordFormManager::ConstructObservedFormDigest() {
  std::string signon_realm;
  GURL origin;
  if (observed_not_web_form_digest_) {
    origin = observed_not_web_form_digest_->origin;
    // GetSignonRealm is not suitable for http auth credentials.
    signon_realm = IsHttpAuth()
                       ? observed_not_web_form_digest_->signon_realm
                       : GetSignonRealm(observed_not_web_form_digest_->origin);
  } else {
    origin = observed_form_.url;
    signon_realm = GetSignonRealm(observed_form_.url);
  }
  return PasswordStore::FormDigest(GetScheme(), signon_realm, origin);
}

void PasswordFormManager::OnPasswordsRevealed() {
  votes_uploader_.set_has_passwords_revealed_vote(true);
}

bool PasswordFormManager::IsNewLogin() const {
  return pending_credentials_state_ == PendingCredentialsState::NEW_LOGIN ||
         pending_credentials_state_ == PendingCredentialsState::AUTOMATIC_SAVE;
}

FormFetcher* PasswordFormManager::GetFormFetcher() {
  return form_fetcher_;
}

bool PasswordFormManager::IsPendingCredentialsPublicSuffixMatch() const {
  return pending_credentials_.is_public_suffix_match;
}

void PasswordFormManager::PresaveGeneratedPassword(const PasswordForm& form) {
  // TODO(https://crbug.com/831123): Propagate generated password independently
  // of PasswordForm when PasswordForm goes away from the renderer process.
  PresaveGeneratedPasswordInternal(form.form_data,
                                   form.password_value /*generated_password*/);
}

void PasswordFormManager::PasswordNoLongerGenerated() {
  if (!HasGeneratedPassword())
    return;

  generation_manager_->PasswordNoLongerGenerated();
  generation_manager_.reset();
  votes_uploader_.set_has_generated_password(false);
  votes_uploader_.set_generated_password_changed(false);
  metrics_recorder_->SetGeneratedPasswordStatus(
      PasswordFormMetricsRecorder::GeneratedPasswordStatus::kPasswordDeleted);
}

bool PasswordFormManager::HasGeneratedPassword() const {
  return generation_manager_ && generation_manager_->HasGeneratedPassword();
}

void PasswordFormManager::SetGenerationPopupWasShown(
    bool is_manual_generation) {
  votes_uploader_.set_generation_popup_was_shown(true);
  votes_uploader_.set_is_manual_generation(is_manual_generation);
  metrics_recorder_->SetPasswordGenerationPopupShown(true,
                                                     is_manual_generation);
}

void PasswordFormManager::SetGenerationElement(
    const base::string16& generation_element) {
  votes_uploader_.set_generation_element(generation_element);
}

bool PasswordFormManager::IsPossibleChangePasswordFormWithoutUsername() const {
  return parsed_submitted_form_ &&
         parsed_submitted_form_->IsPossibleChangePasswordFormWithoutUsername();
}

bool PasswordFormManager::IsPasswordUpdate() const {
  return pending_credentials_state_ == PendingCredentialsState::UPDATE;
}

base::WeakPtr<PasswordManagerDriver> PasswordFormManager::GetDriver() const {
  return driver_;
}

const PasswordForm* PasswordFormManager::GetSubmittedForm() const {
  return parsed_submitted_form_.get();
}

#if defined(OS_IOS)
void PasswordFormManager::PresaveGeneratedPassword(
    PasswordManagerDriver* driver,
    const FormData& form,
    const base::string16& generated_password,
    const base::string16& generation_element) {
  observed_form_ = form;
  PresaveGeneratedPasswordInternal(form, generated_password);
  votes_uploader_.set_generation_element(generation_element);
}

bool PasswordFormManager::UpdateGeneratedPasswordOnUserInput(
    const base::string16& form_identifier,
    const base::string16& field_identifier,
    const base::string16& field_value) {
  if (observed_form_.name != form_identifier || !HasGeneratedPassword()) {
    // *this might not have generated password, because
    // 1.This function is called before PresaveGeneratedPassword, or
    // 2.There are multiple forms with the same |form_identifier|
    return false;
  }
  bool form_data_changed = false;
  for (FormFieldData& field : observed_form_.fields) {
    if (field.unique_id == field_identifier) {
      field.value = field_value;
      form_data_changed = true;
      break;
    }
  }
  base::string16 generated_password = generation_manager_->generated_password();
  if (votes_uploader_.get_generation_element() == field_identifier) {
    generated_password = field_value;
    form_data_changed = true;
  }
  if (form_data_changed)
    PresaveGeneratedPasswordInternal(observed_form_, generated_password);
  return true;
}
#endif  // defined(OS_IOS)

std::unique_ptr<PasswordFormManager> PasswordFormManager::Clone() {
  // Fetcher is cloned to avoid re-fetching data from PasswordStore.
  std::unique_ptr<FormFetcher> fetcher = form_fetcher_->Clone();

  // Some data is filled through the constructor. No PasswordManagerDriver is
  // needed, because the UI does not need any functionality related to the
  // renderer process, to which the driver serves as an interface. The full
  // |observed_form_| needs to be copied, because it is used to create the
  // blacklisting entry if needed.
  auto result = std::make_unique<PasswordFormManager>(
      client_, base::WeakPtr<PasswordManagerDriver>(), observed_form_,
      fetcher.get(), form_saver_->Clone(), metrics_recorder_);

  // The constructor only can take a weak pointer to the fetcher, so moving the
  // owning one needs to happen explicitly.
  result->owned_form_fetcher_ = std::move(fetcher);

  if (generation_manager_) {
    result->generation_manager_ =
        generation_manager_->Clone(result->form_saver_.get());
  }

  // These data members all satisfy:
  //   (1) They could have been changed by |*this| between its construction and
  //       calling Clone().
  //   (2) They are potentially used in the clone as the clone is used in the UI
  //       code.
  //   (3) They are not changed during OnFetchCompleted, triggered at some point
  //   by the
  //       cloned FormFetcher.
  result->votes_uploader_ = votes_uploader_;
  if (parser_.predictions())
    result->parser_.set_predictions(*parser_.predictions());

  result->pending_credentials_ = pending_credentials_;
  if (parsed_submitted_form_) {
    result->parsed_submitted_form_.reset(
        new PasswordForm(*parsed_submitted_form_));
  }
  result->pending_credentials_state_ = pending_credentials_state_;
  result->is_submitted_ = is_submitted_;

  return result;
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    std::unique_ptr<PasswordForm> saved_form,
    std::unique_ptr<FormFetcher> form_fetcher,
    std::unique_ptr<FormSaver> form_saver)
    : PasswordFormManager(client,
                          form_fetcher.get(),
                          std::move(form_saver),
                          nullptr /* metrics_recorder */,
                          PasswordStore::FormDigest(*saved_form)) {
  observed_not_web_form_digest_ = PasswordStore::FormDigest(*saved_form);
  parsed_submitted_form_ = std::move(saved_form);
  is_submitted_ = true;
  owned_form_fetcher_ = std::move(form_fetcher),
  form_fetcher_->AddConsumer(this);
  if (form_fetcher_)
    form_fetcher_->Fetch();
}

void PasswordFormManager::OnFetchCompleted() {
  received_stored_credentials_time_ = TimeTicks::Now();

  // Copy out blacklisted matches.
  newly_blacklisted_ = false;
  autofills_left_ = kMaxTimesAutofill;

  if (IsCredentialAPISave()) {
    // This is saving with credential API, there is no form to fill, so no
    // filling required.
    return;
  }

  if (IsHttpAuth()) {
    // No server prediction for http auth, so no need to wait.
    FillHttpAuth();
  } else if (parser_.predictions() ||
             !wait_for_server_predictions_for_filling_) {
    ReportTimeBetweenStoreAndServerUMA();
    Fill();
  } else if (!waiting_for_server_predictions_) {
    waiting_for_server_predictions_ = true;
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PasswordFormManager::Fill,
                       weak_ptr_factory_.GetWeakPtr()),
        kMaxFillingDelayForServerPredictions);
  }
}

bool PasswordFormManager::ProvisionallySave(
    const FormData& submitted_form,
    const PasswordManagerDriver* driver,
    const PossibleUsernameData* possible_username) {
  DCHECK(DoesManage(submitted_form, driver));

  std::unique_ptr<PasswordForm> parsed_submitted_form =
      ParseFormAndMakeLogging(submitted_form, FormDataParser::Mode::kSaving);

  RecordMetricOnReadonly(parser_.readonly_status(), !!parsed_submitted_form,
                         FormDataParser::Mode::kSaving);

  // This function might be called multiple times. Consider as success if the
  // submitted form was successfully parsed on a previous call.
  if (!parsed_submitted_form)
    return is_submitted_;

  parsed_submitted_form_ = std::move(parsed_submitted_form);
  submitted_form_ = submitted_form;
  is_submitted_ = true;
  CalculateFillingAssistanceMetric(submitted_form);
  metrics_recorder_->set_possible_username_used(false);
  votes_uploader_.clear_single_username_vote_data();

  if (IsUsernameFirstFlowFeatureEnabled() &&
      parsed_submitted_form_->username_value.empty() && possible_username &&
      IsPossibleUsernameValid(*possible_username,
                              parsed_submitted_form_->signon_realm,
                              base::Time::Now())) {
    parsed_submitted_form_->username_value = possible_username->value;
    metrics_recorder_->set_possible_username_used(true);
    if (possible_username->form_predictions) {
      votes_uploader_.set_single_username_vote_data(
          possible_username->renderer_id, *possible_username->form_predictions);
    }
  }
  CreatePendingCredentials();
  return true;
}

bool PasswordFormManager::ProvisionallySaveHttpAuthForm(
    const PasswordForm& submitted_form) {
  if (!IsHttpAuth())
    return false;
  if (!(*observed_not_web_form_digest_ ==
        PasswordStore::FormDigest(submitted_form)))
    return false;

  parsed_submitted_form_.reset(new PasswordForm(submitted_form));
  is_submitted_ = true;
  CreatePendingCredentials();
  return true;
}

bool PasswordFormManager::IsHttpAuth() const {
  return GetScheme() != PasswordForm::Scheme::kHtml;
}

bool PasswordFormManager::IsCredentialAPISave() const {
  return observed_not_web_form_digest_ && !IsHttpAuth();
}

PasswordForm::Scheme PasswordFormManager::GetScheme() const {
  return observed_not_web_form_digest_ ? observed_not_web_form_digest_->scheme
                                       : PasswordForm::Scheme::kHtml;
}

void PasswordFormManager::ProcessServerPredictions(
    const std::map<FormSignature, FormPredictions>& predictions) {
  if (parser_.predictions()) {
    // This method might be called multiple times. No need to process
    // predictions again.
    return;
  }
  FormSignature observed_form_signature =
      CalculateFormSignature(observed_form_);
  auto it = predictions.find(observed_form_signature);
  if (it == predictions.end())
    return;

  ReportTimeBetweenStoreAndServerUMA();
  parser_.set_predictions(it->second);
  Fill();
}

void PasswordFormManager::Fill() {
  if (!driver_)
    return;

  waiting_for_server_predictions_ = false;

  if (form_fetcher_->GetState() == FormFetcher::State::WAITING)
    return;

  if (autofills_left_ <= 0)
    return;
  autofills_left_--;

  // There are additional signals (server-side data) and parse results in
  // filling and saving mode might be different so it is better not to cache
  // parse result, but to parse each time again.
  std::unique_ptr<PasswordForm> observed_password_form =
      ParseFormAndMakeLogging(observed_form_, FormDataParser::Mode::kFilling);
  RecordMetricOnReadonly(parser_.readonly_status(), !!observed_password_form,
                         FormDataParser::Mode::kFilling);
  if (!observed_password_form)
    return;

  if (observed_password_form->is_new_password_reliable && !IsBlacklisted()) {
#if defined(OS_IOS)
    driver_->FormEligibleForGenerationFound(
        {/*form_name*/ observed_password_form->form_data.name,
         /*new_password_element*/ observed_password_form->new_password_element,
         /*confirmation_password_element*/
         observed_password_form->confirmation_password_element});
#else
    driver_->FormEligibleForGenerationFound(
        {/*new_password_renderer_id*/
         observed_password_form->new_password_element_renderer_id,
         /*confirmation_password_renderer_id*/
         observed_password_form->confirmation_password_element_renderer_id});
#endif
  }

#if defined(OS_IOS)
  // Filling on username first flow is not supported on iOS.
  if (observed_password_form->IsSingleUsername())
    return;
#endif

  SendFillInformationToRenderer(
      client_, driver_.get(), *observed_password_form.get(),
      form_fetcher_->GetBestMatches(), form_fetcher_->GetFederatedMatches(),
      form_fetcher_->GetPreferredMatch(), metrics_recorder_.get());
}

void PasswordFormManager::FillForm(const FormData& observed_form) {
  uint32_t differences_bitmask =
      FindFormsDifferences(observed_form_, observed_form);
  metrics_recorder_->RecordFormChangeBitmask(differences_bitmask);

  if (differences_bitmask)
    observed_form_ = observed_form;

  if (!waiting_for_server_predictions_)
    Fill();
}

void PasswordFormManager::OnGeneratedPasswordAccepted(
    FormData form_data,
    uint32_t generation_element_id,
    const base::string16& password) {
  // Find the generating element to update its value. The parser needs a non
  // empty value.
  auto it = std::find_if(form_data.fields.begin(), form_data.fields.end(),
                         [generation_element_id](const auto& field_data) {
                           return generation_element_id ==
                                  field_data.unique_renderer_id;
                         });
  DCHECK(it != form_data.fields.end());
  it->value = password;
  std::unique_ptr<PasswordForm> parsed_form =
      ParseFormAndMakeLogging(form_data, FormDataParser::Mode::kSaving);
  if (!parsed_form) {
    // Create a password form with a minimum data.
    parsed_form.reset(new PasswordForm);
    parsed_form->origin = form_data.url;
    parsed_form->signon_realm = GetSignonRealm(form_data.url);
  }
  parsed_form->password_value = password;
  generation_manager_ =
      std::make_unique<PasswordGenerationManager>(form_saver_.get(), client_);
  generation_manager_->GeneratedPasswordAccepted(*parsed_form, *form_fetcher_,
                                                 driver_);
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    FormFetcher* form_fetcher,
    std::unique_ptr<FormSaver> form_saver,
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder,
    PasswordStore::FormDigest form_digest)
    : client_(client),
      metrics_recorder_(metrics_recorder),
      owned_form_fetcher_(form_fetcher
                              ? nullptr
                              : FormFetcherImpl::CreateFormFetcherImpl(
                                    std::move(form_digest),
                                    client_,
                                    true /* should_migrate_http_passwords */)),
      form_fetcher_(form_fetcher ? form_fetcher : owned_form_fetcher_.get()),
      form_saver_(std::move(form_saver)),
      // TODO(https://crbug.com/831123): set correctly
      // |is_possible_change_password_form| in |votes_uploader_| constructor
      votes_uploader_(client, false /* is_possible_change_password_form */) {
  if (!metrics_recorder_) {
    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        client_->IsMainFrameSecure(), client_->GetUkmSourceId());
  }
}

void PasswordFormManager::RecordMetricOnReadonly(
    FormDataParser::ReadonlyPasswordFields readonly_status,
    bool parsing_successful,
    FormDataParser::Mode mode) {
  // The reported value is combined of the |readonly_status| shifted by one bit
  // to the left, and the success bit put in the least significant bit. Note:
  // C++ guarantees that bool->int conversions map false to 0 and true to 1.
  uint64_t value = static_cast<uint64_t>(parsing_successful) +
                   (static_cast<uint64_t>(readonly_status) << 1);
  switch (mode) {
    case FormDataParser::Mode::kSaving:
      metrics_recorder_->RecordReadonlyWhenSaving(value);
      break;
    case FormDataParser::Mode::kFilling:
      metrics_recorder_->RecordReadonlyWhenFilling(value);
      break;
  }
}

void PasswordFormManager::ReportTimeBetweenStoreAndServerUMA() {
  if (!received_stored_credentials_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("PasswordManager.TimeBetweenStoreAndServer",
                        TimeTicks::Now() - received_stored_credentials_time_);
  }
}

// TODO(https://crbug.com/831123): move this function to the proper place
// corresponding to its place in the header.
void PasswordFormManager::CreatePendingCredentials() {
  DCHECK(is_submitted_);
  // TODO(https://crbug.com/831123): Process correctly the case when saved
  // credentials are not received from the store yet.
  if (!parsed_submitted_form_)
    return;

  // Calculate the user's action based on existing matches and the submitted
  // form.
  metrics_recorder_->CalculateUserAction(GetBestMatches(),
                                         *parsed_submitted_form_);

  // This function might be called multiple times so set variables that are
  // changed in this function to initial states.
  pending_credentials_state_ = PendingCredentialsState::NONE;
  votes_uploader_.set_password_overridden(false);

  ValueElementPair password_to_save(PasswordToSave(*parsed_submitted_form_));
  // Look for the actually submitted credentials in the list of previously saved
  // credentials that were available to autofilling.
  const PasswordForm* saved_form = password_manager_util::GetMatchForUpdating(
      *parsed_submitted_form_, GetBestMatches());
  if (saved_form) {
    // A similar credential exists in the store already. We should either update
    // the password or create an new record if it's a PSL-matched credentials.
    pending_credentials_ = *saved_form;
    if (pending_credentials_.password_value != password_to_save.first) {
      // Password should be updated.
      pending_credentials_state_ = PendingCredentialsState::UPDATE;
      votes_uploader_.set_password_overridden(true);
    } else if (pending_credentials_.is_public_suffix_match) {
      // If the autofilled credentials were a PSL match, store a copy with the
      // current origin and signon realm. This ensures that on the next visit, a
      // precise match is found.
      pending_credentials_state_ = PendingCredentialsState::AUTOMATIC_SAVE;
      // Update credential to reflect that it has been used for submission.
      // If this isn't updated, then password generation uploads are off for
      // sites where PSL matching is required to fill the login form, as two
      // PASSWORD votes are uploaded per saved password instead of one.
      password_manager_util::UpdateMetadataForUsage(&pending_credentials_);

      // Update |pending_credentials_| in order to be able correctly save it.
      pending_credentials_.origin = parsed_submitted_form_->origin;
      pending_credentials_.signon_realm = parsed_submitted_form_->signon_realm;
      pending_credentials_.action = parsed_submitted_form_->action;
    }
  } else {
    pending_credentials_state_ = PendingCredentialsState::NEW_LOGIN;
    // No stored credentials can be matched to the submitted form. Offer to
    // save new credentials.
    CreatePendingCredentialsForNewCredentials(password_to_save.second);
    // Generate username correction votes.
    bool username_correction_found =
        votes_uploader_.FindCorrectedUsernameElement(
            form_fetcher_->GetAllRelevantMatches(),
            parsed_submitted_form_->username_value,
            parsed_submitted_form_->password_value);
    UMA_HISTOGRAM_BOOLEAN("PasswordManager.UsernameCorrectionFound",
                          username_correction_found);
    if (username_correction_found) {
      metrics_recorder_->RecordDetailedUserAction(
          password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
              kCorrectedUsernameInForm);
    }
  }
  // Whether it's a new credential or an update to existing one, we set the
  // following fields.
  pending_credentials_.password_value =
      HasGeneratedPassword() ? generation_manager_->generated_password()
                             : password_to_save.first;
  pending_credentials_.preferred = true;
  pending_credentials_.date_last_used = base::Time::Now();
  pending_credentials_.form_has_autofilled_value =
      parsed_submitted_form_->form_has_autofilled_value;
  pending_credentials_.all_possible_passwords =
      parsed_submitted_form_->all_possible_passwords;
  CopyFieldPropertiesMasks(submitted_form_, &pending_credentials_.form_data);

  // If we're dealing with an API-driven provisionally saved form, then take
  // the server provided values. We don't do this for non-API forms, as
  // those will never have those members set.
  if (parsed_submitted_form_->type == PasswordForm::Type::kApi) {
    pending_credentials_.skip_zero_click =
        parsed_submitted_form_->skip_zero_click;
    pending_credentials_.display_name = parsed_submitted_form_->display_name;
    pending_credentials_.federation_origin =
        parsed_submitted_form_->federation_origin;
    pending_credentials_.icon_url = parsed_submitted_form_->icon_url;
    // It's important to override |signon_realm| for federated credentials
    // because it has format "federation://" + origin_host + "/" +
    // federation_host
    pending_credentials_.signon_realm = parsed_submitted_form_->signon_realm;
  }

  if (HasGeneratedPassword())
    pending_credentials_.type = PasswordForm::Type::kGenerated;
}

void PasswordFormManager::CreatePendingCredentialsForNewCredentials(
    const base::string16& password_element) {
  if (IsHttpAuth() || IsCredentialAPISave()) {
    pending_credentials_ = *parsed_submitted_form_;
    return;
  }

  pending_credentials_ = *parsed_submitted_form_;
  pending_credentials_.form_data = observed_form_;
  // The password value will be filled in later, remove any garbage for now.
  pending_credentials_.password_value.clear();
  // The password element should be determined earlier in |PasswordToSave|.
  pending_credentials_.password_element = password_element;
  // The new password's value and element name should be empty.
  pending_credentials_.new_password_value.clear();
  pending_credentials_.new_password_element.clear();
}

void PasswordFormManager::ProcessUpdate() {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  DCHECK(form_fetcher_->GetPreferredMatch() ||
         pending_credentials_.IsFederatedCredential());
  // If we're doing an Update, we either autofilled correctly and need to
  // update the stats, or the user typed in a new password for autofilled
  // username, or the user selected one of the non-preferred matches,
  // thus requiring a swap of preferred bits.
  DCHECK(pending_credentials_.preferred);
  DCHECK(!client_->IsIncognito());
  DCHECK(parsed_submitted_form_);

  password_manager_util::UpdateMetadataForUsage(&pending_credentials_);

  base::RecordAction(
      base::UserMetricsAction("PasswordManager_LoginFollowingAutofill"));

  // Check to see if this form is a candidate for password generation.
  // Do not send votes on change password forms, since they were already sent in
  // Update() method.
  if (!parsed_submitted_form_->IsPossibleChangePasswordForm()) {
    votes_uploader_.SendVoteOnCredentialsReuse(
        observed_form_, *parsed_submitted_form_, &pending_credentials_);
  }
  if (IsPasswordUpdate()) {
    votes_uploader_.UploadPasswordVote(
        *parsed_submitted_form_, *parsed_submitted_form_,
        autofill::NEW_PASSWORD,
        autofill::FormStructure(pending_credentials_.form_data)
            .FormSignatureAsStr());
  }

  if (pending_credentials_.times_used == 1) {
    votes_uploader_.UploadFirstLoginVotes(
        GetBestMatches(), pending_credentials_, *parsed_submitted_form_);
  }
}

void PasswordFormManager::FillHttpAuth() {
  DCHECK(IsHttpAuth());
  if (!form_fetcher_->GetPreferredMatch())
    return;
  client_->AutofillHttpAuth(*form_fetcher_->GetPreferredMatch(), this);
}

std::unique_ptr<PasswordForm> PasswordFormManager::ParseFormAndMakeLogging(
    const FormData& form,
    FormDataParser::Mode mode) {
  std::unique_ptr<PasswordForm> password_form = parser_.Parse(form, mode);

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogFormData(Logger::STRING_FORM_PARSING_INPUT, form);
    if (password_form)
      logger.LogPasswordForm(Logger::STRING_FORM_PARSING_OUTPUT,
                             *password_form);
  }
  return password_form;
}

void PasswordFormManager::PresaveGeneratedPasswordInternal(
    const FormData& form,
    const base::string16& generated_password) {
  std::unique_ptr<PasswordForm> parsed_form =
      ParseFormAndMakeLogging(form, FormDataParser::Mode::kSaving);

  if (!parsed_form) {
    // Create a password form with a minimum data.
    parsed_form.reset(new PasswordForm());
    parsed_form->origin = form.url;
    parsed_form->signon_realm = GetSignonRealm(form.url);
  }

  if (!HasGeneratedPassword()) {
    generation_manager_ =
        std::make_unique<PasswordGenerationManager>(form_saver_.get(), client_);
    votes_uploader_.set_generated_password_changed(false);
    metrics_recorder_->SetGeneratedPasswordStatus(
        PasswordFormMetricsRecorder::GeneratedPasswordStatus::
            kPasswordAccepted);
  } else {
    // If the password is already generated and a new value to presave differs
    // from the presaved one, then mark that the generated password was changed.
    // If a user recovers the original generated password, it will be recorded
    // as a password change.
    if (generation_manager_->generated_password() != generated_password) {
      votes_uploader_.set_generated_password_changed(true);
      metrics_recorder_->SetGeneratedPasswordStatus(
          PasswordFormMetricsRecorder::GeneratedPasswordStatus::
              kPasswordEdited);
    }
  }
  votes_uploader_.set_has_generated_password(true);

  // Set |password_value| to the generated password in order to ensure that the
  // generated password is saved.
  parsed_form->password_value = generated_password;

  generation_manager_->PresaveGeneratedPassword(
      std::move(*parsed_form), form_fetcher_->GetAllRelevantMatches());
}

void PasswordFormManager::CalculateFillingAssistanceMetric(
    const FormData& submitted_form) {
  // TODO(https://crbug.com/918846): implement collecting all necessary data on
  // iOS.
#if not defined(OS_IOS)
  std::set<base::string16> saved_usernames;
  std::set<base::string16> saved_passwords;

  for (auto* saved_form : form_fetcher_->GetNonFederatedMatches()) {
    saved_usernames.insert(saved_form->username_value);
    saved_passwords.insert(saved_form->password_value);
  }

  // Saved credentials might have empty usernames which are not interesting for
  // filling assistance metric.
  saved_usernames.erase(base::string16());

  metrics_recorder_->CalculateFillingAssistanceMetric(
      submitted_form, saved_usernames, saved_passwords, IsBlacklisted(),
      form_fetcher_->GetInteractionsStats());
#endif
}

void PasswordFormManager::SavePendingToStore(bool update) {
  const PasswordForm* saved_form = password_manager_util::GetMatchForUpdating(
      *parsed_submitted_form_, GetBestMatches());
  if ((update || IsPasswordUpdate()) &&
      !pending_credentials_.IsFederatedCredential()) {
    DCHECK(saved_form);
  }
  base::string16 old_password =
      saved_form ? saved_form->password_value : base::string16();
  if (HasGeneratedPassword()) {
    generation_manager_->CommitGeneratedPassword(
        pending_credentials_, form_fetcher_->GetAllRelevantMatches(),
        old_password);
  } else if (update) {
    form_saver_->Update(pending_credentials_,
                        form_fetcher_->GetAllRelevantMatches(), old_password);
  } else {
    form_saver_->Save(pending_credentials_,
                      form_fetcher_->GetAllRelevantMatches(), old_password);
  }
}

}  // namespace password_manager
