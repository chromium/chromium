// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager.h"

#include <stddef.h>

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
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/keychain_migration_status_mac.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/new_password_form_manager.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
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
using autofill::PasswordForm;
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
using password_manager::metrics_util::SyncPasswordHashChange;
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED

namespace password_manager {

namespace {

const char kSpdyProxyRealm[] = "/SpdyProxy";

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

bool URLsEqualUpToScheme(const GURL& a, const GURL& b) {
  return (a.GetContent() == b.GetContent());
}

bool URLsEqualUpToHttpHttpsSubstitution(const GURL& a, const GURL& b) {
  if (a == b)
    return true;

  // The first-time and retry login forms action URLs sometimes differ in
  // switching from HTTP to HTTPS, see http://crbug.com/400769.
  if (a.SchemeIsHTTPOrHTTPS() && b.SchemeIsHTTPOrHTTPS())
    return URLsEqualUpToScheme(a, b);

  return false;
}

// Since empty or unspecified form's action is automatically set to the page
// origin, this function checks if a form's action is empty by comparing it to
// its origin.
bool HasNonEmptyAction(const PasswordForm& form) {
  return form.action != form.origin;
}

// Checks if the observed form looks like the submitted one to handle "Invalid
// password entered" case so we don't offer a password save when we shouldn't.
bool IsPasswordFormReappeared(const PasswordForm& observed_form,
                              const PasswordForm& submitted_form) {
  if (observed_form.action.is_valid() && HasNonEmptyAction(observed_form) &&
      HasNonEmptyAction(submitted_form) &&
      URLsEqualUpToHttpHttpsSubstitution(submitted_form.action,
                                         observed_form.action)) {
    return true;
  }

  // Match the form if username and password fields are same.
  if (base::EqualsCaseInsensitiveASCII(observed_form.username_element,
                                       submitted_form.username_element) &&
      base::EqualsCaseInsensitiveASCII(observed_form.password_element,
                                       submitted_form.password_element)) {
    return true;
  }

  // Match the form if the observed username field has the same value as in
  // the submitted form.
  if (!submitted_form.username_value.empty() &&
      observed_form.username_value == submitted_form.username_value) {
    return true;
  }

  return false;
}

// Helper UMA reporting function for differences in URLs during form submission.
void RecordWhetherTargetDomainDiffers(const GURL& src, const GURL& target) {
  bool target_domain_differs =
      !net::registry_controlled_domains::SameDomainOrHost(
          src, target,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.SubmitNavigatesToDifferentDomain",
                        target_domain_differs);
}

bool IsSignupForm(const PasswordForm& form) {
  return !form.new_password_element.empty() && form.password_element.empty();
}

// Tries to find if at least one of the values from |server_field_predictions|
// can be converted from AutofillQueryResponseContents::Field::FieldPrediction
// to a PasswordFormFieldPredictionType stored in |type|. Returns true if the
// conversion was made.
bool ServerPredictionsToPasswordFormPrediction(
    std::vector<autofill::AutofillQueryResponseContents::Field::FieldPrediction>
        server_field_predictions,
    autofill::PasswordFormFieldPredictionType* type) {
  for (auto const& server_field_prediction : server_field_predictions) {
    switch (server_field_prediction.type()) {
      case autofill::USERNAME:
      case autofill::USERNAME_AND_EMAIL_ADDRESS:
        *type = autofill::PREDICTION_USERNAME;
        return true;

      case autofill::PASSWORD:
        *type = autofill::PREDICTION_CURRENT_PASSWORD;
        return true;

      case autofill::ACCOUNT_CREATION_PASSWORD:
        *type = autofill::PREDICTION_NEW_PASSWORD;
        return true;

      default:
        break;
    }
  }
  return false;
}

// Returns true if the |field_type| is known to be possibly
// misinterpreted as a password by the Password Manager.
bool IsPredictedTypeNotPasswordPrediction(
    autofill::ServerFieldType field_type) {
  return field_type == autofill::CREDIT_CARD_NUMBER ||
         field_type == autofill::CREDIT_CARD_VERIFICATION_CODE;
}

bool AreAllFieldsEmpty(const PasswordForm& form) {
  return form.username_value.empty() && form.password_value.empty() &&
         form.new_password_value.empty();
}

// Helper function that determines whether update or save prompt should be
// shown for credentials in |provisional_save_manager|.
// TODO(https://crbug.com/831123): Move to NewPasswordFormManager when the old
// PasswordFormManager is gone.
bool IsPasswordUpdate(
    const PasswordFormManagerInterface& provisional_save_manager) {
  return (!provisional_save_manager.GetBestMatches().empty() &&
          provisional_save_manager
              .IsPossibleChangePasswordFormWithoutUsername()) ||
         provisional_save_manager.IsPasswordOverridden() ||
         provisional_save_manager.RetryPasswordFormPasswordUpdate();
}

// Finds the matched form manager for |form| in |pending_login_managers|.
PasswordFormManager* FindMatchedManager(
    const PasswordForm& form,
    const std::vector<std::unique_ptr<PasswordFormManager>>&
        pending_login_managers,
    const password_manager::PasswordManagerDriver* driver,
    BrowserSavePasswordProgressLogger* logger) {
  auto matched_manager_it = pending_login_managers.end();
  PasswordFormManager::MatchResultMask current_match_result =
      PasswordFormManager::RESULT_NO_MATCH;
  // Below, "matching" is in DoesManage-sense and "not ready" in the sense of
  // FormFetcher being ready. We keep track of such PasswordFormManager
  // instances for UMA.
  for (auto iter = pending_login_managers.begin();
       iter != pending_login_managers.end(); ++iter) {
    PasswordFormManager::MatchResultMask result =
        (*iter)->DoesManage(form, driver);

    if (result == PasswordFormManager::RESULT_COMPLETE_MATCH) {
      // If we find a manager that exactly matches the submitted form including
      // the action URL, exit the loop.
      if (logger)
        logger->LogMessage(Logger::STRING_EXACT_MATCH);
      matched_manager_it = iter;
      break;
    }

    if (result > current_match_result) {
      current_match_result = result;
      matched_manager_it = iter;

      if (logger) {
        if (result == (PasswordFormManager::RESULT_COMPLETE_MATCH &
                       ~PasswordFormManager::RESULT_ACTION_MATCH))
          logger->LogMessage(Logger::STRING_MATCH_WITHOUT_ACTION);
        if (IsSignupForm(form))
          logger->LogMessage(Logger::STRING_ORIGINS_MATCH);
      }
    }
  }

  return matched_manager_it == pending_login_managers.end()
             ? nullptr
             : matched_manager_it->get();
}

std::unique_ptr<PasswordFormManager> FindAndCloneMatchedPasswordFormManager(
    const PasswordForm& password_form,
    const std::vector<std::unique_ptr<PasswordFormManager>>&
        pending_login_managers,
    const password_manager::PasswordManagerDriver* driver) {
  PasswordFormManager* matched_manager = FindMatchedManager(
      password_form, pending_login_managers, driver, nullptr);
  if (!matched_manager)
    return nullptr;
  // TODO(crbug.com/741537): Process manual saving request even if there is
  // still no response from the store.
  if (matched_manager->GetFormFetcher()->GetState() ==
      FormFetcher::State::WAITING) {
    return nullptr;
  }

  std::unique_ptr<PasswordFormManager> manager = matched_manager->Clone();
  PasswordForm form(password_form);
  form.preferred = true;
  manager->ProvisionallySave(form);
  return manager;
}

// Returns true if the user needs to be prompted before a password can be
// saved (instead of automatically saving the password), based on inspecting
// the state of |manager|.
bool ShouldPromptUserToSavePassword(
    const PasswordFormManagerInterface& manager) {
  if (IsPasswordUpdate(manager)) {
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
NewPasswordFormManager* FindMatchedManager(
    const FormData& form,
    const std::vector<std::unique_ptr<NewPasswordFormManager>>& form_managers,
    const PasswordManagerDriver* driver) {
  for (const auto& form_manager : form_managers) {
    if (form_manager->DoesManage(form, driver))
      return form_manager.get();
  }
  return nullptr;
}

// Returns a form manager that is ready to save/update credentials, provided
// that |form| is submitted form. Namely 1. Finds form manager from
// |form_managers| that manages |form| 2. Clones it. 3. Passes |form| as
// submitted form to the cloned form manager.
std::unique_ptr<NewPasswordFormManager>
FindAndCloneMatchedNewPasswordFormManager(
    const FormData& form,
    const std::vector<std::unique_ptr<NewPasswordFormManager>>& form_managers,
    const PasswordManagerDriver* driver) {
  NewPasswordFormManager* matched_manager =
      FindMatchedManager(form, form_managers, driver);
  if (!matched_manager)
    return nullptr;
  // TODO(crbug.com/741537): Process manual saving request even if there is
  // still no response from the store.
  if (matched_manager->GetFormFetcher()->GetState() ==
      FormFetcher::State::WAITING) {
    return nullptr;
  }

  std::unique_ptr<NewPasswordFormManager> manager = matched_manager->Clone();
  // Cloned NewPasswordFormManager doesn't have |driver|, so nullptr must be
  // passed to ensure that the |form| is managed.
  if (manager->SetSubmittedFormIfIsManaged(form, nullptr))
    return manager;

  return nullptr;
}

// Records the difference between how |old_manager| and |new_manager| understood
// the pending credentials.
void RecordParsingOnSavingDifference(
    const PasswordFormManagerInterface& old_manager,
    const PasswordFormManagerInterface& new_manager,
    PasswordFormMetricsRecorder* metrics_recorder) {
  const PasswordForm& old_form = old_manager.GetPendingCredentials();
  const PasswordForm& new_form = new_manager.GetPendingCredentials();
  uint64_t result = 0;

  if (old_form.username_element != new_form.username_element ||
      old_form.username_value != new_form.username_value ||
      old_form.password_element != new_form.password_element ||
      old_form.password_value != new_form.password_value) {
    result |= static_cast<int>(
        PasswordFormMetricsRecorder::ParsingOnSavingDifference::kFields);
  }
  if (old_form.signon_realm != new_form.signon_realm) {
    result |= static_cast<int>(
        PasswordFormMetricsRecorder::ParsingOnSavingDifference::kSignonRealm);
  }
  if (old_manager.IsNewLogin() != new_manager.IsNewLogin()) {
    result |= static_cast<int>(PasswordFormMetricsRecorder::
                                   ParsingOnSavingDifference::kNewLoginStatus);
  }
  if (old_manager.HasGeneratedPassword() !=
      new_manager.HasGeneratedPassword()) {
    result |= static_cast<int>(
        PasswordFormMetricsRecorder::ParsingOnSavingDifference::kGenerated);
  }

  metrics_recorder->RecordParsingOnSavingDifference(result);
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
  registry->RegisterBooleanPref(prefs::kDuplicatedBlacklistedCredentialsRemoved,
                                false);
  registry->RegisterBooleanPref(prefs::kCredentialsWithWrongSignonRealmRemoved,
                                false);
  registry->RegisterDoublePref(prefs::kLastTimeObsoleteHttpCredentialsRemoved,
                               0.0);

#if defined(OS_MACOSX)
  registry->RegisterIntegerPref(
      prefs::kKeychainMigrationStatus,
      static_cast<int>(MigrationStatus::MIGRATED_DELETED));
#endif
  registry->RegisterListPref(prefs::kPasswordHashDataList,
                             PrefRegistry::NO_REGISTRATION_FLAGS);
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
    : client_(client),
      is_new_form_parsing_for_saving_enabled_(
          base::FeatureList::IsEnabled(
              features::kNewPasswordFormParsingForSaving) &&
          base::FeatureList::IsEnabled(features::kNewPasswordFormParsing)) {
  DCHECK(client_);
}

PasswordManager::~PasswordManager() {
  for (LoginModelObserver& observer : observers_)
    observer.OnLoginModelDestroying();
}

void PasswordManager::GenerationAvailableForForm(const PasswordForm& form) {
  DCHECK(client_->IsSavingAndFillingEnabledForCurrentPage());

  PasswordFormManager* form_manager = GetMatchingPendingManager(form);
  if (form_manager) {
    form_manager->MarkGenerationAvailable();
    return;
  }
}

void PasswordManager::OnPresaveGeneratedPassword(PasswordManagerDriver* driver,
                                                 const PasswordForm& form) {
  DCHECK(client_->IsSavingAndFillingEnabledForCurrentPage());
  PasswordFormManagerInterface* form_manager = GetMatchedManager(driver, form);
  if (form_manager) {
    form_manager->PresaveGeneratedPassword(form);
    UMA_HISTOGRAM_BOOLEAN("PasswordManager.GeneratedFormHasNoFormManager",
                          false);
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("PasswordManager.GeneratedFormHasNoFormManager", true);
}

void PasswordManager::OnPasswordNoLongerGenerated(PasswordManagerDriver* driver,
                                                  const PasswordForm& form) {
  DCHECK(client_->IsSavingAndFillingEnabledForCurrentPage());

  PasswordFormManagerInterface* form_manager = GetMatchedManager(driver, form);
  if (form_manager)
    form_manager->PasswordNoLongerGenerated();
}

void PasswordManager::SetGenerationElementAndReasonForForm(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& form,
    const base::string16& generation_element,
    bool is_manually_triggered) {
  DCHECK(client_->IsSavingAndFillingEnabledForCurrentPage());

  PasswordFormManager* form_manager = GetMatchingPendingManager(form);
  if (form_manager) {
    form_manager->SetGenerationElement(generation_element);
    form_manager->SetGenerationPopupWasShown(true, is_manually_triggered);
    return;
  }

  // If there is no corresponding PasswordFormManager, we create one. This is
  // not the common case, and should only happen when there is a bug in our
  // ability to detect forms.
  auto manager = std::make_unique<PasswordFormManager>(
      this, client_, driver->AsWeakPtr(), form,
      std::make_unique<FormSaverImpl>(client_->GetPasswordStore()), nullptr);
  manager->Init(nullptr);
  pending_login_managers_.push_back(std::move(manager));
}

void PasswordManager::ProvisionallySavePassword(
    const PasswordForm& form,
    const password_manager::PasswordManagerDriver* driver) {
  // If the form was declined by some heuristics, don't show automatic bubble
  // for it, only fallback saving should be available.
  if (form.only_for_fallback_saving)
    return;

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_PROVISIONALLY_SAVE_PASSWORD_METHOD);
    logger->LogPasswordForm(Logger::STRING_PROVISIONALLY_SAVE_PASSWORD_FORM,
                            form);
  }

  if (!client_->IsSavingAndFillingEnabledForCurrentPage()) {
    RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::SAVING_DISABLED, form.origin,
        logger.get());
    return;
  }

  // No password value to save? Then don't.
  if (PasswordFormManager::PasswordToSave(form).first.empty()) {
    RecordProvisionalSaveFailure(PasswordManagerMetricsRecorder::EMPTY_PASSWORD,
                                 form.origin, logger.get());
    return;
  }

  bool should_block = ShouldBlockPasswordForSameOriginButDifferentScheme(form);
  metrics_util::LogShouldBlockPasswordForSameOriginButDifferentScheme(
      should_block);
  if (should_block) {
    RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::SAVING_ON_HTTP_AFTER_HTTPS, form.origin,
        logger.get());
    return;
  }

  PasswordFormManager* matched_manager =
      FindMatchedManager(form, pending_login_managers_, driver, logger.get());

  // If we didn't find a manager, this means a form was submitted without
  // first loading the page containing the form. Don't offer to save
  // passwords in this case.
  if (!matched_manager) {
    RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::NO_MATCHING_FORM, form.origin,
        logger.get());
    return;
  }
  matched_manager->SaveSubmittedFormTypeForMetrics(form);

