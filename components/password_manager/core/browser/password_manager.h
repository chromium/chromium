// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/renderer_id.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/form_submission_observer.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection_delegate.h"
#include "components/password_manager/core/browser/password_form_forward.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/possible_username_data.h"

class PrefRegistrySimple;

namespace base {
class TimeDelta;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace autofill {
struct FormData;
class FormStructure;
}  // namespace autofill

namespace password_manager {

class BrowserSavePasswordProgressLogger;
class PasswordManagerClient;
class PasswordManagerDriver;
class PasswordFormManagerForUI;
class PasswordFormManager;
class PasswordManagerMetricsRecorder;
struct PossibleUsernameData;

// Define the modes of collaboration between Password Manager and Autofill
// Assistant (who handles form submissions, whether to show prompts or not).
enum class AutofillAssistantMode {
  // Autofill Assistant UI is not being shown. Password Manager operates in the
  // regular
  // mode - it handles submissions and shows prompts.
  kUINotShown = 0,
  // Autofill Assistant UI is being shown. The password manager
  // is basically off - it does not handle submissions and therefore does not
  // show prompts. The script does all the work instead.
  kUIShown
};

// Per-tab password manager. Handles creation and management of UI elements,
// receiving password form data from the renderer and managing the password
// database through the PasswordStore.
class PasswordManager : public PasswordManagerInterface {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void RegisterLocalPrefs(PrefRegistrySimple* registry);

  explicit PasswordManager(PasswordManagerClient* client);
  ~PasswordManager() override;

  // FormSubmissionObserver:
  void DidNavigateMainFrame(bool form_may_be_submitted) override;

  // PasswordManagerInterface:
  void OnPasswordFormsParsed(
      PasswordManagerDriver* driver,
      const std::vector<autofill::FormData>& forms_data) override;
  void OnPasswordFormsRendered(
      PasswordManagerDriver* driver,
      const std::vector<autofill::FormData>& visible_forms_data,
      bool did_stop_loading) override;
  void OnPasswordFormSubmitted(PasswordManagerDriver* driver,
                               const autofill::FormData& form_data) override;
#if defined(OS_IOS)
  void OnPasswordFormSubmittedNoChecksForiOS(
      PasswordManagerDriver* driver,
      const autofill::FormData& form_data) override;
  void PresaveGeneratedPassword(
      PasswordManagerDriver* driver,
      const autofill::FormData& form,
      const base::string16& generated_password,
      autofill::FieldRendererId generation_element) override;
  void UpdateStateOnUserInput(PasswordManagerDriver* driver,
                              autofill::FormRendererId form_id,
                              autofill::FieldRendererId field_id,
                              const base::string16& field_value) override;
  void OnPasswordNoLongerGenerated(PasswordManagerDriver* driver) override;
  void OnPasswordFormRemoved(
      PasswordManagerDriver* driver,
      const autofill::FieldDataManager* field_data_manager,
      autofill::FormRendererId form_id) override;
  void OnIframeDetach(
      const std::string& frame_id,
      PasswordManagerDriver* driver,
      const autofill::FieldDataManager* field_data_manager) override;
#endif

  // Notifies the renderer to start the generation flow or pops up additional UI
  // in case there is a danger to overwrite an existing password.
  void OnGeneratedPasswordAccepted(
      PasswordManagerDriver* driver,
      const autofill::FormData& form_data,
      autofill::FieldRendererId generation_element_id,
      const base::string16& password);

  // Presaves the form with generated password. |driver| is needed to find the
  // matched form manager.
  void OnPresaveGeneratedPassword(PasswordManagerDriver* driver,
                                  const autofill::FormData& form,
                                  const base::string16& generated_password);

  // Stops treating a password as generated. |driver| is needed to find the
  // matched form manager.
  void OnPasswordNoLongerGenerated(PasswordManagerDriver* driver,
                                   const autofill::FormData& form_data);

  // Update the generation element and whether generation was triggered
  // manually.
  void SetGenerationElementAndReasonForForm(
      PasswordManagerDriver* driver,
      const autofill::FormData& form_data,
      autofill::FieldRendererId generation_element,
      bool is_manually_triggered);

  // Called upon navigation to persist the state from |CredentialCache|
  // used to decide when to record
  // |PasswordManager.ResultOfSavingFlowAfterUnblacklistin|.
  void MarkWasUnblacklistedInFormManagers(CredentialCache* credential_cache);

  // Handles a password form being submitted, assumes that submission is
  // successful and does not do any checks on success of submission. For
  // example, this is called if |password_form| was filled upon in-page
  // navigation. This often means history.pushState being called from
  // JavaScript.
  // TODO(crbug.com/949519): Rename this method together with
  // SameDocumentNavigation in autofill::mojom::PasswordManagerDriver
  void OnPasswordFormSubmittedNoChecks(
      PasswordManagerDriver* driver,
      autofill::mojom::SubmissionIndicatorEvent event);

