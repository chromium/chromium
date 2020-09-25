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
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

#if defined(OS_WIN)
#include "components/prefs/pref_registry_simple.h"
#endif

using autofill::ACCOUNT_CREATION_PASSWORD;
using autofill::FieldDataManager;
using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormRendererId;
using autofill::FormStructure;
using autofill::NEW_PASSWORD;
using autofill::NOT_USERNAME;
using autofill::SINGLE_USERNAME;
using autofill::UNKNOWN_TYPE;
using autofill::USERNAME;
using autofill::mojom::PasswordFormFieldPredictionType;
using base::NumberToString;
using BlacklistedStatus =
    password_manager::OriginCredentialStore::BlacklistedStatus;
#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
using password_manager::metrics_util::GaiaPasswordHashChange;
#endif  // PASSWORD_REUSE_DETECTION_ENABLED

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

#if !defined(OS_IOS)
// Finds the matched form manager with id |form_renderer_id| in
// |form_managers|.
PasswordFormManager* FindMatchedManagerByRendererId(
    autofill::FormRendererId form_renderer_id,
    const std::vector<std::unique_ptr<PasswordFormManager>>& form_managers,
    const PasswordManagerDriver* driver) {
  for (const auto& form_manager : form_managers) {
    if (form_manager->DoesManageAccordingToRendererId(form_renderer_id, driver))
      return form_manager.get();
  }
  return nullptr;
}
#endif  // !defined(OS_IOS)

bool HasSingleUsernameVote(const FormPredictions& form) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kUsernameFirstFlow)) {
    return false;
  }
  for (const auto& field : form.fields) {
    if (field.type == autofill::SINGLE_USERNAME)
      return true;
  }
  return false;
}

// Returns true if at least one of the fields in |form| has a prediction to be a
// new-password related field.
bool HasNewPasswordVote(const FormPredictions& form) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              KEnablePasswordGenerationForClearTextFields))
    return false;
  for (const auto& field : form.fields) {
    if (field.type == ACCOUNT_CREATION_PASSWORD || field.type == NEW_PASSWORD)
      return true;
  }
  return false;
}

// Adds predictions to |predictions->fields| if |field_info_manager| has
// predictions for corresponding fields. Predictions from |field_info_manager|
// have priority over server predictions.
void AddLocallySavedPredictions(FieldInfoManager* field_info_manager,
                                FormPredictions* predictions,
                                BrowserSavePasswordProgressLogger* logger) {
  DCHECK(predictions);
  if (!field_info_manager)
    return;

  for (PasswordFieldPrediction& field : predictions->fields) {
    auto local_prediction = field_info_manager->GetFieldType(
        predictions->form_signature, field.signature);
    if (local_prediction == SINGLE_USERNAME) {
      field.type = SINGLE_USERNAME;
    } else if (local_prediction == NOT_USERNAME) {
      // Now local prediction NOT_USERNAME is based on the weak signal (the user
      // ignored or rejected the prompt) so use it only if the server does not
      // have data.
      if (field.type != SINGLE_USERNAME && field.type != USERNAME)
        field.type = NOT_USERNAME;
    }
    if (logger && local_prediction != UNKNOWN_TYPE) {
      std::string message = base::StrCat(
          {"form signature=",
           NumberToString(predictions->form_signature.value()),
           " , field signature=", NumberToString(field.signature.value()),
           ", type=",
           autofill::AutofillType::ServerFieldTypeToString(local_prediction)});
      logger->LogString(Logger::STRING_LOCALLY_SAVED_PREDICTION, message);
    }
  }
}

FormData SimplifiedFormDataFromFormStructure(
    const FormStructure& form_structure) {
  FormData form_data;
  form_data.name = form_structure.form_name();
  form_data.is_form_tag = form_structure.is_form_tag();
  form_data.unique_renderer_id = form_structure.unique_renderer_id();
  return form_data;
}

}  // namespace

// static
void PasswordManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
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
  registry->RegisterDoublePref(prefs::kLastTimePasswordCheckCompleted, 0.0);

  registry->RegisterDictionaryPref(prefs::kAccountStoragePerAccountSettings);

  registry->RegisterTimePref(prefs::kProfileStoreDateLastUsedForFilling,
                             base::Time());
  registry->RegisterTimePref(prefs::kAccountStoreDateLastUsedForFilling,
                             base::Time());

  registry->RegisterIntegerPref(prefs::kSettingsLaunchedPasswordChecks, 0);

#if defined(OS_APPLE)
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

