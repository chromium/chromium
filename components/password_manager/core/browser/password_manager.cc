// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_onboarding.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

#if defined(OS_WIN)
#include "components/prefs/pref_registry_simple.h"
#endif

using autofill::FormData;
using autofill::FormStructure;
using autofill::PasswordForm;
using autofill::mojom::PasswordFormFieldPredictionType;
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
using password_manager::metrics_util::GaiaPasswordHashChange;
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED

namespace password_manager {

namespace {

// Shorten the name to spare line breaks. The code provides enough context
// already.
using Logger = autofill::SavePasswordProgressLogger;

bool AreAllFieldsEmpty(const FormData& form_data) {
  for (const auto& field : form_data.fields) {
    if (!field.value.empty())
      return false;
  }

  return true;
}

// Returns true if the user needs to be prompted before a password can be
// saved (instead of automatically saving the password), based on inspecting
// the state of |manager|.
bool ShouldPromptUserToSavePassword(const PasswordFormManager& manager) {
  if (manager.IsPasswordUpdate()) {
    // Updating a credential might erase a useful stored value by accident.
    // Always ask the user to confirm.
    return true;
  }

  // User successfully signed-in with PSL match credentials. These credentials
  // should be automatically saved in order to be autofilled on next login.
  if (manager.IsPendingCredentialsPublicSuffixMatch())
    return false;

  if (manager.HasGeneratedPassword())
    return false;

  return manager.IsNewLogin();
}

// Checks that |form| has visible password fields. It should be used only for
// GAIA forms.
bool IsThereVisiblePasswordField(const FormData& form) {
  for (const autofill::FormFieldData& field : form.fields) {
    if (field.form_control_type == "password" && field.is_focusable)
      return true;
  }
  return false;
}

// Finds the matched form manager for |form| in |form_managers|.
PasswordFormManager* FindMatchedManager(
    const FormData& form,
    const std::vector<std::unique_ptr<PasswordFormManager>>& form_managers,
    const PasswordManagerDriver* driver) {
  for (const auto& form_manager : form_managers) {
    if (form_manager->DoesManage(form, driver))
      return form_manager.get();
  }
  return nullptr;
}

// Finds the matched form manager with id |form_renderer_id| in |form_managers|.
PasswordFormManager* FindMatchedManagerByRendererId(
    uint32_t form_renderer_id,
    const std::vector<std::unique_ptr<PasswordFormManager>>& form_managers,
    const PasswordManagerDriver* driver) {
  for (const auto& form_manager : form_managers) {
    if (form_manager->DoesManageAccordingToRendererId(form_renderer_id, driver))
      return form_manager.get();
  }
  return nullptr;
}

bool HasSingleUsernameVote(const FormStructure& form) {
  for (const auto& field : form) {
    if (field->server_type() == autofill::SINGLE_USERNAME)
      return true;
  }
  return false;
}

// Returns true if at least one of the fields in |form| has a prediction to be a
// new-password related field.
bool HasNewPasswordVote(const FormStructure& form) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              KEnablePasswordGenerationForClearTextFields))
    return false;
  for (const auto& field : form) {
    if (field->server_type() == autofill::ACCOUNT_CREATION_PASSWORD ||
        field->server_type() == autofill::NEW_PASSWORD) {
      return true;
    }
  }
  return false;
}

}  // namespace