  // Called when a user changed a value in a non-password field. The field is in
  // a frame corresponding to |driver| and has a renderer id |renderer_id|.
  // |value| is the current value of the field.
  void OnUserModifiedNonPasswordField(PasswordManagerDriver* driver,
                                      autofill::FieldRendererId renderer_id,
                                      const base::string16& value);

  // Handles user input and decides whether to show manual fallback for password
  // saving, i.e. the omnibox icon with the anchored hidden prompt.
  void OnInformAboutUserInput(PasswordManagerDriver* driver,
                              const autofill::FormData& form_data);

  // Handles a request to hide manual fallback for password saving.
  void HideManualFallbackForSaving();

  void ProcessAutofillPredictions(
      PasswordManagerDriver* driver,
      const std::vector<autofill::FormStructure*>& forms);

  // Causes all |pending_login_managers_| to query the password store again.
  // Results in updating the fill information on the page.
  void UpdateFormManagers();

  // Cleans the state by removing all the PasswordFormManager instances and
  // visible forms.
  void DropFormManagers();

  // Returns true if password element is detected on the current page.
  bool IsPasswordFieldDetectedOnPage();

  PasswordManagerClient* client() { return client_; }

#if defined(UNIT_TEST)
  const std::vector<std::unique_ptr<PasswordFormManager>>& form_managers()
      const {
    return form_managers_;
  }

  PasswordFormManager* GetSubmittedManagerForTest() const {
    return GetSubmittedManager();
  }

  void set_leak_factory(std::unique_ptr<LeakDetectionCheckFactory> factory) {
    leak_delegate_.set_leak_factory(std::move(factory));
  }

#endif  // defined(UNIT_TEST)

#if !defined(OS_IOS)
  // Reports the success from the renderer's PasswordAutofillAgent to fill
  // credentials into a site. This may be called multiple times, but only
  // the first result will be recorded for each PasswordFormManager.
  void LogFirstFillingResult(PasswordManagerDriver* driver,
                             autofill::FormRendererId form_renderer_id,
                             int32_t result);
#endif  // !defined(OS_IOS)

  // Notifies that Credential Management API function store() is called.
  void NotifyStorePasswordCalled();

  // Sets the Autofill Assistant mode to disable prompts while |mode=kRunning|.
  // A script finish will clear pending credentials in all form managers.
  void SetAutofillAssistantMode(AutofillAssistantMode mode);

  // Returns the currently set autofill-assistant mode.
  AutofillAssistantMode GetAutofillAssistantMode() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      PasswordManagerTest,
      ShouldBlockPasswordForSameOriginButDifferentSchemeTest);

  // Returns true if there is a form manager for a submitted form and this form
  // manager contains the submitted credentials suitable for automatic save
  // prompt, not for manual fallback only.
  bool IsAutomaticSavePromptAvailable();

  // Returns true if there already exists a provisionally saved password form
  // from the origin |origin|, but with a different and secure scheme.
  // This prevents a potential attack where users can be tricked into saving
  // unwanted credentials, see http://crbug.com/571580 and [1] for details.
  //
  // [1] docs.google.com/document/d/1ei3PcUNMdgmSKaWSb-A4KhowLXaBMFxDdt5hvU_0YY8
  bool ShouldBlockPasswordForSameOriginButDifferentScheme(
      const GURL& origin) const;

  // Called when the login was deemed successful. It handles the special case
  // when the provisionally saved password is a sync credential, and otherwise
  // asks the user about saving the password or saves it directly, as
  // appropriate.
  void OnLoginSuccessful();

  // Helper function called inside OnLoginSuccessful() to save password hash
  // data from |submitted_manager| for password reuse detection purpose.
  void MaybeSavePasswordHash(PasswordFormManager* submitted_manager);

  // Checks for every form in |forms_data| whether |pending_login_managers_|
  // already contain a manager for that form. If not, adds a manager for each
  // such form.
  void CreatePendingLoginManagers(
      PasswordManagerDriver* driver,
      const std::vector<autofill::FormData>& forms_data);

  // Checks for every form in |forms_data| whether |form_managers_| already
  // contain a manager for that form. If not, adds a manager for each such form.
  void CreateFormManagers(PasswordManagerDriver* driver,
                          const std::vector<autofill::FormData>& forms_data);

  // Create PasswordFormManager for |form|, adds the newly created one to
  // |form_managers_| and returns it.
  PasswordFormManager* CreateFormManager(PasswordManagerDriver* driver,
                                         const autofill::FormData& form);