#if defined(OS_MAC)
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
    autofill::FieldRendererId generation_element_id,
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

void PasswordManager::OnPresaveGeneratedPassword(
    PasswordManagerDriver* driver,
    const FormData& form_data,
    const base::string16& generated_password) {
  DCHECK(client_->IsSavingAndFillingEnabled(form_data.url));
  PasswordFormManager* form_manager = GetMatchedManager(driver, form_data);
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.GeneratedFormHasNoFormManager",
                        !form_manager);
  if (form_manager)
    form_manager->PresaveGeneratedPassword(form_data, generated_password);
}

void PasswordManager::OnPasswordNoLongerGenerated(PasswordManagerDriver* driver,
                                                  const FormData& form_data) {
  DCHECK(client_->IsSavingAndFillingEnabled(form_data.url));

  PasswordFormManager* form_manager = GetMatchedManager(driver, form_data);
  if (form_manager)
    form_manager->PasswordNoLongerGenerated();
}

void PasswordManager::SetGenerationElementAndReasonForForm(
    password_manager::PasswordManagerDriver* driver,
    const FormData& form_data,
    FieldRendererId generation_element,
    bool is_manually_triggered) {
  DCHECK(client_->IsSavingAndFillingEnabled(form_data.url));

  PasswordFormManager* form_manager = GetMatchedManager(driver, form_data);
  if (form_manager) {
    form_manager->SetGenerationElement(generation_element);
    form_manager->SetGenerationPopupWasShown(is_manually_triggered);
  }
}

void PasswordManager::MarkWasUnblacklistedInFormManagers(
    CredentialCache* credential_cache) {
  if (owned_submitted_form_manager_) {
    const OriginCredentialStore& credential_store =
        credential_cache->GetCredentialStore(
            url::Origin::Create(owned_submitted_form_manager_->GetURL()));
    if (credential_store.GetBlacklistedStatus() ==
        BlacklistedStatus::kWasBlacklisted) {
      owned_submitted_form_manager_->MarkWasUnblacklisted();
    }
  }

  for (const auto& form_manager : form_managers_) {
    const OriginCredentialStore& credential_store =
        credential_cache->GetCredentialStore(
            url::Origin::Create(form_manager->GetURL()));
    if (credential_store.GetBlacklistedStatus() ==
        BlacklistedStatus::kWasBlacklisted) {
      form_manager->MarkWasUnblacklisted();
    }
  }
}

void PasswordManager::DidNavigateMainFrame(bool form_may_be_submitted) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger = std::make_unique<BrowserSavePasswordProgressLogger>(
        client_->GetLogManager());
    logger->LogBoolean(Logger::STRING_DID_NAVIGATE_MAIN_FRAME,
                       form_may_be_submitted);
  }

  if (client_->IsNewTabPage()) {
    if (logger)
      logger->LogMessage(Logger::STRING_NAVIGATION_NTP);
    // On a successful Chrome sign-in the page navigates to the new tab page
    // (ntp). OnPasswordFormsRendered is not called on ntp. That is
    // why the standard flow for saving hash does not work. Save a password hash
    // now since a navigation to ntp is the sign of successful sign-in.
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
  UMA_HISTOGRAM_COUNTS_1000("PasswordManager.NumFormManagersCleared",
                            form_managers_.size());
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
  visible_forms_data_.clear();
  TryToFindPredictionsToPossibleUsernameData();
  predictions_.clear();
}

bool PasswordManager::IsPasswordFieldDetectedOnPage() {
  return !form_managers_.empty();
}

void PasswordManager::OnPasswordFormSubmitted(PasswordManagerDriver* driver,
                                              const FormData& form_data) {
  ProvisionallySaveForm(form_data, driver, false);
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
    PasswordManagerDriver* driver,
    const FormData& form_data) {
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogMessage(Logger::STRING_ON_SAME_DOCUMENT_NAVIGATION);
  }

  ProvisionallySaveForm(form_data, driver, false);

  if (IsAutomaticSavePromptAvailable())
    OnLoginSuccessful();
}
#endif

void PasswordManager::OnUserModifiedNonPasswordField(
    PasswordManagerDriver* driver,
    autofill::FieldRendererId renderer_id,
    const base::string16& value) {
  // |driver| might be empty on iOS or in tests.
  int driver_id = driver ? driver->GetId() : 0;
  possible_username_.emplace(GetSignonRealm(driver->GetLastCommittedURL()),
                             renderer_id, value, base::Time::Now(), driver_id);
}