// static
void PasswordManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kBlacklistedCredentialsNormalized,
                                false);
  registry->RegisterBooleanPref(
      prefs::kCredentialsEnableService, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kCredentialsEnableAutosignin, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterStringPref(prefs::kSyncPasswordHash, std::string(),
                               PrefRegistry::NO_REGISTRATION_FLAGS);
  registry->RegisterStringPref(prefs::kSyncPasswordLengthAndHashSalt,
                               std::string(),
                               PrefRegistry::NO_REGISTRATION_FLAGS);
  registry->RegisterBooleanPref(
      prefs::kWasAutoSignInFirstRunExperienceShown, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterDoublePref(prefs::kLastTimeObsoleteHttpCredentialsRemoved,
                               0.0);
  registry->RegisterIntegerPref(
      prefs::kPasswordManagerOnboardingState,
      static_cast<int>(metrics_util::OnboardingState::kDoNotShow));
  registry->RegisterBooleanPref(prefs::kWasOnboardingFeatureCheckedBefore,
                                false);

#if defined(OS_MACOSX)
  registry->RegisterIntegerPref(prefs::kKeychainMigrationStatus,
                                4 /* MIGRATED_DELETED */);
#endif
  registry->RegisterListPref(prefs::kPasswordHashDataList,
                             PrefRegistry::NO_REGISTRATION_FLAGS);
  registry->RegisterBooleanPref(
      prefs::kPasswordLeakDetectionEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// static
void PasswordManager::RegisterLocalPrefs(PrefRegistrySimple* registry) {
#if defined(OS_WIN)
  registry->RegisterInt64Pref(prefs::kOsPasswordLastChanged, 0);
  registry->RegisterBooleanPref(prefs::kOsPasswordBlank, false);
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
  registry->RegisterTimePref(prefs::kPasswordRecovery, base::Time());
#endif
}

PasswordManager::PasswordManager(PasswordManagerClient* client)
    : client_(client), leak_delegate_(client) {
  DCHECK(client_);
}

PasswordManager::~PasswordManager() = default;

void PasswordManager::OnGeneratedPasswordAccepted(
    PasswordManagerDriver* driver,
    const FormData& form_data,
    uint32_t generation_element_id,
    const base::string16& password) {
  PasswordFormManager* manager = GetMatchedManager(driver, form_data);
  if (manager) {
    manager->OnGeneratedPasswordAccepted(form_data, generation_element_id,
                                         password);
  } else {
    // OnPresaveGeneratedPassword records the histogram in all other cases.
    UMA_HISTOGRAM_BOOLEAN("PasswordManager.GeneratedFormHasNoFormManager",
                          true);
  }
}

void PasswordManager::OnPresaveGeneratedPassword(PasswordManagerDriver* driver,
                                                 const PasswordForm& form) {
  DCHECK(client_->IsSavingAndFillingEnabled(form.origin));
  PasswordFormManager* form_manager = GetMatchedManager(driver, form);
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.GeneratedFormHasNoFormManager",
                        !form_manager);
  if (form_manager)
    form_manager->PresaveGeneratedPassword(form);
}

void PasswordManager::OnPasswordNoLongerGenerated(PasswordManagerDriver* driver,
                                                  const PasswordForm& form) {
  DCHECK(client_->IsSavingAndFillingEnabled(form.origin));

  PasswordFormManager* form_manager = GetMatchedManager(driver, form);
  if (form_manager)
    form_manager->PasswordNoLongerGenerated();
}

void PasswordManager::SetGenerationElementAndReasonForForm(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& form,
    const base::string16& generation_element,
    bool is_manually_triggered) {
  DCHECK(client_->IsSavingAndFillingEnabled(form.origin));

  PasswordFormManager* form_manager = GetMatchedManager(driver, form);
  if (form_manager) {
    form_manager->SetGenerationElement(generation_element);
    form_manager->SetGenerationPopupWasShown(is_manually_triggered);
  }
}

void PasswordManager::DidNavigateMainFrame(bool form_may_be_submitted) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogBoolean(Logger::STRING_DID_NAVIGATE_MAIN_FRAME,
                       form_may_be_submitted);
  }

  if (client_->IsNewTabPage()) {
    if (logger)
      logger->LogMessage(Logger::STRING_NAVIGATION_NTP);
    // On a successful Chrome sign-in the page navigates to the new tab page
    // (ntp). OnPasswordFormsRendered is not called on ntp. That is why the
    // standard flow for saving hash does not work. Save a password hash now
    // since a navigation to ntp is the sign of successful sign-in.
    PasswordFormManager* manager = GetSubmittedManager();
    if (manager && manager->GetSubmittedForm()
                       ->form_data.is_gaia_with_skip_save_password_form) {
      MaybeSavePasswordHash(manager);
    }
  }

  for (std::unique_ptr<PasswordFormManager>& manager : form_managers_) {
    if (form_may_be_submitted && manager->is_submitted()) {
      owned_submitted_form_manager_ = std::move(manager);
      break;
    }
  }
  form_managers_.clear();

  TryToFindPredictionsToPossibleUsernameData();
  predictions_.clear();
  store_password_called_ = false;
}