  ProvisionallySaveManager(form, matched_manager, logger.get());

  // Cache the user-visible URL (i.e., the one seen in the omnibox). Once the
  // post-submit navigation concludes, we compare the landing URL against the
  // cached and report the difference through UMA.
  main_frame_url_ = client_->GetMainFrameURL();

  // Report SubmittedFormFrame metric.
  if (driver) {
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
      frame =
          (main_frame_signon_realm == form.signon_realm)
              ? metrics_util::SubmittedFormFrame::
                    IFRAME_WITH_DIFFERENT_URL_SAME_SIGNON_REALM_AS_MAIN_FRAME
              : metrics_util::SubmittedFormFrame::
                    IFRAME_WITH_DIFFERENT_SIGNON_REALM;
    }
    metrics_util::LogSubmittedFormFrame(frame);
  }
}

void PasswordManager::UpdateFormManagers() {
  std::vector<PasswordFormManagerInterface*> form_managers;
  if (base::FeatureList::IsEnabled(features::kNewPasswordFormParsing)) {
    for (const auto& form_manager : form_managers_)
      form_managers.push_back(form_manager.get());
  } else {
    for (const auto& form_manager : pending_login_managers_)
      form_managers.push_back(form_manager.get());
  }

  // Get the fetchers and all the drivers.
  std::vector<FormFetcher*> fetchers;
  std::vector<PasswordManagerDriver*> drivers;
  for (PasswordFormManagerInterface* form_manager : form_managers) {
    fetchers.push_back(form_manager->GetFormFetcher());
    for (const auto& driver : form_manager->GetDrivers()) {
      if (driver)
        drivers.push_back(driver.get());
    }
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
  pending_login_managers_.clear();
  form_managers_.clear();
  owned_submitted_form_manager_.reset();
  provisional_save_manager_.reset();
  all_visible_forms_.clear();
}

bool PasswordManager::IsPasswordFieldDetectedOnPage() {
  return !pending_login_managers_.empty();
}

void PasswordManager::AddObserverAndDeliverCredentials(
    LoginModelObserver* observer,
    const PasswordForm& observed_form) {
  observers_.AddObserver(observer);

  observer->set_signon_realm(observed_form.signon_realm);
  // TODO(vabr): Even though the observers do the realm check, this mechanism
  // will still result in every observer being notified about every form. We
  // could perhaps do better by registering an observer call-back instead.

  std::vector<PasswordForm> observed_forms;
  observed_forms.push_back(observed_form);
  OnPasswordFormsParsed(nullptr, observed_forms);
}

void PasswordManager::RemoveObserver(LoginModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PasswordManager::DidNavigateMainFrame() {
  entry_to_check_ = NavigationEntryToCheck::LAST_COMMITTED;
  pending_login_managers_.clear();

  for (std::unique_ptr<NewPasswordFormManager>& manager : form_managers_) {
    if (manager->is_submitted()) {
      owned_submitted_form_manager_ = std::move(manager);
      break;
    }
  }

  form_managers_.clear();
}

void PasswordManager::OnPasswordFormSubmitted(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  if (is_new_form_parsing_for_saving_enabled_)
    ProcessSubmittedForm(password_form.form_data, driver);

  ProvisionallySavePassword(password_form, driver);
}

void PasswordManager::OnPasswordFormSubmittedNoChecks(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogMessage(Logger::STRING_ON_SAME_DOCUMENT_NAVIGATION);
  }

  if (gaia::IsGaiaSignonRealm(GURL(password_form.signon_realm)) &&
      !IsThereVisiblePasswordField(password_form.form_data)) {
    // Gaia form without visible password fields is found.
    // It might happen only when Password Manager autofilled a username
    // (visible) and a password (invisible) fields. Then the user typed a new
    // username. A page removed the form. As result a form is inconsistent - the
    // username from one account, the password from another. Skip such form.
    return;
  }

  if (is_new_form_parsing_for_saving_enabled_)
    ProcessSubmittedForm(password_form.form_data, driver);

  ProvisionallySavePassword(password_form, driver);

  if (CanProvisionalManagerSave())
    OnLoginSuccessful();
}

void PasswordManager::ShowManualFallbackForSaving(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  if (!client_->GetPasswordStore()->IsAbleToSavePasswords() ||
      !client_->IsSavingAndFillingEnabledForCurrentPage() ||
      ShouldBlockPasswordForSameOriginButDifferentScheme(password_form) ||
      !client_->GetStoreResultFilter()->ShouldSave(password_form))
    return;

  std::unique_ptr<PasswordFormManagerInterface> manager = nullptr;
  if (is_new_form_parsing_for_saving_enabled_) {
    manager = FindAndCloneMatchedNewPasswordFormManager(password_form.form_data,
                                                        form_managers_, driver);
  } else {
    manager = FindAndCloneMatchedPasswordFormManager(
        password_form, pending_login_managers_, driver);
  }
  if (!manager)
    return;

  // Show the fallback if a prompt or a confirmation bubble should be available.
  bool has_generated_password = manager->HasGeneratedPassword();
  if (ShouldPromptUserToSavePassword(*manager) || has_generated_password) {
    bool is_update = IsPasswordUpdate(*manager);
    manager->GetMetricsRecorder()->RecordShowManualFallbackForSaving(
        has_generated_password, is_update);
    client_->ShowManualFallbackForSaving(std::move(manager),
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

  PasswordGenerationManager* password_generation_manager =
      driver ? driver->GetPasswordGenerationManager() : nullptr;
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

  if (base::FeatureList::IsEnabled(features::kNewPasswordFormParsing)) {
    CreateFormManagers(driver, forms);
  }

  const PasswordForm::Scheme effective_form_scheme =
      forms.empty() ? PasswordForm::SCHEME_HTML : forms.front().scheme;
  switch (effective_form_scheme) {
    case PasswordForm::SCHEME_HTML:
    case PasswordForm::SCHEME_OTHER:
    case PasswordForm::SCHEME_USERNAME_ONLY:
      entry_to_check_ = NavigationEntryToCheck::LAST_COMMITTED;
      break;
    case PasswordForm::SCHEME_BASIC:
    case PasswordForm::SCHEME_DIGEST:
      entry_to_check_ = NavigationEntryToCheck::VISIBLE;
      break;
  }

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

  if (!client_->IsFillingEnabledForCurrentPage())
    return;

  if (logger) {
    logger->LogNumber(Logger::STRING_OLD_NUMBER_LOGIN_MANAGERS,
                      pending_login_managers_.size());
  }

  if (skip_old_form_managers_in_tests_)
    return;

  for (const PasswordForm& form : forms) {
    // Don't involve the password manager if this form corresponds to
    // SpdyProxy authentication, as indicated by the realm.
    if (base::EndsWith(form.signon_realm, kSpdyProxyRealm,
                       base::CompareCase::SENSITIVE))
      continue;

    bool old_manager_found = false;
    for (const auto& old_manager : pending_login_managers_) {
      if (old_manager->DoesManage(form, driver) !=
          PasswordFormManager::RESULT_COMPLETE_MATCH) {
        continue;
      }
      old_manager_found = true;
      if (driver)
        old_manager->ProcessFrame(driver->AsWeakPtr());
      break;
    }
    if (old_manager_found)
      continue;  // The current form is already managed.

    UMA_HISTOGRAM_BOOLEAN("PasswordManager.EmptyUsernames.ParsedUsernameField",
                          form.username_element.empty());

    // Out of the forms not containing a username field, determine how many
    // are password change forms.
    if (form.username_element.empty()) {
      UMA_HISTOGRAM_BOOLEAN(
          "PasswordManager.EmptyUsernames."
          "FormWithoutUsernameFieldIsPasswordChangeForm",
          form.new_password_element.empty());
    }

    if (logger)
      logger->LogFormSignatures(Logger::STRING_ADDING_SIGNATURE, form);
    auto manager = std::make_unique<PasswordFormManager>(
        this, client_,
        (driver ? driver->AsWeakPtr() : base::WeakPtr<PasswordManagerDriver>()),
        form, std::make_unique<FormSaverImpl>(client_->GetPasswordStore()),
        nullptr);
    manager->Init(
        GetMetricRecorderFromNewPasswordFormManager(form.form_data, driver));
    pending_login_managers_.push_back(std::move(manager));
  }

  if (logger) {
    logger->LogNumber(Logger::STRING_NEW_NUMBER_LOGIN_MANAGERS,
                      pending_login_managers_.size());
  }
}

void PasswordManager::CreateFormManagers(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<PasswordForm>& forms) {
  // Find new forms.
  std::vector<const PasswordForm*> new_forms;
  for (const PasswordForm& form : forms) {
    // TODO(https://crbug.com/831123): Implement inside NewPasswordFormManger
    // not-filling Gaia forms that should be ignored instead of non-creating
    // NewPasswordFormManger instance.
    if (form.is_gaia_with_skip_save_password_form)
      continue;
    NewPasswordFormManager* manager =
        FindMatchedManager(form.form_data, form_managers_, driver);

    if (manager) {
      // This extra filling is just duplicating redundancy that was in
      // PasswordFormManager, that helps to fix cases when the site overrides
      // filled values.
      // TODO(https://crbug.com/831123): Implement more robust filling and
      // remove the next line.
      manager->Fill();
    } else {
      new_forms.push_back(&form);
    }
  }

  // Create form manager for new forms.
  for (const PasswordForm* new_form : new_forms) {
    form_managers_.push_back(std::make_unique<NewPasswordFormManager>(
        client_,
        driver ? driver->AsWeakPtr() : base::WeakPtr<PasswordManagerDriver>(),
        new_form->form_data, nullptr,
        std::make_unique<FormSaverImpl>(client_->GetPasswordStore()), nullptr));
    form_managers_.back()->set_old_parsing_result(*new_form);
  }
}

void PasswordManager::ProcessSubmittedForm(
    const FormData& submitted_form,
    const PasswordManagerDriver* driver) {
  NewPasswordFormManager* matching_form_manager = nullptr;
  for (const auto& manager : form_managers_) {
    if (manager->SetSubmittedFormIfIsManaged(submitted_form, driver)) {
      matching_form_manager = manager.get();
      break;
    }
  }
  if (!matching_form_manager) {
    // TODO(https://crbug.com/831123). Add metrics and implement more robust
    // handling when |matching_form_manager| is not found.
    return;
  }

  // Set all other form managers to no submission state.
  for (const auto& manager : form_managers_) {
    if (manager.get() != matching_form_manager)
      manager->set_not_submitted();
  }
}

void PasswordManager::ReportSpecPriorityForGeneratedPassword(
    const PasswordForm& password_form,
    uint32_t spec_priority) {
  PasswordFormManager* form_manager = GetMatchingPendingManager(password_form);
  if (form_manager && form_manager->GetMetricsRecorder()) {
    form_manager->GetMetricsRecorder()->ReportSpecPriorityForGeneratedPassword(
        spec_priority);
  }
}

void PasswordManager::OnStartNavigation(PasswordManagerDriver* driver) {
  // TODO(crbug/842643): use this signal instead of DidStartProvisionalLoad in
  // the renderer.
}

void PasswordManager::ProvisionallySaveManager(
    const PasswordForm& form,
    PasswordFormManager* matched_manager,
    BrowserSavePasswordProgressLogger* logger) {
  DCHECK(matched_manager);
  std::unique_ptr<PasswordFormManager> manager = matched_manager->Clone();

  PasswordForm submitted_form(form);
  submitted_form.preferred = true;
  if (logger) {
    logger->LogPasswordForm(Logger::STRING_PROVISIONALLY_SAVED_FORM,
                            submitted_form);
  }
  manager->ProvisionallySave(submitted_form);
  provisional_save_manager_.swap(manager);
}

bool PasswordManager::CanProvisionalManagerSave() {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_CAN_PROVISIONAL_MANAGER_SAVE_METHOD);
  }

  PasswordFormManagerInterface* submitted_manager = GetSubmittedManager();

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
  return true;
}

bool PasswordManager::ShouldBlockPasswordForSameOriginButDifferentScheme(
    const PasswordForm& form) const {
  const GURL& old_origin = main_frame_url_.GetOrigin();
  const GURL& new_origin = form.origin.GetOrigin();
  return old_origin.host_piece() == new_origin.host_piece() &&
         old_origin.SchemeIsCryptographic() &&
         !new_origin.SchemeIsCryptographic();
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

  if (!CanProvisionalManagerSave())
    return;

  PasswordFormManagerInterface* submitted_manager = GetSubmittedManager();

  // If the server throws an internal error, access denied page, page not
  // found etc. after a login attempt, we do not save the credentials.
  if (client_->WasLastNavigationHTTPError()) {
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_DROP);
    submitted_manager->GetMetricsRecorder()->LogSubmitFailed();
    provisional_save_manager_.reset();
    owned_submitted_form_manager_.reset();
    return;
  }

  if (logger) {
    logger->LogNumber(Logger::STRING_NUMBER_OF_VISIBLE_FORMS,
                      visible_forms.size());
  }

  // Record all visible forms from the frame.
  all_visible_forms_.insert(all_visible_forms_.end(),
                            visible_forms.begin(),
                            visible_forms.end());

  if (!did_stop_loading)
    return;

  // If we see the login form again, then the login failed.
  if (submitted_manager->GetPendingCredentials().scheme ==
      PasswordForm::SCHEME_HTML) {
    for (const PasswordForm& form : all_visible_forms_) {
      if (IsPasswordFormReappeared(
              form, submitted_manager->GetPendingCredentials())) {
        if (submitted_manager->IsPossibleChangePasswordFormWithoutUsername() &&
            AreAllFieldsEmpty(form)) {
          continue;
        }
        submitted_manager->GetMetricsRecorder()->LogSubmitFailed();
        if (logger) {
          logger->LogPasswordForm(Logger::STRING_PASSWORD_FORM_REAPPEARED,
                                  form);
          logger->LogMessage(Logger::STRING_DECISION_DROP);
        }
        provisional_save_manager_.reset();
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

  PasswordFormManagerInterface* submitted_manager = GetSubmittedManager();
  DCHECK(submitted_manager);
  DCHECK(submitted_manager->GetSubmittedForm());

  client_->GetStoreResultFilter()->ReportFormLoginSuccess(*submitted_manager);

  auto submission_event =
      submitted_manager->GetSubmittedForm()->submission_event;
  metrics_util::LogPasswordSuccessfulSubmissionIndicatorEvent(submission_event);
  if (logger)
    logger->LogSuccessfulSubmissionIndicatorEvent(submission_event);

  bool able_to_save_passwords =
      client_->GetPasswordStore()->IsAbleToSavePasswords();
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.AbleToSavePasswordsOnSuccessfulLogin",
                        able_to_save_passwords);
  if (!able_to_save_passwords)
    return;

  MaybeSavePasswordHash(*submitted_manager);

  // TODO(https://crbug.com/831123): Implement checking whether to save with
  // NewPasswordFormManager.
  if (!client_->GetStoreResultFilter()->ShouldSave(
          *submitted_manager->GetSubmittedForm())) {
    RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::SYNC_CREDENTIAL,
        submitted_manager->GetOrigin(), logger.get());
    provisional_save_manager_.reset();
    owned_submitted_form_manager_.reset();
    return;
  }

  submitted_manager->GetMetricsRecorder()->LogSubmitPassed();

  RecordWhetherTargetDomainDiffers(main_frame_url_, client_->GetMainFrameURL());
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.SuccessfulLoginHappened",
      submitted_manager->GetSubmittedForm()->origin.SchemeIsCryptographic());

  // If the form is eligible only for saving fallback, it shouldn't go here.
  DCHECK(!submitted_manager->GetPendingCredentials().only_for_fallback_saving);

  // TODO(https://crbug.com/831123): Remove logging when the old form parsing is
  // removed.
  if (is_new_form_parsing_for_saving_enabled_) {
    // In this case, |submitted_manager| points to a NewPasswordFormManager and
    // |provisional_save_manager_| to a PasswordFormManager. They use the new
    // and the old FormData parser, respectively. Log the differences using UKM
    // to be alerted of regressions early.
    if (provisional_save_manager_) {
      RecordParsingOnSavingDifference(*provisional_save_manager_,
                                      *submitted_manager,
                                      submitted_manager->GetMetricsRecorder());
    }
  }

  if (ShouldPromptUserToSavePassword(*submitted_manager)) {
    bool empty_password =
        submitted_manager->GetPendingCredentials().username_value.empty();
    UMA_HISTOGRAM_BOOLEAN("PasswordManager.EmptyUsernames.OfferedToSave",
                          empty_password);
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_ASK);
    bool update_password = IsPasswordUpdate(*submitted_manager);
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
          submitted_manager->GetPendingCredentials());
    }

    if (submitted_manager->HasGeneratedPassword()) {
      client_->AutomaticPasswordSave(MoveOwnedSubmittedManager());
    } else {
      provisional_save_manager_.reset();
      owned_submitted_form_manager_.reset();
    }
  }
}