void PasswordManager::OnInformAboutUserInput(PasswordManagerDriver* driver,
                                             const FormData& form_data) {
  PasswordFormManager* manager = ProvisionallySaveForm(form_data, driver, true);

  if (manager && form_data.is_gaia_with_skip_save_password_form) {
    manager->GetMetricsRecorder()
        ->set_user_typed_password_on_chrome_sign_in_page();
  }

  auto availability =
      manager ? PasswordManagerMetricsRecorder::FormManagerAvailable::kSuccess
              : PasswordManagerMetricsRecorder::FormManagerAvailable::
                    kMissingManual;
  if (client_ && client_->GetMetricsRecorder())
    client_->GetMetricsRecorder()->RecordFormManagerAvailable(availability);

  ShowManualFallbackForSaving(manager, form_data);
}

void PasswordManager::HideManualFallbackForSaving() {
  client_->HideManualFallbackForSaving();
}

void PasswordManager::OnPasswordFormsParsed(
    PasswordManagerDriver* driver,
    const std::vector<FormData>& form_data) {
  CreatePendingLoginManagers(driver, form_data);

  PasswordGenerationFrameHelper* password_generation_manager =
      driver ? driver->GetPasswordGenerationHelper() : nullptr;
  if (password_generation_manager) {
    password_generation_manager->PrefetchSpec(
        client_->GetLastCommittedOrigin().GetURL());
  }
}

void PasswordManager::CreatePendingLoginManagers(
    PasswordManagerDriver* driver,
    const std::vector<FormData>& forms_data) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger = std::make_unique<BrowserSavePasswordProgressLogger>(
        client_->GetLogManager());
    logger->LogMessage(Logger::STRING_CREATE_LOGIN_MANAGERS_METHOD);
  }

  CreateFormManagers(driver, forms_data);

  // Record whether or not this top-level URL has at least one password field.
  client_->AnnotateNavigationEntry(!forms_data.empty());

  // Only report SSL error status for cases where there are potentially forms to
  // fill or save from.
  if (!forms_data.empty()) {
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
    PasswordManagerDriver* driver,
    const std::vector<FormData>& forms_data) {
  // Find new forms.
  std::vector<const FormData*> new_forms_data;
  for (const FormData& form_data : forms_data) {
    if (!client_->IsFillingEnabled(form_data.url))
      continue;

    PasswordFormManager* manager = GetMatchedManager(driver, form_data);

    if (manager) {
      // This extra filling is just duplicating redundancy that was in
      // PasswordFormManager, that helps to fix cases when the site overrides
      // filled values.
      // TODO(https://crbug.com/831123): Implement more robust filling and
      // remove the next line.
      manager->FillForm(form_data);
    } else {
      new_forms_data.push_back(&form_data);
    }
  }

  // Create form manager for new forms.
  for (const FormData* new_form_data : new_forms_data)
    CreateFormManager(driver, *new_form_data);
}