void PasswordManager::UpdateFormManagers() {
  std::vector<PasswordFormManager*> form_managers;
  for (const auto& form_manager : form_managers_)
    form_managers.push_back(form_manager.get());

  // Get the fetchers and all the drivers.
  std::vector<FormFetcher*> fetchers;
  std::vector<PasswordManagerDriver*> drivers;
  for (PasswordFormManager* form_manager : form_managers) {
    fetchers.push_back(form_manager->GetFormFetcher());
    if (form_manager->GetDriver())
      drivers.push_back(form_manager->GetDriver().get());
  }

  // Remove the duplicates.
  std::sort(fetchers.begin(), fetchers.end());
  fetchers.erase(std::unique(fetchers.begin(), fetchers.end()), fetchers.end());
  std::sort(drivers.begin(), drivers.end());
  drivers.erase(std::unique(drivers.begin(), drivers.end()), drivers.end());
  // Refetch credentials for all the forms and update the drivers.
  for (FormFetcher* fetcher : fetchers)
    fetcher->Fetch();

  // The autofill manager will be repopulated again when the credentials
  // are retrieved.
  for (PasswordManagerDriver* driver : drivers) {
    // GetPasswordAutofillManager() is returning nullptr in iOS Chrome, since
    // PasswordAutofillManager is not instantiated on iOS Chrome.
    // See //ios/chrome/browser/passwords/ios_chrome_password_manager_driver.mm
    if (driver->GetPasswordAutofillManager()) {
      driver->GetPasswordAutofillManager()->DeleteFillData();
    }
  }
}

void PasswordManager::DropFormManagers() {
  form_managers_.clear();
  owned_submitted_form_manager_.reset();
  all_visible_forms_.clear();
  TryToFindPredictionsToPossibleUsernameData();
  predictions_.clear();
}

bool PasswordManager::IsPasswordFieldDetectedOnPage() {
  return !form_managers_.empty();
}

void PasswordManager::OnPasswordFormSubmitted(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  ProvisionallySaveForm(password_form.form_data, driver, false);
}

void PasswordManager::OnPasswordFormSubmittedNoChecks(
    password_manager::PasswordManagerDriver* driver,
    autofill::mojom::SubmissionIndicatorEvent event) {
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogMessage(Logger::STRING_ON_SAME_DOCUMENT_NAVIGATION);
  }
  PasswordFormManager* submitted_manager = GetSubmittedManager();
  // TODO(crbug.com/949519): Add UMA metric for how frequently submitted_manager
  // is actually null.
  if (!submitted_manager || !submitted_manager->GetSubmittedForm())
    return;

  const PasswordForm* submitted_form = submitted_manager->GetSubmittedForm();

  if (gaia::IsGaiaSignonRealm(GURL(submitted_form->signon_realm)) &&
      !IsThereVisiblePasswordField(submitted_form->form_data)) {
    // Gaia form without visible password fields is found.
    // It might happen only when Password Manager autofilled a username
    // (visible) and a password (invisible) fields. Then the user typed a new
    // username. A page removed the form. As result a form is inconsistent - the
    // username from one account, the password from another. Skip such form.
    return;
  }

  submitted_manager->UpdateSubmissionIndicatorEvent(event);

  if (IsAutomaticSavePromptAvailable())
    OnLoginSuccessful();
}

#if defined(OS_IOS)
void PasswordManager::OnPasswordFormSubmittedNoChecksForiOS(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogMessage(Logger::STRING_ON_SAME_DOCUMENT_NAVIGATION);
  }

  ProvisionallySaveForm(password_form.form_data, driver, false);

  if (IsAutomaticSavePromptAvailable())
    OnLoginSuccessful();
}
#endif

void PasswordManager::OnUserModifiedNonPasswordField(
    PasswordManagerDriver* driver,
    int32_t renderer_id,
    const base::string16& value) {
  // |driver| might be empty on iOS or in tests.
  int driver_id = driver ? driver->GetId() : 0;
  possible_username_.emplace(GetSignonRealm(driver->GetLastCommittedURL()),
                             renderer_id, value, base::Time::Now(), driver_id);
}

void PasswordManager::ShowManualFallbackForSaving(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  PasswordFormManager* manager =
      ProvisionallySaveForm(password_form.form_data, driver, true);

  if (manager && password_form.form_data.is_gaia_with_skip_save_password_form) {
    manager->GetMetricsRecorder()
        ->set_user_typed_password_on_chrome_sign_in_page();
  }

  if (!client_->GetProfilePasswordStore()->IsAbleToSavePasswords() ||
      !client_->IsSavingAndFillingEnabled(password_form.origin) ||
      ShouldBlockPasswordForSameOriginButDifferentScheme(
          password_form.origin)) {
    return;
  }

  auto availability =
      manager ? PasswordManagerMetricsRecorder::FormManagerAvailable::kSuccess
              : PasswordManagerMetricsRecorder::FormManagerAvailable::
                    kMissingManual;
  if (client_ && client_->GetMetricsRecorder())
    client_->GetMetricsRecorder()->RecordFormManagerAvailable(availability);
  if (!manager)
    return;

  if (!client_->GetStoreResultFilter()->ShouldSave(
          *manager->GetSubmittedForm())) {
    return;
  }

  // Show the fallback if a prompt or a confirmation bubble should be available.
  bool has_generated_password = manager->HasGeneratedPassword();
  if (ShouldPromptUserToSavePassword(*manager) || has_generated_password) {
    bool is_update = manager->IsPasswordUpdate();
    manager->GetMetricsRecorder()->RecordShowManualFallbackForSaving(
        has_generated_password, is_update);
    client_->ShowManualFallbackForSaving(manager->Clone(),
                                         has_generated_password, is_update);
  } else {
    HideManualFallbackForSaving();
  }
}