void PasswordManager::MaybeSavePasswordHash(
    const PasswordFormManagerInterface& submitted_manager) {
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // When |username_value| is empty, it's not clear whether the submitted
  // credentials are really Gaia or enterprise credentials. Don't save
  // password hash in that case.
  std::string username =
      base::UTF16ToUTF8(submitted_manager.GetSubmittedForm()->username_value);
  if (username.empty())
    return;

  password_manager::PasswordStore* store = client_->GetPasswordStore();
  // May be null in tests.
  if (!store)
    return;

  const PasswordForm* password_form = submitted_manager.GetSubmittedForm();

  bool should_save_enterprise_pw =
      client_->GetStoreResultFilter()->ShouldSaveEnterprisePasswordHash(
          *password_form);
  bool should_save_gaia_pw =
      client_->GetStoreResultFilter()->ShouldSaveGaiaPasswordHash(
          *password_form);

  if (!should_save_enterprise_pw && !should_save_gaia_pw)
    return;

  // Canonicalizes username if it is an email.
  if (username.find('@') != std::string::npos)
    username = gaia::CanonicalizeEmail(username);
  bool is_password_change = !password_form->new_password_element.empty();
  const base::string16 password = is_password_change
                                      ? password_form->new_password_value
                                      : password_form->password_value;

  if (should_save_enterprise_pw) {
    store->SaveEnterprisePasswordHash(username, password);
    return;
  }

  DCHECK(should_save_gaia_pw);
  SyncPasswordHashChange event =
      client_->GetStoreResultFilter()->IsSyncAccountEmail(username)
          ? (is_password_change
                 ? SyncPasswordHashChange::CHANGED_IN_CONTENT_AREA
                 : SyncPasswordHashChange::SAVED_IN_CONTENT_AREA)
          : SyncPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE;
  store->SaveGaiaPasswordHash(username, password, event);
#endif
}

