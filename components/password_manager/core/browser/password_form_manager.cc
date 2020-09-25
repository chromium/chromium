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
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/possible_username_data.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

using autofill::FieldDataManager;
using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormRendererId;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::GaiaIdHash;
using autofill::NOT_USERNAME;
using autofill::SINGLE_USERNAME;
using autofill::ValueElementPair;
using base::TimeDelta;
using base::TimeTicks;

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

bool PasswordFormManager::wait_for_server_predictions_for_filling_ = true;

namespace {

constexpr TimeDelta kMaxFillingDelayForServerPredictions =
    TimeDelta::FromMilliseconds(500);

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

// Find a field in |predictions| with given renderer id.
const PasswordFieldPrediction* FindFieldPrediction(
    const base::Optional<FormPredictions>& predictions,
    autofill::FieldRendererId field_renderer_id) {
  if (!predictions)
    return nullptr;
  for (const auto& field : predictions->fields) {
    if (field.renderer_id == field_renderer_id)
      return &field;
  }
  return nullptr;
}

void LogUsingPossibleUsername(PasswordManagerClient* client,
                              bool is_used,
                              const char* message) {
  if (!password_manager_util::IsLoggingActive(client))
    return;
  BrowserSavePasswordProgressLogger logger(client->GetLogManager());
  logger.LogString(is_used ? Logger::STRING_POSSIBLE_USERNAME_USED
                           : Logger::STRING_POSSIBLE_USERNAME_NOT_USED,
                   message);
}

}  // namespace

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const FormData& observed_form_data,
    FormFetcher* form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager,
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder)
    : PasswordFormManager(client,
                          observed_form_data,
                          form_fetcher,
                          std::move(password_save_manager),
                          metrics_recorder) {
  driver_ = driver;
  if (driver_)
    driver_id_ = driver->GetId();

  metrics_recorder_->RecordFormSignature(
      CalculateFormSignature(*observed_form()));
  // Do not fetch saved credentials for Chrome sync form, since nor filling nor
  // saving are supported.
  if (owned_form_fetcher_ &&
      !observed_form()->is_gaia_with_skip_save_password_form) {
    owned_form_fetcher_->Fetch();
  }
  votes_uploader_.StoreInitialFieldValues(*observed_form());
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    PasswordStore::FormDigest observed_http_auth_digest,
    FormFetcher* form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager)
    : PasswordFormManager(client,
                          observed_http_auth_digest,
                          form_fetcher,
                          std::move(password_save_manager),
                          nullptr /* metrics_recorder */) {
  if (owned_form_fetcher_)
    owned_form_fetcher_->Fetch();
}

PasswordFormManager::~PasswordFormManager() {
  form_fetcher_->RemoveConsumer(this);
}

bool PasswordFormManager::DoesManage(
    const FormData& form,
    const PasswordManagerDriver* driver) const {
  if (driver != driver_.get())
    return false;

  if (observed_form()->is_form_tag != form.is_form_tag)
    return false;
  // All unowned input elements are considered as one synthetic form.
  if (!observed_form()->is_form_tag && !form.is_form_tag)
    return true;
  return observed_form()->unique_renderer_id == form.unique_renderer_id;
}