void PasswordManager::HideManualFallbackForSaving() {
  client_->HideManualFallbackForSaving();
}

void PasswordManager::OnPasswordFormsParsed(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<PasswordForm>& forms) {
  CreatePendingLoginManagers(driver, forms);

  PasswordGenerationFrameHelper* password_generation_manager =
      driver ? driver->GetPasswordGenerationHelper() : nullptr;
  if (password_generation_manager) {
    password_generation_manager->PrefetchSpec(
        client_->GetLastCommittedEntryURL().GetOrigin());
  }
}

void PasswordManager::CreatePendingLoginManagers(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<PasswordForm>& forms) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_CREATE_LOGIN_MANAGERS_METHOD);
  }

  CreateFormManagers(driver, forms);

  // Record whether or not this top-level URL has at least one password field.
  client_->AnnotateNavigationEntry(!forms.empty());

  // Only report SSL error status for cases where there are potentially forms to
  // fill or save from.
  if (!forms.empty()) {
    metrics_util::CertificateError cert_error =
        metrics_util::CertificateError::NONE;
    const net::CertStatus cert_status = client_->GetMainFrameCertStatus();
    // The order of the if statements matters -- if the status involves multiple
    // errors, Chrome should report the one highest up in the list below.
    if (cert_status & net::CERT_STATUS_AUTHORITY_INVALID)
      cert_error = metrics_util::CertificateError::AUTHORITY_INVALID;
    else if (cert_status & net::CERT_STATUS_COMMON_NAME_INVALID)
      cert_error = metrics_util::CertificateError::COMMON_NAME_INVALID;
    else if (cert_status & net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM)
      cert_error = metrics_util::CertificateError::WEAK_SIGNATURE_ALGORITHM;
    else if (cert_status & net::CERT_STATUS_DATE_INVALID)
      cert_error = metrics_util::CertificateError::DATE_INVALID;
    else if (net::IsCertStatusError(cert_status))
      cert_error = metrics_util::CertificateError::OTHER;

    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.CertificateErrorsWhileSeeingForms", cert_error,
        metrics_util::CertificateError::COUNT);
  }
}

void PasswordManager::CreateFormManagers(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<PasswordForm>& forms) {
  // Find new forms.
  std::vector<const PasswordForm*> new_forms;
  for (const PasswordForm& form : forms) {
    if (!client_->IsFillingEnabled(form.origin))
      continue;

    PasswordFormManager* manager =
        FindMatchedManager(form.form_data, form_managers_, driver);

    if (manager) {
      // This extra filling is just duplicating redundancy that was in
      // PasswordFormManager, that helps to fix cases when the site overrides
      // filled values.
      // TODO(https://crbug.com/831123): Implement more robust filling and
      // remove the next line.
      manager->FillForm(form.form_data);
    } else {
      new_forms.push_back(&form);
    }
  }

  // Create form manager for new forms.
  for (const PasswordForm* new_form : new_forms)
    CreateFormManager(driver, new_form->form_data);
}

PasswordFormManager* PasswordManager::CreateFormManager(
    PasswordManagerDriver* driver,
    const autofill::FormData& form) {
  form_managers_.push_back(std::make_unique<PasswordFormManager>(
      client_,
      driver ? driver->AsWeakPtr() : base::WeakPtr<PasswordManagerDriver>(),
      form, nullptr,
      std::make_unique<FormSaverImpl>(client_->GetProfilePasswordStore()),
      nullptr));
  form_managers_.back()->ProcessServerPredictions(predictions_);
  return form_managers_.back().get();
}