PasswordFormManager* PasswordManager::CreateFormManager(
    PasswordManagerDriver* driver,
    const autofill::FormData& form) {
  form_managers_.push_back(std::make_unique<PasswordFormManager>(
      client_,
      driver ? driver->AsWeakPtr() : base::WeakPtr<PasswordManagerDriver>(),
      form, nullptr,
      PasswordSaveManagerImpl::CreatePasswordSaveManagerImpl(client_),
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
    logger = std::make_unique<BrowserSavePasswordProgressLogger>(
        client_->GetLogManager());
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

  const GURL& submitted_url = submitted_form.url;
  if (ShouldBlockPasswordForSameOriginButDifferentScheme(submitted_url)) {
    RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::SAVING_ON_HTTP_AFTER_HTTPS,
        submitted_url, logger.get());
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

  // Cache the committed URL. Once the post-submit navigation concludes, we
  // compare the landing URL against the cached and report the difference.
  submitted_form_url_ = submitted_url;

  ReportSubmittedFormFrameMetric(driver, *matched_manager->GetSubmittedForm());

  return matched_manager;
}

#if !defined(OS_IOS)
void PasswordManager::LogFirstFillingResult(
    PasswordManagerDriver* driver,
    autofill::FormRendererId form_renderer_id,
    int32_t result) {
  PasswordFormManager* matching_manager =
      FindMatchedManagerByRendererId(form_renderer_id, form_managers_, driver);
  if (!matching_manager)
    return;
  matching_manager->GetMetricsRecorder()->RecordFirstFillingResult(result);
}
#endif  // !defined(OS_IOS)

void PasswordManager::NotifyStorePasswordCalled() {
  store_password_called_ = true;
  DropFormManagers();
}

#if defined(OS_IOS)
void PasswordManager::PresaveGeneratedPassword(
    PasswordManagerDriver* driver,
    const FormData& form,
    const base::string16& generated_password,
    FieldRendererId generation_element) {
  PasswordFormManager* form_manager = GetMatchedManager(driver, form);
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.GeneratedFormHasNoFormManager",
                        !form_manager);

  // TODO(https://crbug.com/886583): Create form manager if not found.
  if (form_manager) {
    form_manager->PresaveGeneratedPassword(driver, form, generated_password,
                                           generation_element);
  }
}

void PasswordManager::UpdateStateOnUserInput(
    PasswordManagerDriver* driver,
    FormRendererId form_id,
    FieldRendererId field_id,
    const base::string16& field_value) {
  for (std::unique_ptr<PasswordFormManager>& manager : form_managers_) {
    if (manager->UpdateStateOnUserInput(form_id, field_id, field_value)) {
      ProvisionallySaveForm(*manager->observed_form(), driver, true);
      if (manager->is_submitted() && !manager->HasGeneratedPassword()) {
        ShowManualFallbackForSaving(manager.get(), *manager->observed_form());
      } else {
        HideManualFallbackForSaving();
      }
      break;
    }
  }
}

void PasswordManager::OnPasswordNoLongerGenerated(
    PasswordManagerDriver* driver) {
  for (std::unique_ptr<PasswordFormManager>& manager : form_managers_)
    manager->PasswordNoLongerGenerated();
}

void PasswordManager::OnPasswordFormRemoved(
    PasswordManagerDriver* driver,
    const FieldDataManager* field_data_manager,
    FormRendererId form_id) {
  for (auto& manager : form_managers_) {
    if (driver && !manager->GetDriver())
      manager->SetDriver(driver->AsWeakPtr());
    // Find a form with corresponding renderer id.
    if (manager->DoesManageAccordingToRendererId(form_id, driver)) {
      DetectPotentialSubmission(manager.get(), field_data_manager, driver);
      return;
    }
  }
}

void PasswordManager::OnIframeDetach(
    const std::string& frame_id,
    PasswordManagerDriver* driver,
    const FieldDataManager* field_data_manager) {
  for (auto& manager : form_managers_) {
    // Find a form with corresponding frame id. Stop iterating in case the
    // target form manager was found to avoid crbug.com/1129758 and since only
    // one password form is being submitted at a time.
    if (manager->observed_form()->frame_id == frame_id &&
        DetectPotentialSubmission(manager.get(), field_data_manager, driver)) {
      return;
    }
  }
}
#endif

bool PasswordManager::IsAutomaticSavePromptAvailable() {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger = std::make_unique<BrowserSavePasswordProgressLogger>(
        client_->GetLogManager());
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
        submitted_manager->GetURL(), logger.get());
    return false;
  }

  return !submitted_manager->GetPendingCredentials().only_for_fallback;
}

bool PasswordManager::ShouldBlockPasswordForSameOriginButDifferentScheme(
    const GURL& url) const {
  return submitted_form_url_.host_piece() == url.host_piece() &&
         submitted_form_url_.SchemeIsCryptographic() &&
         !url.SchemeIsCryptographic();
}

void PasswordManager::OnPasswordFormsRendered(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<FormData>& visible_forms_data,
    bool did_stop_loading) {
  CreatePendingLoginManagers(driver, visible_forms_data);
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger = std::make_unique<BrowserSavePasswordProgressLogger>(
        client_->GetLogManager());
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
                      visible_forms_data.size());
  }

  // Record all visible forms from the frame.
  visible_forms_data_.insert(visible_forms_data_.end(),
                             visible_forms_data.begin(),
                             visible_forms_data.end());
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
    for (const FormData& form_data : visible_forms_data_) {
      if (submitted_manager->IsEqualToSubmittedForm(form_data)) {
        if (submitted_manager->IsPossibleChangePasswordFormWithoutUsername() &&
            AreAllFieldsEmpty(form_data)) {
          continue;
        }
        submitted_manager->GetMetricsRecorder()->LogSubmitFailed();
        if (logger) {
          logger->LogFormData(Logger::STRING_PASSWORD_FORM_REAPPEARED,
                              form_data);
          logger->LogMessage(Logger::STRING_DECISION_DROP);
        }
        owned_submitted_form_manager_.reset();
        // Clear visible_forms_data_ once we found the match.
        visible_forms_data_.clear();
        return;
      }
    }
  } else {
    if (logger)
      logger->LogMessage(Logger::STRING_PROVISIONALLY_SAVED_FORM_IS_NOT_HTML);
  }
  // Clear visible_forms_data_ after checking all the visible forms.
  visible_forms_data_.clear();

  // Looks like a successful login attempt. Either show an infobar or
  // automatically save the login data. We prompt when the user hasn't
  // already given consent, either through previously accepting the infobar
  // or by having the browser generate the password.
  OnLoginSuccessful();
}