bool PasswordFormManager::DoesManageAccordingToRendererId(
    autofill::FormRendererId form_renderer_id,
    const PasswordManagerDriver* driver) const {
  if (driver != driver_.get())
    return false;
  return observed_form()->unique_renderer_id == form_renderer_id;
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

const GURL& PasswordFormManager::GetURL() const {
  return observed_form() ? observed_form()->url : observed_digest()->url;
}

const std::vector<const PasswordForm*>& PasswordFormManager::GetBestMatches()
    const {
  return form_fetcher_->GetBestMatches();
}

std::vector<const PasswordForm*> PasswordFormManager::GetFederatedMatches()
    const {
  return form_fetcher_->GetFederatedMatches();
}

const PasswordForm& PasswordFormManager::GetPendingCredentials() const {
  return password_save_manager_->GetPendingCredentials();
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

base::span<const CompromisedCredentials>
PasswordFormManager::GetCompromisedCredentials() const {
  return form_fetcher_->GetCompromisedCredentials();
}

bool PasswordFormManager::IsBlacklisted() const {
  return form_fetcher_->IsBlacklisted() || newly_blacklisted_;
}

bool PasswordFormManager::WasUnblacklisted() const {
  return was_unblacklisted_while_on_page_;
}

bool PasswordFormManager::IsMovableToAccountStore() const {
  DCHECK(
      client_->GetPasswordFeatureManager()->ShouldShowAccountStorageBubbleUi())
      << "Ensure that the client supports moving passwords for this user!";
  signin::IdentityManager* identity_manager = client_->GetIdentityManager();
  DCHECK(identity_manager);
  const std::string gaia_id =
      identity_manager
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kNotRequired)
          .gaia;
  DCHECK(!gaia_id.empty()) << "Cannot move without signed in user";

  const base::string16& username = GetPendingCredentials().username_value;
  const base::string16& password = GetPendingCredentials().password_value;
  // If no match in the profile store with the same username and password exist,
  // then there is nothing to move.
  auto is_movable = [&](const PasswordForm* match) {
    return !match->IsUsingAccountStore() && match->username_value == username &&
           match->password_value == password;
  };
  return base::ranges::any_of(form_fetcher_->GetBestMatches(), is_movable) &&
         !form_fetcher_->IsMovingBlocked(GaiaIdHash::FromGaiaId(gaia_id),
                                         username);
}

void PasswordFormManager::Save() {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  DCHECK(!client_->IsIncognito());
  if (IsBlacklisted()) {
    password_save_manager_->Unblacklist(ConstructObservedFormDigest());
    newly_blacklisted_ = false;
  }

  password_save_manager_->Save(observed_form(), *parsed_submitted_form_);

  client_->UpdateFormManagers();
}

void PasswordFormManager::Update(const PasswordForm& credentials_to_update) {
  metrics_util::LogPasswordAcceptedSaveUpdateSubmissionIndicatorEvent(
      parsed_submitted_form_->submission_event);
  metrics_recorder_->SetSubmissionIndicatorEvent(
      parsed_submitted_form_->submission_event);

  password_save_manager_->Update(credentials_to_update, observed_form(),
                                 *parsed_submitted_form_);

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
  password_save_manager_->PermanentlyBlacklist(ConstructObservedFormDigest());
  newly_blacklisted_ = true;
}

PasswordStore::FormDigest PasswordFormManager::ConstructObservedFormDigest()
    const {
  std::string signon_realm;
  GURL url;
  if (observed_digest()) {
    url = observed_digest()->url;
    // GetSignonRealm is not suitable for http auth credentials.
    signon_realm = IsHttpAuth() ? observed_digest()->signon_realm
                                : GetSignonRealm(observed_digest()->url);
  } else {
    url = observed_form()->url;
    signon_realm = GetSignonRealm(observed_form()->url);
  }
  return PasswordStore::FormDigest(GetScheme(), signon_realm, url);
}

void PasswordFormManager::OnPasswordsRevealed() {
  votes_uploader_.set_has_passwords_revealed_vote(true);
}

void PasswordFormManager::MoveCredentialsToAccountStore() {
  DCHECK(client_->GetPasswordFeatureManager()->IsOptedInForAccountStorage());
  password_save_manager_->MoveCredentialsToAccountStore(
      metrics_util::MoveToAccountStoreTrigger::
          kSuccessfulLoginWithProfileStorePassword);
}

void PasswordFormManager::BlockMovingCredentialsToAccountStore() {
  // Nothing to do if there is no signed in user or the credentials are already
  // blocked for moving.
  if (!IsMovableToAccountStore())
    return;
  const std::string gaia_id =
      client_->GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kNotRequired)
          .gaia;
  // The above call to IsMovableToAccountStore() guarantees there is a signed in
  // user.
  DCHECK(!gaia_id.empty());
  password_save_manager_->BlockMovingToAccountStoreFor(
      GaiaIdHash::FromGaiaId(gaia_id));
}