PasswordFormManager* PasswordManager::ProvisionallySaveForm(
    const FormData& submitted_form,
    PasswordManagerDriver* driver,
    bool is_manual_fallback) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_PROVISIONALLY_SAVE_FORM_METHOD);
  }
  if (!client_->IsSavingAndFillingEnabled(submitted_form.url)) {
    RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::SAVING_DISABLED, submitted_form.url,
        logger.get());
    return nullptr;
  }

  if (store_password_called_)
    return nullptr;

  // No need to report PasswordManagerMetricsRecorder::EMPTY_PASSWORD, because
  // PasswordToSave in PasswordFormManager DCHECKs that the password is never
  // empty.

  const GURL& origin = submitted_form.url;
  if (ShouldBlockPasswordForSameOriginButDifferentScheme(origin)) {
    RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::SAVING_ON_HTTP_AFTER_HTTPS, origin,
        logger.get());
    return nullptr;
  }

  PasswordFormManager* matched_manager =
      GetMatchedManager(driver, submitted_form);

  auto availability =
      matched_manager
          ? PasswordManagerMetricsRecorder::FormManagerAvailable::kSuccess
          : PasswordManagerMetricsRecorder::FormManagerAvailable::
                kMissingProvisionallySave;
  if (client_ && client_->GetMetricsRecorder())
    client_->GetMetricsRecorder()->RecordFormManagerAvailable(availability);

  if (!matched_manager) {
    RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::NO_MATCHING_FORM, submitted_form.url,
        logger.get());
    matched_manager = CreateFormManager(driver, submitted_form);
  }

  if (is_manual_fallback && matched_manager->GetFormFetcher()->GetState() ==
                                FormFetcher::State::WAITING) {
    // In case of manual fallback, the form manager has to be ready for saving.
    return nullptr;
  }

  TryToFindPredictionsToPossibleUsernameData();
  const PossibleUsernameData* possible_username =
      possible_username_ ? &possible_username_.value() : nullptr;
  if (!matched_manager->ProvisionallySave(submitted_form, driver,
                                          possible_username)) {
    return nullptr;
  }

  // Set all other form managers to no submission state.
  for (const auto& manager : form_managers_) {
    if (manager.get() != matched_manager)
      manager->set_not_submitted();
  }

  // Cache the user-visible URL (i.e., the one seen in the omnibox). Once the
  // post-submit navigation concludes, we compare the landing URL against the
  // cached and report the difference through UMA.
  main_frame_url_ = client_->GetMainFrameURL();

  ReportSubmittedFormFrameMetric(driver, *matched_manager->GetSubmittedForm());

  return matched_manager;
}

void PasswordManager::LogFirstFillingResult(PasswordManagerDriver* driver,
                                            uint32_t form_renderer_id,
                                            int32_t result) {
  PasswordFormManager* matching_manager =
      FindMatchedManagerByRendererId(form_renderer_id, form_managers_, driver);
  if (!matching_manager)
    return;
  matching_manager->GetMetricsRecorder()->RecordFirstFillingResult(result);
}

void PasswordManager::NotifyStorePasswordCalled() {
  store_password_called_ = true;
  DropFormManagers();
}

#if defined(OS_IOS)
void PasswordManager::PresaveGeneratedPassword(
    PasswordManagerDriver* driver,
    const FormData& form,
    const base::string16& generated_password,
    const base::string16& generation_element) {
  PasswordFormManager* form_manager =
      FindMatchedManager(form, form_managers_, driver);
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.GeneratedFormHasNoFormManager",
                        !form_manager);

  // TODO(https://crbug.com/886583): Create form manager if not found.
  if (form_manager) {
    form_manager->PresaveGeneratedPassword(driver, form, generated_password,
                                           generation_element);
  }
}

void PasswordManager::UpdateGeneratedPasswordOnUserInput(
    const base::string16& form_identifier,
    const base::string16& field_identifier,
    const base::string16& field_value) {
  for (std::unique_ptr<PasswordFormManager>& manager : form_managers_) {
    if (manager->UpdateGeneratedPasswordOnUserInput(
            form_identifier, field_identifier, field_value)) {
      break;
    }
  }
}

void PasswordManager::OnPasswordNoLongerGenerated(
    PasswordManagerDriver* driver) {
  for (std::unique_ptr<PasswordFormManager>& manager : form_managers_)
    manager->PasswordNoLongerGenerated();
}
#endif

bool PasswordManager::IsAutomaticSavePromptAvailable() {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_CAN_PROVISIONAL_MANAGER_SAVE_METHOD);
  }

  PasswordFormManager* submitted_manager = GetSubmittedManager();

  if (!submitted_manager) {
    if (logger) {
      logger->LogMessage(Logger::STRING_NO_PROVISIONAL_SAVE_MANAGER);
    }
    return false;
  }

  if (submitted_manager->GetFormFetcher()->GetState() ==
      FormFetcher::State::WAITING) {
    // We have a provisional save manager, but it didn't finish matching yet.
    // We just give up.
    RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::MATCHING_NOT_COMPLETE,
        submitted_manager->GetOrigin(), logger.get());
    return false;
  }

  return !submitted_manager->GetPendingCredentials().only_for_fallback;
}