  // Passes |form| to PasswordFormManager that manages it for using it after
  // detecting submission success for saving. |driver| is needed to determine
  // the match. If the function is called multiple times, only the form from the
  // last call is provisionally saved. Multiple calls is possible because it is
  // called on any user keystroke. If there is no PasswordFormManager that
  // manages |form|, the new one is created. If |is_manual_fallback| is true
  // and the matched form manager has not recieved yet response from the
  // password store, then nullptr is returned. Returns manager which manages
  // |form|.
  PasswordFormManager* ProvisionallySaveForm(const autofill::FormData& form,
                                             PasswordManagerDriver* driver,
                                             bool is_manual_fallback);

  // Returns the form manager that corresponds to the submitted form. It might
  // be nullptr if there is no submitted form.
  // TODO(https://crbug.com/831123): Remove when the old PasswordFormManager is
  // gone.
  PasswordFormManager* GetSubmittedManager() const;

  // Returns the form manager that corresponds to the submitted form. It also
  // sets |submitted_form_manager_| to nullptr.
  // TODO(https://crbug.com/831123): Remove when the old PasswordFormManager is
  // gone.
  std::unique_ptr<PasswordFormManagerForUI> MoveOwnedSubmittedManager();

  // Records provisional save failure using current |client_| and
  // |main_frame_url_|.
  void RecordProvisionalSaveFailure(
      PasswordManagerMetricsRecorder::ProvisionalSaveFailure failure,
      const GURL& form_origin,
      BrowserSavePasswordProgressLogger* logger);

  // Returns the manager which manages |form|. |driver| is needed to determine
  // the match. Returns nullptr when no matched manager is found.
  PasswordFormManager* GetMatchedManager(PasswordManagerDriver* driver,
                                         const autofill::FormData& form);

  // Log a frame (main frame, iframe) of a submitted password form.
  void ReportSubmittedFormFrameMetric(const PasswordManagerDriver* driver,
                                      const PasswordForm& form);

  //  If |possible_username_.form_predictions| is missing, this functions tries
  //  to find predictions for the form which contains |possible_username_| in
  //  |predictions_|.
  void TryToFindPredictionsToPossibleUsernameData();

  // Handles a request to show manual fallback for password saving, i.e. the
  // omnibox icon with the anchored hidden prompt. todo
  void ShowManualFallbackForSaving(PasswordFormManager* form_manager,
                                   const autofill::FormData& form_data);

  // Returns the timeout for the disabling Password Manager's prompts.
  base::TimeDelta GetTimeoutForDisablingPrompts();

  // Resets |autofill_assistant_mode_| to the default.
  void ResetAutofillAssistantMode();

#if defined(OS_IOS)
  // Even though the formal submission might not happen, the manager
  // could still be provisionally saved on user input or have autofilled data,
  // in this case submission might be considered successful and a save prompt
  // might be shown.
  bool DetectPotentialSubmission(
      PasswordFormManager* form_manager,
      const autofill::FieldDataManager* field_data_manager,
      PasswordManagerDriver* driver);
#endif

  // PasswordFormManager transition schemes:
  // 1. HTML submission with navigation afterwads.
  // form "seen"
  //      |
  // form_managers -- submit --> (is_submitted = true) -- navigation --
  //
  //                                                          __ Prompt.
  // owned_submitted_form_manager --> (successful submission) __ Automatic save.
  //
  // 2.Other submssions detection types (XHR, frame detached, in-page
  // navigation etc) without navigation.
  // form "seen"
  //      |
  // form_managers -- successful submission (success is detected in the Render)
  //                            ____ Prompt.
  //  --> (is_submitted = true) ---- Automatic save.

  // Contains one PasswordFormManager per each form on the page.
  // When a form is "seen" on a page, a PasswordFormManager is created
  // and stored in this collection until user navigates away from page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers_;

  // Corresponds to the submitted form, after navigion away before submission
  // success detection is finished.
  std::unique_ptr<PasswordFormManager> owned_submitted_form_manager_;

  // The embedder-level client. Must outlive this class.
  PasswordManagerClient* const client_;

  // Records all visible forms seen during a page load, in all frames of the
  // page. When the page stops loading, the password manager checks if one of
  // the recorded forms matches the login form from the previous page
  // (to see if the login was a failure), and clears the vector.
  std::vector<autofill::FormData> visible_forms_data_;

  // Server predictions for the forms on the page.
  std::map<autofill::FormSignature, FormPredictions> predictions_;

  // The URL of the last submitted form.
  GURL submitted_form_url_;

  // True if Credential Management API function store() was called. In this case
  // PasswordManager does not need to show a save/update prompt since
  // CredentialManagerImpl takes care of it.
  bool store_password_called_ = false;

  // Helper for making the requests on leak detection.
  LeakDetectionDelegate leak_delegate_;

  base::Optional<PossibleUsernameData> possible_username_;

  // By default Autofill Assistant is not running. Password Manager handles
  // submissions and shows prompts.
  AutofillAssistantMode autofill_assistant_mode_ =
      AutofillAssistantMode::kUINotShown;

  DISALLOW_COPY_AND_ASSIGN(PasswordManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_H_