bool PasswordFormManager::IsNewLogin() const {
  return password_save_manager_->IsNewLogin();
}

FormFetcher* PasswordFormManager::GetFormFetcher() {
  return form_fetcher_;
}

bool PasswordFormManager::IsPendingCredentialsPublicSuffixMatch() const {
  return password_save_manager_->GetPendingCredentials().is_public_suffix_match;
}

void PasswordFormManager::PresaveGeneratedPassword(
    const FormData& form_data,
    const base::string16& password_value) {
  // TODO(https://crbug.com/831123): Propagate generated password independently
  // of PasswordForm when PasswordForm goes away from the renderer process.
  PresaveGeneratedPasswordInternal(form_data,
                                   password_value /*generated_password*/);
}

void PasswordFormManager::PasswordNoLongerGenerated() {
  if (!HasGeneratedPassword())
    return;

  password_save_manager_->PasswordNoLongerGenerated();
}

bool PasswordFormManager::HasGeneratedPassword() const {
  return password_save_manager_->HasGeneratedPassword();
}

void PasswordFormManager::SetGenerationPopupWasShown(
    bool is_manual_generation) {
  votes_uploader_.set_generation_popup_was_shown(true);
  votes_uploader_.set_is_manual_generation(is_manual_generation);
  metrics_recorder_->SetPasswordGenerationPopupShown(true,
                                                     is_manual_generation);
}

void PasswordFormManager::SetGenerationElement(
    FieldRendererId generation_element) {
  votes_uploader_.set_generation_element(generation_element);
}

bool PasswordFormManager::IsPossibleChangePasswordFormWithoutUsername() const {
  return parsed_submitted_form_ &&
         parsed_submitted_form_->IsPossibleChangePasswordFormWithoutUsername();
}

bool PasswordFormManager::IsPasswordUpdate() const {
  return password_save_manager_->IsPasswordUpdate();
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
    FieldRendererId generation_element) {
  *mutable_observed_form() = form;
  PresaveGeneratedPasswordInternal(form, generated_password);
  votes_uploader_.set_generation_element(generation_element);
}

bool PasswordFormManager::UpdateStateOnUserInput(
    FormRendererId form_id,
    FieldRendererId field_id,
    const base::string16& field_value) {
  if (form_id) {
    if (!observed_form()->is_form_tag ||
        (observed_form()->is_form_tag &&
         observed_form()->unique_renderer_id != form_id)) {
      return false;
    }
  } else if (observed_form()->is_form_tag) {
    return false;
  }

  bool form_data_changed = false;
  for (FormFieldData& field : mutable_observed_form()->fields) {
    if (field.unique_renderer_id == field_id) {
      field.value = field_value;
      form_data_changed = true;
      break;
    }
  }

  if (!HasGeneratedPassword())
    return true;

  base::string16 generated_password =
      password_save_manager_->GetGeneratedPassword();
  if (votes_uploader_.get_generation_element() == field_id) {
    generated_password = field_value;
    form_data_changed = true;
  }
  if (form_data_changed)
    PresaveGeneratedPasswordInternal(*observed_form(), generated_password);
  return true;
}

void PasswordFormManager::SetDriver(
    const base::WeakPtr<PasswordManagerDriver>& driver) {
  driver_ = driver;
}

void PasswordFormManager::UpdateObservedFormDataWithFieldDataManagerInfo(
    const FieldDataManager* field_data_manager) {
  for (FormFieldData& field : mutable_observed_form()->fields) {
    FieldRendererId field_id = field.unique_renderer_id;
    if (!field_data_manager->HasFieldData(field_id))
      continue;
    field.typed_value = field_data_manager->GetUserTypedValue(field_id);
    field.properties_mask =
        field_data_manager->GetFieldPropertiesMask(field_id);
    field.value =
        field_data_manager->GetAutofilledValue(field_id).value_or(field.value);
  }
}
#endif  // defined(OS_IOS)