bool PasswordManager::ShouldBlockPasswordForSameOriginButDifferentScheme(
    const GURL& origin) const {
  const GURL& old_origin = main_frame_url_.GetOrigin();
  return old_origin.host_piece() == origin.host_piece() &&
         old_origin.SchemeIsCryptographic() && !origin.SchemeIsCryptographic();
}

void PasswordManager::OnPasswordFormsRendered(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<PasswordForm>& visible_forms,
    bool did_stop_loading) {
  CreatePendingLoginManagers(driver, visible_forms);
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_ON_PASSWORD_FORMS_RENDERED_METHOD);
  }

  if (!IsAutomaticSavePromptAvailable())
    return;

  PasswordFormManager* submitted_manager = GetSubmittedManager();

  // If the server throws an internal error, access denied page, page not
  // found etc. after a login attempt, we do not save the credentials.
  if (client_->WasLastNavigationHTTPError()) {
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_DROP);
    submitted_manager->GetMetricsRecorder()->LogSubmitFailed();
    owned_submitted_form_manager_.reset();
    return;
  }

  if (logger) {
    logger->LogNumber(Logger::STRING_NUMBER_OF_VISIBLE_FORMS,
                      visible_forms.size());
  }

  // Record all visible forms from the frame.
  all_visible_forms_.insert(all_visible_forms_.end(), visible_forms.begin(),
                            visible_forms.end());

  if (!did_stop_loading &&
      !submitted_manager->GetSubmittedForm()
           ->form_data.is_gaia_with_skip_save_password_form) {
    // |form_data.is_gaia_with_skip_save_password_form| = true means that this
    // is a Chrome sign-in page. Chrome sign-in pages are redirected to an empty
    // pages, and for some reasons |did_stop_loading| might be false. So
    // |did_stop_loading| is ignored for them.
    return;
  }

  if (!driver->IsMainFrame() &&
      submitted_manager->driver_id() != driver->GetId()) {
    // Frames different from the main frame and the frame of the submitted form
    // are unlikely relevant to success of submission.
    return;
  }

  // If we see the login form again, then the login failed.
  if (submitted_manager->GetPendingCredentials().scheme ==
      PasswordForm::Scheme::kHtml) {
    for (const PasswordForm& form : all_visible_forms_) {
      if (submitted_manager->IsEqualToSubmittedForm(form.form_data)) {
        if (submitted_manager->IsPossibleChangePasswordFormWithoutUsername() &&
            AreAllFieldsEmpty(form.form_data)) {
          continue;
        }
        submitted_manager->GetMetricsRecorder()->LogSubmitFailed();
        if (logger) {
          logger->LogPasswordForm(Logger::STRING_PASSWORD_FORM_REAPPEARED,
                                  form);
          logger->LogMessage(Logger::STRING_DECISION_DROP);
        }
        owned_submitted_form_manager_.reset();
        // Clear all_visible_forms_ once we found the match.
        all_visible_forms_.clear();
        return;
      }
    }
  } else {
    if (logger)
      logger->LogMessage(Logger::STRING_PROVISIONALLY_SAVED_FORM_IS_NOT_HTML);
  }

  // Clear all_visible_forms_ after checking all the visible forms.
  all_visible_forms_.clear();

  // Looks like a successful login attempt. Either show an infobar or
  // automatically save the login data. We prompt when the user hasn't
  // already given consent, either through previously accepting the infobar
  // or by having the browser generate the password.
  OnLoginSuccessful();
}