void PasswordManager::AutofillHttpAuth(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const PasswordForm& preferred_match) const {
  DCHECK_NE(PasswordForm::SCHEME_HTML, preferred_match.scheme);

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_PASSWORDMANAGER_AUTOFILLHTTPAUTH);
    logger->LogBoolean(Logger::STRING_LOGINMODELOBSERVER_PRESENT,
                       observers_.might_have_observers());
  }

  for (LoginModelObserver& observer : observers_)
    observer.OnAutofillDataAvailable(preferred_match);
  DCHECK(!best_matches.empty());
  client_->PasswordWasAutofilled(best_matches,
                                 best_matches.begin()->second->origin, nullptr);
}

void PasswordManager::ProcessAutofillPredictions(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<autofill::FormStructure*>& forms) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_))
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));

  if (base::FeatureList::IsEnabled(
          password_manager::features::kNewPasswordFormParsing)) {
    for (auto& manager : form_managers_)
      manager->ProcessServerPredictions(forms);
  }

  // Leave only forms that contain fields that are useful for password manager.
  std::map<FormData, autofill::PasswordFormFieldPredictionMap> predictions;
  for (const autofill::FormStructure* form : forms) {
    if (logger)
      logger->LogFormStructure(Logger::STRING_SERVER_PREDICTIONS, *form);
    for (const auto& field : *form) {
      autofill::PasswordFormFieldPredictionType prediction_type;
      if (ServerPredictionsToPasswordFormPrediction(field->server_predictions(),
                                                    &prediction_type)) {
        predictions[form->ToFormData()][*field] = prediction_type;
      }
      // Certain fields are annotated by the browsers as "not passwords" i.e.
      // they should not be treated as passwords by the Password Manager.
      if (field->form_control_type == "password" &&
          IsPredictedTypeNotPasswordPrediction(
              field->Type().GetStorableType())) {
        predictions[form->ToFormData()][*field] =
            autofill::PREDICTION_NOT_PASSWORD;
      }
    }
  }
  if (predictions.empty())
    return;
  driver->AutofillDataReceived(predictions);
}

