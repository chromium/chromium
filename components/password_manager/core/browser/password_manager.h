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
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/form_submission_observer.h"
#include "components/password_manager/core/browser/login_model.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace autofill {
struct FormData;
class FormStructure;
}

namespace password_manager {

class BrowserSavePasswordProgressLogger;
class PasswordManagerClient;
class PasswordManagerDriver;
class PasswordFormManager;
class NewPasswordFormManager;

// Per-tab password manager. Handles creation and management of UI elements,
// receiving password form data from the renderer and managing the password
// database through the PasswordStore. The PasswordManager is a LoginModel
// for purposes of supporting HTTP authentication dialogs.
class PasswordManager : public LoginModel, public FormSubmissionObserver {
 public:
  // Expresses which navigation entry to use to check whether password manager
  // is enabled.
  enum class NavigationEntryToCheck { LAST_COMMITTED, VISIBLE };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void RegisterLocalPrefs(PrefRegistrySimple* registry);

  explicit PasswordManager(PasswordManagerClient* client);
  ~PasswordManager() override;

  // Called by a PasswordFormManager when it decides a HTTP auth dialog can be
  // autofilled.
  void AutofillHttpAuth(
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const autofill::PasswordForm& preferred_match) const;

  // LoginModel implementation.
  void AddObserverAndDeliverCredentials(
      LoginModelObserver* observer,
      const autofill::PasswordForm& observed_form) override;
  void RemoveObserver(LoginModelObserver* observer) override;

  void GenerationAvailableForForm(const autofill::PasswordForm& form);

  // Presaves the form with generated password. |driver| is needed to find the
  // matched form manager.
  void OnPresaveGeneratedPassword(PasswordManagerDriver* driver,
                                  const autofill::PasswordForm& form);

  // Stops treating a password as generated. |driver| is needed to find the
  // matched form manager.
  void OnPasswordNoLongerGenerated(PasswordManagerDriver* driver,
                                   const autofill::PasswordForm& form);

  // Update the generation element and whether generation was triggered
  // manually.
  void SetGenerationElementAndReasonForForm(
      PasswordManagerDriver* driver,
      const autofill::PasswordForm& form,
      const base::string16& generation_element,
      bool is_manually_triggered);

  // TODO(isherman): This should not be public, but is currently being used by
  // the LoginPrompt code.
  // When a form is submitted, we prepare to save the password but wait
  // until we decide the user has successfully logged in. This is step 1
  // of 2 (see SavePassword).
  // |driver| is optional and if it's given it should be a driver that
  // corresponds to a frame from which |form| comes from.
  void ProvisionallySavePassword(const autofill::PasswordForm& form,
                                 const PasswordManagerDriver* driver);

  // Should be called when the user navigates the main frame. Not called for
  // in-page navigation.
  void DidNavigateMainFrame();

  // Handles password forms being parsed.
  void OnPasswordFormsParsed(PasswordManagerDriver* driver,
                             const std::vector<autofill::PasswordForm>& forms);

  // Handles password forms being rendered.
  void OnPasswordFormsRendered(
      PasswordManagerDriver* driver,
      const std::vector<autofill::PasswordForm>& visible_forms,
      bool did_stop_loading);

  // Handles a password form being submitted.
  void OnPasswordFormSubmitted(PasswordManagerDriver* driver,
                               const autofill::PasswordForm& password_form);

  // Handles a password form being submitted, assumes that submission is
  // successful and does not do any checks on success of submission.
  // For example, this is called if |password_form| was filled
  // upon in-page navigation. This often means history.pushState being
  // called from JavaScript.
  void OnPasswordFormSubmittedNoChecks(
      PasswordManagerDriver* driver,
      const autofill::PasswordForm& password_form);

  // Handles a request to show manual fallback for password saving, i.e. the
  // omnibox icon with the anchored hidden prompt.
  void ShowManualFallbackForSaving(PasswordManagerDriver* driver,
                                   const autofill::PasswordForm& password_form);

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
  // TODO(crbug.com/639786): Replace using this by quering the factory for
  // mocked PasswordFormManagers.
  const std::vector<std::unique_ptr<PasswordFormManager>>&
  pending_login_managers() const {
    return pending_login_managers_;
  }

  const PasswordFormManager* provisional_save_manager() const {
    return provisional_save_manager_.get();
  }

  const std::vector<std::unique_ptr<NewPasswordFormManager>>& form_managers()
      const {
    return form_managers_;
  }

  PasswordFormManagerInterface* GetSubmittedManagerForTest() const {
    return GetSubmittedManager();
  }

  void set_skip_old_form_managers_in_tests(bool value) {
    skip_old_form_managers_in_tests_ = value;
  }

#endif

  NavigationEntryToCheck entry_to_check() const { return entry_to_check_; }

  // Reports the priority of a PasswordGenerationRequirementsSpec for a
  // generated password. See
  // PasswordFormMetricsRecorder::ReportSpecPriorityForGeneratedPassword.
  void ReportSpecPriorityForGeneratedPassword(
      const autofill::PasswordForm& password_form,
      uint32_t spec_priority);

 private:
  FRIEND_TEST_ALL_PREFIXES(
      PasswordManagerTest,
      ShouldBlockPasswordForSameOriginButDifferentSchemeTest);

  // FormSubmissionObserver:
  void OnStartNavigation(PasswordManagerDriver* driver) override;

  // Clones |matched_manager| and keeps it as |provisional_save_manager_|.
  // |form| is saved provisionally to |provisional_save_manager_|.
  void ProvisionallySaveManager(const autofill::PasswordForm& form,
                                PasswordFormManager* matched_manager,
                                BrowserSavePasswordProgressLogger* logger);