void PasswordManager::OnLoginSuccessful() {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_ON_ASK_USER_OR_SAVE_PASSWORD);
  }

  PasswordFormManager* submitted_manager = GetSubmittedManager();
  DCHECK(submitted_manager);
  DCHECK(submitted_manager->GetSubmittedForm());

  client_->GetStoreResultFilter()->ReportFormLoginSuccess(*submitted_manager);
  leak_delegate_.StartLeakCheck(submitted_manager->GetPendingCredentials());

  auto submission_event =
      submitted_manager->GetSubmittedForm()->submission_event;
  metrics_util::LogPasswordSuccessfulSubmissionIndicatorEvent(submission_event);
  if (logger)
    logger->LogSuccessfulSubmissionIndicatorEvent(submission_event);

  bool able_to_save_passwords =
      client_->GetProfilePasswordStore()->IsAbleToSavePasswords();
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.AbleToSavePasswordsOnSuccessfulLogin",
                        able_to_save_passwords);
  if (!able_to_save_passwords)
    return;

  MaybeSavePasswordHash(submitted_manager);

  // TODO(https://crbug.com/831123): Implement checking whether to save with
  // PasswordFormManager.
  if (!client_->GetStoreResultFilter()->ShouldSave(
          *submitted_manager->GetSubmittedForm())) {
    RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::SYNC_CREDENTIAL,
        submitted_manager->GetOrigin(), logger.get());
    owned_submitted_form_manager_.reset();
    return;
  }

  submitted_manager->GetMetricsRecorder()->LogSubmitPassed();

  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.SuccessfulLoginHappened",
      submitted_manager->GetSubmittedForm()->origin.SchemeIsCryptographic());

  // If the form is eligible only for saving fallback, it shouldn't go here.
  DCHECK(!submitted_manager->GetPendingCredentials().only_for_fallback);

  if (ShouldPromptUserToSavePassword(*submitted_manager)) {
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_ASK);
    bool update_password = submitted_manager->IsPasswordUpdate();
    bool is_blacklisted = submitted_manager->IsBlacklisted();
    SyncState password_sync_state = client_->GetPasswordSyncState();
    if (ShouldShowOnboarding(
            client_->GetPrefs(), PasswordUpdateBool(update_password),
            BlacklistedBool(is_blacklisted), password_sync_state)) {
      if (client_->ShowOnboarding(MoveOwnedSubmittedManager())) {
        if (logger)
          logger->LogMessage(Logger::STRING_SHOW_ONBOARDING);
      }
    } else if (client_->PromptUserToSaveOrUpdatePassword(
                   MoveOwnedSubmittedManager(), update_password)) {
      if (logger)
        logger->LogMessage(Logger::STRING_SHOW_PASSWORD_PROMPT);
    }
  } else {
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_SAVE);
    submitted_manager->Save();

    if (!submitted_manager->IsNewLogin()) {
      client_->NotifySuccessfulLoginWithExistingPassword(
          submitted_manager->GetPendingCredentials());
    }

    if (submitted_manager->HasGeneratedPassword())
      client_->AutomaticPasswordSave(MoveOwnedSubmittedManager());
  }
  owned_submitted_form_manager_.reset();
}

void PasswordManager::MaybeSavePasswordHash(
    PasswordFormManager* submitted_manager) {
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  const PasswordForm* submitted_form = submitted_manager->GetSubmittedForm();
  // When |username_value| is empty, it's not clear whether the submitted
  // credentials are really Gaia or enterprise credentials. Don't save
  // password hash in that case.
  std::string username = base::UTF16ToUTF8(submitted_form->username_value);
  if (username.empty())
    return;

  password_manager::PasswordStore* store = client_->GetProfilePasswordStore();
  // May be null in tests.
  if (!store)
    return;

  bool should_save_enterprise_pw =
      client_->GetStoreResultFilter()->ShouldSaveEnterprisePasswordHash(
          *submitted_form);
  bool should_save_gaia_pw =
      client_->GetStoreResultFilter()->ShouldSaveGaiaPasswordHash(
          *submitted_form);

  if (!should_save_enterprise_pw && !should_save_gaia_pw)
    return;

  if (submitted_form->form_data.is_gaia_with_skip_save_password_form) {
    submitted_manager->GetMetricsRecorder()
        ->set_password_hash_saved_on_chrome_sing_in_page();
  }

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogMessage(Logger::STRING_SAVE_PASSWORD_HASH);
  }

  // Canonicalizes username if it is an email.
  if (username.find('@') != std::string::npos)
    username = gaia::CanonicalizeEmail(username);
  bool is_password_change = !submitted_form->new_password_element.empty();
  const base::string16 password = is_password_change
                                      ? submitted_form->new_password_value
                                      : submitted_form->password_value;

  if (should_save_enterprise_pw) {
    store->SaveEnterprisePasswordHash(username, password);
    return;
  }

  DCHECK(should_save_gaia_pw);
  GaiaPasswordHashChange event =
      client_->GetStoreResultFilter()->IsSyncAccountEmail(username)
          ? (is_password_change
                 ? GaiaPasswordHashChange::CHANGED_IN_CONTENT_AREA
                 : GaiaPasswordHashChange::SAVED_IN_CONTENT_AREA)
          : GaiaPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE;
  store->SaveGaiaPasswordHash(username, password, event);
#endif
}