void PasswordManager::OnLoginSuccessful() {
  if (autofill_assistant_mode_ == AutofillAssistantMode::kUIShown) {
    // Suppress prompts while Autofill Assistant is running.
    return;
  }

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger = std::make_unique<BrowserSavePasswordProgressLogger>(
        client_->GetLogManager());
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
  // Check whether the filter allows saving this credential. In practice, this
  // prevents saving the password of the syncing account. However, if the
  // password is already saved, then *updating* it is still allowed - better
  // than keeping an outdated password around.
  if (!submitted_manager->IsPasswordUpdate() &&
      !client_->GetStoreResultFilter()->ShouldSave(
          *submitted_manager->GetSubmittedForm())) {
    RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::SYNC_CREDENTIAL,
        submitted_manager->GetURL(), logger.get());
    owned_submitted_form_manager_.reset();
    return;
  }

  submitted_manager->GetMetricsRecorder()->LogSubmitPassed();

  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.SuccessfulLoginHappened",
      submitted_manager->GetSubmittedForm()->url.SchemeIsCryptographic());

  // If the form is eligible only for saving fallback, it shouldn't go here.
  DCHECK(!submitted_manager->GetPendingCredentials().only_for_fallback);

  if (ShouldPromptUserToSavePassword(*submitted_manager)) {
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_ASK);
    bool update_password = submitted_manager->IsPasswordUpdate();
    if (client_->PromptUserToSaveOrUpdatePassword(MoveOwnedSubmittedManager(),
                                                  update_password)) {
      if (logger)
        logger->LogMessage(Logger::STRING_SHOW_PASSWORD_PROMPT);
    }
  } else {
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_SAVE);
    submitted_manager->Save();

    if (!submitted_manager->IsNewLogin()) {
      client_->NotifySuccessfulLoginWithExistingPassword(
          submitted_manager->Clone());
    }

    if (submitted_manager->HasGeneratedPassword())
      client_->AutomaticPasswordSave(MoveOwnedSubmittedManager());
  }
  owned_submitted_form_manager_.reset();
}

void PasswordManager::MaybeSavePasswordHash(
    PasswordFormManager* submitted_manager) {
#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
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
  bool is_sync_account_email =
      client_->GetStoreResultFilter()->IsSyncAccountEmail(username);
  GaiaPasswordHashChange event =
      is_sync_account_email
          ? (is_password_change
                 ? GaiaPasswordHashChange::CHANGED_IN_CONTENT_AREA
                 : GaiaPasswordHashChange::SAVED_IN_CONTENT_AREA)
          : (is_password_change
                 ? GaiaPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE
                 : GaiaPasswordHashChange::SAVED_IN_CONTENT_AREA);
  store->SaveGaiaPasswordHash(username, password, is_sync_account_email, event);
#endif
}

void PasswordManager::ProcessAutofillPredictions(
    PasswordManagerDriver* driver,
    const std::vector<FormStructure*>& forms) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger = std::make_unique<BrowserSavePasswordProgressLogger>(
        client_->GetLogManager());
  }

  for (const FormStructure* form : forms) {
    // |driver| might be empty on iOS or in tests.
    int driver_id = driver ? driver->GetId() : 0;
    predictions_[form->form_signature()] =
        ConvertToFormPredictions(driver_id, *form);
    AddLocallySavedPredictions(client_->GetFieldInfoManager(),
                               &predictions_[form->form_signature()],
                               logger.get());
  }

  // Create form managers for non-password forms if |predictions_| has evidence
  // that these forms are password related.
  for (const FormStructure* form : forms) {
    if (logger)
      logger->LogFormStructure(Logger::STRING_SERVER_PREDICTIONS, *form);
    FormData form_data = SimplifiedFormDataFromFormStructure(*form);
    if (GetMatchedManager(driver, form_data)) {
      // The form manager is already created.
      continue;
    }

    if (form->has_password_field())
      continue;

    const FormPredictions* form_predictions =
        &predictions_[form->form_signature()];
    // Do not skip the form if it either contains a field for the Username
    // first flow or a clear-text password field.
    if (!(HasSingleUsernameVote(*form_predictions) ||
          HasNewPasswordVote(*form_predictions))) {
      continue;
    }

    CreateFormManager(driver, form->ToFormData());
  }

  for (auto& manager : form_managers_)
    manager->ProcessServerPredictions(predictions_);
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
        failure, submitted_form_url_, form_origin, logger);
  }
}