std::unique_ptr<PasswordFormManager> PasswordFormManager::Clone() {
  // Fetcher is cloned to avoid re-fetching data from PasswordStore.
  std::unique_ptr<FormFetcher> fetcher = form_fetcher_->Clone();

  // Some data is filled through the constructor. No PasswordManagerDriver is
  // needed, because the UI does not need any functionality related to the
  // renderer process, to which the driver serves as an interface.
  auto result = base::WrapUnique(new PasswordFormManager(
      client_, observed_form_or_digest_, fetcher.get(),
      password_save_manager_->Clone(), metrics_recorder_));

  // The constructor only can take a weak pointer to the fetcher, so moving the
  // owning one needs to happen explicitly.
  result->owned_form_fetcher_ = std::move(fetcher);

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

  if (parsed_submitted_form_) {
    result->parsed_submitted_form_ =
        std::make_unique<PasswordForm>(*parsed_submitted_form_);
  }
  result->is_submitted_ = is_submitted_;
  result->password_save_manager_->Init(result->client_, result->form_fetcher_,
                                       result->metrics_recorder_,
                                       &result->votes_uploader_);
  return result;
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    std::unique_ptr<PasswordForm> saved_form,
    std::unique_ptr<FormFetcher> form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager)
    : PasswordFormManager(client,
                          PasswordStore::FormDigest(*saved_form),
                          form_fetcher.get(),
                          std::move(password_save_manager),
                          nullptr /* metrics_recorder */) {
  parsed_submitted_form_ = std::move(saved_form);
  is_submitted_ = true;
  owned_form_fetcher_ = std::move(form_fetcher);
  owned_form_fetcher_->Fetch();
}