  // Returns true if |provisional_save_manager_| is ready for saving and
  // non-blacklisted.
  bool CanProvisionalManagerSave();

  // Returns true if there already exists a provisionally saved password form
  // from the same origin as |form|, but with a different and secure scheme.
  // This prevents a potential attack where users can be tricked into saving
  // unwanted credentials, see http://crbug.com/571580 for details.
  bool ShouldBlockPasswordForSameOriginButDifferentScheme(
      const autofill::PasswordForm& form) const;

  // Called when the login was deemed successful. It handles the special case
  // when the provisionally saved password is a sync credential, and otherwise
  // asks the user about saving the password or saves it directly, as
  // appropriate.
  void OnLoginSuccessful();

  // Helper function called inside OnLoginSuccessful() to save password hash
  // data from |submitted_manager| for password reuse detection purpose.
  void MaybeSavePasswordHash(
      const PasswordFormManagerInterface& submitted_manager);

  // Checks for every form in |forms| whether |pending_login_managers_| already
  // contain a manager for that form. If not, adds a manager for each such form.
  void CreatePendingLoginManagers(
      PasswordManagerDriver* driver,
      const std::vector<autofill::PasswordForm>& forms);

  // Checks for every form in |forms| whether |form_managers_| already contain a
  // manager for that form. If not, adds a manager for each such form.
  void CreateFormManagers(PasswordManagerDriver* driver,
                          const std::vector<autofill::PasswordForm>& forms);

  // Passes |submitted_form| to NewPasswordManager that manages it for using it
  // after detecting submission success for saving. |driver| is needed to
  // determine the match.
  // If the function is called multiple times, only the form from the last call
  // is considered to be submitted. Multiple calls is possible because there can
  // be multiple submitted forms on a page or our heuristics might have
  // incorrectly found submissions.
  void ProcessSubmittedForm(const autofill::FormData& submitted_form,
                            const PasswordManagerDriver* driver);

  // Returns the best match in |pending_login_managers_| for |form|. May return
  // nullptr if no match exists.
  PasswordFormManager* GetMatchingPendingManager(
      const autofill::PasswordForm& form);

  // Returns the form manager that corresponds to the submitted form. It might
  // be nullptr if there is no submitted form.
  // TODO(https://crbug.com/831123): Remove when the old PasswordFormManager is
  // gone.
  PasswordFormManagerInterface* GetSubmittedManager() const;

 private:
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

  scoped_refptr<PasswordFormMetricsRecorder>
  GetMetricRecorderFromNewPasswordFormManager(
      const autofill::FormData& form,
      const PasswordManagerDriver* driver);

  // Returns the manager which manages |form|. |driver| is needed to determine
  // the match. Returns nullptr when no matched manager is found.
  PasswordFormManagerInterface* GetMatchedManager(
      const PasswordManagerDriver* driver,
      const autofill::PasswordForm& form);

  // Note about how a PasswordFormManager can transition from
  // pending_login_managers_ to provisional_save_manager_ and the infobar.
  //
  // 1. form "seen"
  //       |                                             new
  //       |                                               ___ Infobar
  // pending_login -- form submit --> provisional_save ___/
  //             ^                            |           \___ (update DB)
  //             |                           fail
  //             |-----------<------<---------|          !new
  //
  // When a form is "seen" on a page, a PasswordFormManager is created
  // and stored in this collection until user navigates away from page.

  std::vector<std::unique_ptr<PasswordFormManager>> pending_login_managers_;

  // When the user submits a password/credential, this contains the
  // PasswordFormManager for the form in question until we deem the login
  // attempt to have succeeded (as in valid credentials). If it fails, we
  // send the PasswordFormManager back to the pending_login_managers_ set.
  // Scoped in case PasswordManager gets deleted (e.g tab closes) between the
  // time a user submits a login form and gets to the next page.
  std::unique_ptr<PasswordFormManager> provisional_save_manager_;

  // NewPasswordFormManager transition schemes:
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

  // Contains one NewPasswordFormManager per each form on the page.
  // When a form is "seen" on a page, a NewPasswordFormManager is created
  // and stored in this collection until user navigates away from page.
  std::vector<std::unique_ptr<NewPasswordFormManager>> form_managers_;

  // Corresponds to the submitted form, after navigion away before submission
  // success detection is finished.
  std::unique_ptr<NewPasswordFormManager> owned_submitted_form_manager_;

  // The embedder-level client. Must outlive this class.
  PasswordManagerClient* const client_;

  // Observers to be notified of LoginModel events.  This is mutable to allow
  // notification in const member functions.
  mutable base::ObserverList<LoginModelObserver>::Unchecked observers_;

  // Records all visible forms seen during a page load, in all frames of the
  // page. When the page stops loading, the password manager checks if one of
  // the recorded forms matches the login form from the previous page
  // (to see if the login was a failure), and clears the vector.
  std::vector<autofill::PasswordForm> all_visible_forms_;

  // The user-visible URL from the last time a password was provisionally saved.
  GURL main_frame_url_;

  // |entry_to_check_| specifies which navigation entry is relevant for
  // determining if password manager is enabled. The last commited one is
  // relevant for HTML forms, the visible one is for HTTP auth.
  NavigationEntryToCheck entry_to_check_ =
      NavigationEntryToCheck::LAST_COMMITTED;

  const bool is_new_form_parsing_for_saving_enabled_;

  // If true, it turns off using PasswordFormManager in PasswordManager. Now it
  // is used only in tests and later the old PasswordFormManager will disappear
  // and with it also this flag.
  bool skip_old_form_managers_in_tests_ = false;

  DISALLOW_COPY_AND_ASSIGN(PasswordManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_H_