PasswordFormManager* PasswordManager::GetMatchingPendingManager(
    const PasswordForm& form) {
  PasswordFormManager* matched_manager = nullptr;
  PasswordFormManager::MatchResultMask current_match_result =
      PasswordFormManager::RESULT_NO_MATCH;

  for (auto& login_manager : pending_login_managers_) {
    PasswordFormManager::MatchResultMask result =
        login_manager->DoesManage(form, nullptr);

    if (result == PasswordFormManager::RESULT_NO_MATCH)
      continue;

    if (result == PasswordFormManager::RESULT_COMPLETE_MATCH) {
      // If we find a manager that exactly matches the submitted form including
      // the action URL, exit the loop.
      matched_manager = login_manager.get();
      break;
    } else if (result == (PasswordFormManager::RESULT_COMPLETE_MATCH &
                          ~PasswordFormManager::RESULT_ACTION_MATCH) &&
               result > current_match_result) {
      // If the current manager matches the submitted form excluding the action
      // URL, remember it as a candidate and continue searching for an exact
      // match. See http://crbug.com/27246 for an example where actions can
      // change.
      matched_manager = login_manager.get();
      current_match_result = result;
    } else if (result > current_match_result) {
      matched_manager = login_manager.get();
      current_match_result = result;
    }
  }
  return matched_manager;
}