void PasswordFormManager::OnFetchCompleted() {
  received_stored_credentials_time_ = TimeTicks::Now();

  newly_blacklisted_ = false;
  autofills_left_ = kMaxTimesAutofill;

  if (IsCredentialAPISave()) {
    // This is saving with credential API, there is no form to fill, so no
    // filling required.
    return;
  }

  client_->UpdateCredentialCache(url::Origin::Create(GetURL()),
                                 form_fetcher_->GetBestMatches(),
                                 form_fetcher_->IsBlacklisted());

  if (is_submitted_)
    CreatePendingCredentials();

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

void PasswordFormManager::CreatePendingCredentials() {
  DCHECK(is_submitted_);
  if (!parsed_submitted_form_)
    return;

  password_save_manager_->CreatePendingCredentials(
      *parsed_submitted_form_, observed_form(), submitted_form_, IsHttpAuth(),
      IsCredentialAPISave());
}

void PasswordFormManager::ResetState() {
  parsed_submitted_form_.reset();
  submitted_form_ = FormData();
  password_save_manager_->ResetPendingCredentials();
  is_submitted_ = false;
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

  bool have_password_to_save =
      parsed_submitted_form &&
      parsed_submitted_form->HasNonEmptyPasswordValue();

  if (!have_password_to_save) {
    // In case of error during parsing, reset the state.
    ResetState();
    return false;
  }

  parsed_submitted_form_ = std::move(parsed_submitted_form);
  submitted_form_ = submitted_form;
  is_submitted_ = true;
  CalculateFillingAssistanceMetric(submitted_form);
  metrics_recorder_->set_possible_username_used(false);
  votes_uploader_.clear_single_username_vote_data();

  if (IsUsernameFirstFlowFeatureEnabled() &&
      parsed_submitted_form_->username_value.empty() &&
      UsePossibleUsername(possible_username)) {
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
  if (*observed_digest() != PasswordStore::FormDigest(submitted_form))
    return false;

  parsed_submitted_form_ = std::make_unique<PasswordForm>(submitted_form);
  is_submitted_ = true;
  CreatePendingCredentials();
  return true;
}

bool PasswordFormManager::IsHttpAuth() const {
  return GetScheme() != PasswordForm::Scheme::kHtml;
}

bool PasswordFormManager::IsCredentialAPISave() const {
  return observed_digest() && !IsHttpAuth();
}

PasswordForm::Scheme PasswordFormManager::GetScheme() const {
  return observed_digest() ? observed_digest()->scheme
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
      CalculateFormSignature(*observed_form());
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
      ParseFormAndMakeLogging(*observed_form(), FormDataParser::Mode::kFilling);
  RecordMetricOnReadonly(parser_.readonly_status(), !!observed_password_form,
                         FormDataParser::Mode::kFilling);
  if (!observed_password_form)
    return;

  if (observed_password_form->is_new_password_reliable && !IsBlacklisted()) {
#if defined(OS_IOS)
    driver_->FormEligibleForGenerationFound(
        {/*form_renderer_id*/ observed_password_form->form_data
             .unique_renderer_id,
         /*new_password_element_renderer_id*/
         observed_password_form->new_password_element_renderer_id,
         /*confirmation_password_element_renderer_id*/
         observed_password_form->confirmation_password_element_renderer_id});
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

void PasswordFormManager::FillForm(const FormData& observed_form_data) {
  uint32_t differences_bitmask =
      FindFormsDifferences(*observed_form(), observed_form_data);
  metrics_recorder_->RecordFormChangeBitmask(differences_bitmask);

  if (differences_bitmask)
    *mutable_observed_form() = observed_form_data;

  if (!waiting_for_server_predictions_)
    Fill();
}

void PasswordFormManager::OnGeneratedPasswordAccepted(
    FormData form_data,
    autofill::FieldRendererId generation_element_id,
    const base::string16& password) {
  // Find the generating element to update its value. The parser needs a non
  // empty value.
  auto it = std::find_if(form_data.fields.begin(), form_data.fields.end(),
                         [generation_element_id](const auto& field_data) {
                           return generation_element_id ==
                                  field_data.unique_renderer_id;
                         });
  // The parameters are coming from the renderer and can't be trusted.
  if (it == form_data.fields.end())
    return;
  it->value = password;
  std::unique_ptr<PasswordForm> parsed_form =
      ParseFormAndMakeLogging(form_data, FormDataParser::Mode::kSaving);
  if (!parsed_form) {
    // Create a password form with a minimum data.
    parsed_form = std::make_unique<PasswordForm>();
    parsed_form->url = form_data.url;
    parsed_form->signon_realm = GetSignonRealm(form_data.url);
  }
  parsed_form->password_value = password;
  password_save_manager_->GeneratedPasswordAccepted(*parsed_form, driver_);
}

void PasswordFormManager::MarkWasUnblacklisted() {
  was_unblacklisted_while_on_page_ = true;
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    FormOrDigest observed_form_or_digest,
    FormFetcher* form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager,
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder)
    : client_(client),
      observed_form_or_digest_(std::move(observed_form_or_digest)),
      metrics_recorder_(metrics_recorder),
      owned_form_fetcher_(
          form_fetcher ? nullptr
                       : FormFetcherImpl::CreateFormFetcherImpl(
                             observed_digest()
                                 ? *observed_digest()
                                 : PasswordStore::FormDigest(*observed_form()),
                             client_,
                             true /* should_migrate_http_passwords */)),
      form_fetcher_(form_fetcher ? form_fetcher : owned_form_fetcher_.get()),
      password_save_manager_(std::move(password_save_manager)),
      // TODO(https://crbug.com/831123): set correctly
      // |is_possible_change_password_form| in |votes_uploader_| constructor
      votes_uploader_(client, false /* is_possible_change_password_form */) {
  form_fetcher_->AddConsumer(this);
  if (!metrics_recorder_) {
    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        client_->IsCommittedMainFrameSecure(), client_->GetUkmSourceId(),
        client_->GetPrefs());
  }
  password_save_manager_->Init(client_, form_fetcher_, metrics_recorder_,
                               &votes_uploader_);
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
    parsed_form = std::make_unique<PasswordForm>();
    parsed_form->url = form.url;
    parsed_form->signon_realm = GetSignonRealm(form.url);
  }
  // Set |password_value| to the generated password in order to ensure that
  // the generated password is saved.
  parsed_form->password_value = generated_password;

  password_save_manager_->PresaveGeneratedPassword(std::move(*parsed_form));
}

void PasswordFormManager::CalculateFillingAssistanceMetric(
    const FormData& submitted_form) {
  std::set<std::pair<base::string16, PasswordForm::Store>> saved_usernames;
  std::set<std::pair<base::string16, PasswordForm::Store>> saved_passwords;

  for (auto* saved_form : form_fetcher_->GetNonFederatedMatches()) {
    // Saved credentials might have empty usernames which are not interesting
    // for filling assistance metric.
    if (!saved_form->username_value.empty())
      saved_usernames.emplace(saved_form->username_value, saved_form->in_store);
    saved_passwords.emplace(saved_form->password_value, saved_form->in_store);
  }

  metrics_recorder_->CalculateFillingAssistanceMetric(
      submitted_form, saved_usernames, saved_passwords, IsBlacklisted(),
      form_fetcher_->GetInteractionsStats(),
      client_->GetPasswordFeatureManager()
          ->ComputePasswordAccountStorageUsageLevel());
}

bool PasswordFormManager::UsePossibleUsername(
    const PossibleUsernameData* possible_username) {
  if (!possible_username) {
    LogUsingPossibleUsername(client_, /*is_used*/ false, "Null");
    return false;
  }

  // The username form and password forms signon realms must be the same.
  if (GetSignonRealm(observed_form()->url) != possible_username->signon_realm) {
    LogUsingPossibleUsername(client_, /*is_used*/ false, "Different domains");
    return false;
  }

  // The username candidate field should not be in |observed_form()|, otherwise
  // that is a task of FormParser to choose it from |observed_form()|.
  if (possible_username->driver_id == driver_id_) {
    for (const auto& field : observed_form()->fields) {
      if (field.unique_renderer_id == possible_username->renderer_id) {
        LogUsingPossibleUsername(client_, /*is_used*/ false, "Same form");
        return false;
      }
    }
  }

  // Check whether server predictions have a definite answer.
  const PasswordFieldPrediction* field_prediction = FindFieldPrediction(
      possible_username->form_predictions, possible_username->renderer_id);
  if (field_prediction) {
    if (field_prediction->type == SINGLE_USERNAME) {
      LogUsingPossibleUsername(client_, /*is_used*/ true, "Server predictions");
      return true;
    }
    if (field_prediction->type == NOT_USERNAME) {
      LogUsingPossibleUsername(client_, /*is_used*/ false,
                               "Server predictions");
      return false;
    }
  }

#if defined(OS_ANDROID)
  // Do not trust local heuristics on Android.
  // TODO(https://crbug.com/1051914): Make local heuristics more reliable.
  return false;
#else
  // Check whether it is already learned from previous user actions whether
  // |possible_username| corresponds to the valid username form.
  const FieldInfoManager* field_info_manager = client_->GetFieldInfoManager();
  if (field_info_manager && field_prediction) {
    auto form_signature = possible_username->form_predictions->form_signature;
    auto field_signature = field_prediction->signature;
    autofill::ServerFieldType type =
        field_info_manager->GetFieldType(form_signature, field_signature);
    if (type == SINGLE_USERNAME) {
      LogUsingPossibleUsername(client_, /*is_used*/ true, "Local prediction");
      return true;
    }
    if (type == NOT_USERNAME) {
      LogUsingPossibleUsername(client_, /*is_used*/ false, "Local prediction");
      return false;
    }
  }

  bool is_possible_username_valid = IsPossibleUsernameValid(
      *possible_username, parsed_submitted_form_->signon_realm,
      base::Time::Now());
  LogUsingPossibleUsername(client_, /*is_used*/ is_possible_username_valid,
                           "Local heuristics");
  return is_possible_username_valid;
#endif  // defined(OS_ANDROID)
}

}  // namespace password_manager