void PasswordManager::ProcessAutofillPredictions(
    PasswordManagerDriver* driver,
    const std::vector<FormStructure*>& forms) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
  }

  for (const FormStructure* form : forms) {
    // |driver| might be empty on iOS or in tests.
    int driver_id = driver ? driver->GetId() : 0;
    predictions_[form->form_signature()] =
        ConvertToFormPredictions(driver_id, *form);
  }
  for (auto& manager : form_managers_)
    manager->ProcessServerPredictions(predictions_);

  // Create form managers for non-password forms with single usernames.
  for (const FormStructure* form : forms) {
    if (logger)
      logger->LogFormStructure(Logger::STRING_SERVER_PREDICTIONS, *form);
    if (form->has_password_field())
      continue;

    // Do not skip the form if it either contains a field for the Username
    // first flow or a clear-text password field.
    if (!(HasSingleUsernameVote(*form) || HasNewPasswordVote(*form)))
      continue;

    if (FindMatchedManagerByRendererId(form->unique_renderer_id(),
                                       form_managers_, driver)) {
      // The form manager is already created.
      continue;
    }

    FormData form_data = form->ToFormData();
    auto* manager = CreateFormManager(driver, form_data);
    manager->ProcessServerPredictions(predictions_);
  }
}

PasswordFormManager* PasswordManager::GetSubmittedManager() const {
  if (owned_submitted_form_manager_)
    return owned_submitted_form_manager_.get();

  for (const std::unique_ptr<PasswordFormManager>& manager : form_managers_) {
    if (manager->is_submitted())
      return manager.get();
  }

  return nullptr;
}

std::unique_ptr<PasswordFormManagerForUI>
PasswordManager::MoveOwnedSubmittedManager() {
  if (owned_submitted_form_manager_)
    return std::move(owned_submitted_form_manager_);

  for (auto iter = form_managers_.begin(); iter != form_managers_.end();
       ++iter) {
    if ((*iter)->is_submitted()) {
      std::unique_ptr<PasswordFormManager> submitted_manager = std::move(*iter);
      form_managers_.erase(iter);
      return std::move(submitted_manager);
    }
  }

  NOTREACHED();
  return nullptr;
}

void PasswordManager::RecordProvisionalSaveFailure(
    PasswordManagerMetricsRecorder::ProvisionalSaveFailure failure,
    const GURL& form_origin,
    BrowserSavePasswordProgressLogger* logger) {
  if (client_ && client_->GetMetricsRecorder()) {
    client_->GetMetricsRecorder()->RecordProvisionalSaveFailure(
        failure, main_frame_url_, form_origin, logger);
  }
}

// TODO(https://crbug.com/831123): Implement creating missing
// PasswordFormManager when PasswordFormManager is gone.
PasswordFormManager* PasswordManager::GetMatchedManager(
    const PasswordManagerDriver* driver,
    const PasswordForm& form) {
  return GetMatchedManager(driver, form.form_data);
}

PasswordFormManager* PasswordManager::GetMatchedManager(
    const PasswordManagerDriver* driver,
    const FormData& form) {
  for (auto& form_manager : form_managers_) {
    if (form_manager->DoesManage(form, driver))
      return form_manager.get();
  }
  return nullptr;
}

void PasswordManager::ReportSubmittedFormFrameMetric(
    const PasswordManagerDriver* driver,
    const PasswordForm& form) {
  if (!driver)
    return;
  metrics_util::SubmittedFormFrame frame;
  if (driver->IsMainFrame()) {
    frame = metrics_util::SubmittedFormFrame::MAIN_FRAME;
  } else if (form.origin == main_frame_url_) {
    frame =
        metrics_util::SubmittedFormFrame::IFRAME_WITH_SAME_URL_AS_MAIN_FRAME;
  } else {
    GURL::Replacements rep;
    rep.SetPathStr("");
    std::string main_frame_signon_realm =
        main_frame_url_.ReplaceComponents(rep).spec();
    frame = (main_frame_signon_realm == form.signon_realm)
                ? metrics_util::SubmittedFormFrame::
                      IFRAME_WITH_DIFFERENT_URL_SAME_SIGNON_REALM_AS_MAIN_FRAME
                : metrics_util::SubmittedFormFrame::
                      IFRAME_WITH_DIFFERENT_SIGNON_REALM;
  }
  metrics_util::LogSubmittedFormFrame(frame);
}

void PasswordManager::TryToFindPredictionsToPossibleUsernameData() {
  if (!possible_username_ || possible_username_->form_predictions)
    return;

  for (auto it : predictions_) {
    if (it.second.driver_id != possible_username_->driver_id)
      continue;
    for (const PasswordFieldPrediction& field : it.second.fields) {
      if (field.renderer_id == possible_username_->renderer_id) {
        possible_username_->form_predictions = it.second;
        return;
      }
    }
  }
}

}  // namespace password_manager