PasswordFormManagerInterface* PasswordManager::GetSubmittedManager() const {
  if (!is_new_form_parsing_for_saving_enabled_)
    return provisional_save_manager_.get();

  if (owned_submitted_form_manager_)
    return owned_submitted_form_manager_.get();

  for (const std::unique_ptr<NewPasswordFormManager>& manager :
       form_managers_) {
    if (manager->is_submitted())
      return manager.get();
  }

  return nullptr;
}

std::unique_ptr<PasswordFormManagerForUI>
PasswordManager::MoveOwnedSubmittedManager() {
  if (!is_new_form_parsing_for_saving_enabled_)
    return std::move(provisional_save_manager_);

  if (owned_submitted_form_manager_)
    return std::move(owned_submitted_form_manager_);

  for (auto iter = form_managers_.begin(); iter != form_managers_.end();
       ++iter) {
    if ((*iter)->is_submitted()) {
      std::unique_ptr<NewPasswordFormManager> submitted_manager =
          std::move(*iter);
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

scoped_refptr<PasswordFormMetricsRecorder>
PasswordManager::GetMetricRecorderFromNewPasswordFormManager(
    const autofill::FormData& form,
    const PasswordManagerDriver* driver) {
  for (auto& form_manager : form_managers_) {
    if (form_manager->DoesManage(form, driver))
      return form_manager->metrics_recorder();
  }

  return nullptr;
}

PasswordFormManagerInterface* PasswordManager::GetMatchedManager(
    const PasswordManagerDriver* driver,
    const autofill::PasswordForm& form) {
  if (!is_new_form_parsing_for_saving_enabled_)
    return GetMatchingPendingManager(form);

  for (auto& form_manager : form_managers_) {
    if (form_manager->DoesManage(form.form_data, driver))
      return form_manager.get();
  }
  return nullptr;
}

}  // namespace password_manager