// TODO(https://crbug.com/831123): Implement creating missing
// PasswordFormManager when PasswordFormManager is gone.
PasswordFormManager* PasswordManager::GetMatchedManager(
    PasswordManagerDriver* driver,
    const FormData& form) {
  for (auto& form_manager : form_managers_) {
// Until support of cross-origin iframes is implemented, there is only one
// driver on iOS. It needs to be set in order for filling to work.
#if defined(OS_IOS)
    if (driver && !form_manager->GetDriver())
      form_manager->SetDriver(driver->AsWeakPtr());
#endif
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
  } else if (form.url == client()->GetLastCommittedURL()) {
    frame =
        metrics_util::SubmittedFormFrame::IFRAME_WITH_SAME_URL_AS_MAIN_FRAME;
  } else {
    std::string main_frame_signon_realm =
        GetSignonRealm(client()->GetLastCommittedURL());
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

void PasswordManager::ShowManualFallbackForSaving(
    PasswordFormManager* form_manager,
    const FormData& form_data) {
  // Where `form_manager` is nullptr, make sure the manual fallback isn't
  // shown. One scenario where this is relevant is when the user inputs some
  // password and then removes it. Upon removing the password, the
  // `form_manager` will become nullptr.
  if (!form_manager) {
    HideManualFallbackForSaving();
    return;
  }

  if (!form_manager->is_submitted())
    return;

  if (!client_->GetProfilePasswordStore()->IsAbleToSavePasswords() ||
      !client_->IsSavingAndFillingEnabled(form_data.url) ||
      ShouldBlockPasswordForSameOriginButDifferentScheme(form_data.url)) {
    return;
  }

  if (!client_->GetStoreResultFilter()->ShouldSave(
          *form_manager->GetSubmittedForm())) {
    return;
  }

  // Show the fallback if a prompt or a confirmation bubble should be available.
  bool has_generated_password = form_manager->HasGeneratedPassword();
  if (ShouldPromptUserToSavePassword(*form_manager) || has_generated_password) {
    bool is_update = form_manager->IsPasswordUpdate();
    form_manager->GetMetricsRecorder()->RecordShowManualFallbackForSaving(
        has_generated_password, is_update);
    client_->ShowManualFallbackForSaving(form_manager->Clone(),
                                         has_generated_password, is_update);
  } else {
    HideManualFallbackForSaving();
  }
}

void PasswordManager::SetAutofillAssistantMode(AutofillAssistantMode mode) {
  if (autofill_assistant_mode_ == mode) {
    return;
  }
  autofill_assistant_mode_ = mode;

  if (autofill_assistant_mode_ == AutofillAssistantMode::kUINotShown) {
    // Reset pending credentials as Autofill Assistant has handled the pending
    // submission.
    for (auto& form_manager : form_managers_)
      form_manager->ResetState();
    owned_submitted_form_manager_.reset();
  }
}

AutofillAssistantMode PasswordManager::GetAutofillAssistantMode() const {
  return autofill_assistant_mode_;
}

#if defined(OS_IOS)
bool PasswordManager::DetectPotentialSubmission(
    PasswordFormManager* form_manager,
    const FieldDataManager* field_data_manager,
    PasswordManagerDriver* driver) {
  // If the manager is not submitted, it still can have autofilled data.
  if (!form_manager->is_submitted()) {
    form_manager->UpdateObservedFormDataWithFieldDataManagerInfo(
        field_data_manager);
    // Provisionally save form and set the manager to be submitted if valid
    // data was recovered.
    form_manager->ProvisionallySave(*form_manager->observed_form(), driver,
                                    nullptr);
  }
  // If the manager was set to be submitted, either prior to this function call
  // or on provisional save above, consider submission successful.
  if (form_manager->is_submitted()) {
    OnLoginSuccessful();
    return true;
  }
  return false;
}
#endif

}  // namespace password_manager
