// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_INTERFACE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_submission_observer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace password_manager {

class PasswordManagerClient;

// Abstract interface for PasswordManagers.
class PasswordManagerInterface : public FormSubmissionObserver {
 public:
  PasswordManagerInterface() = default;
  ~PasswordManagerInterface() override = default;

  PasswordManagerInterface(const PasswordManagerInterface&) = delete;
  PasswordManagerInterface& operator=(const PasswordManagerInterface&) = delete;

  // Cleans the state by removing all the PasswordFormManager instances and
  // visible forms.
  virtual void DropFormManagers() = 0;

  // Returns form cache containing information about parsed password forms on
  // the web page.
  virtual const PasswordFormCache* GetPasswordFormCache() const = 0;

  // Returns true if password element is detected on the current page.
  virtual bool IsPasswordFieldDetectedOnPage() const = 0;

#if BUILDFLAG(USE_BLINK)
  // Reports the success from the renderer's PasswordAutofillAgent to fill
  // credentials into a site. This may be called multiple times, but only
  // the first result will be recorded for each PasswordFormManager.
  virtual void LogFirstFillingResult(PasswordManagerDriver* driver,
                                     autofill::FormRendererId form_renderer_id,
                                     int32_t result) = 0;
#endif  // BUILDFLAG(USE_BLINK)

  // Notifies that Credential Management API function store() is called.
  virtual void NotifyStorePasswordCalled() = 0;

  // Handles a dynamic form submission. In contrast to OnPasswordFormSubmitted()
  // this method does not wait for OnPasswordFormsRendered() before invoking
  // OnLoginSuccessful(), provided that a password form was provisionally saved
  // in the past. Since this is commonly invoked for same document navigations,
  // detachment of frames or hiding a form following an XHR, it does not make
  // sense to await a full page navigation event.
  virtual void OnDynamicFormSubmission(
      PasswordManagerDriver* driver,
      autofill::mojom::SubmissionIndicatorEvent event) = 0;

  // Notifies the renderer to start the generation flow or pops up additional UI
  // in case there is a danger to overwrite an existing password.
  virtual void OnGeneratedPasswordAccepted(
      PasswordManagerDriver* driver,
      const autofill::FormData& form_data,
      autofill::FieldRendererId generation_element_id,
      const std::u16string& password) = 0;

  // Handles user input and decides whether to show manual fallback for password
  // saving, i.e. the omnibox icon with the anchored hidden prompt.
  virtual void OnInformAboutUserInput(PasswordManagerDriver* driver,
                                      const autofill::FormData& form_data) = 0;

  // Handles password forms being parsed.
  virtual void OnPasswordFormsParsed(
      PasswordManagerDriver* driver,
      const std::vector<autofill::FormData>& forms_data) = 0;

  // Handles password forms being rendered.
  virtual void OnPasswordFormsRendered(
      PasswordManagerDriver* driver,
      const std::vector<autofill::FormData>& visible_forms_data) = 0;

  // Handles a password form being submitted.
  virtual void OnPasswordFormSubmitted(PasswordManagerDriver* driver,
                                       const autofill::FormData& form_data) = 0;

  // Handles a password form being cleared by page scripts.
  virtual void OnPasswordFormCleared(PasswordManagerDriver* driver,
                                     const autofill::FormData& form_data) = 0;

  // Called when a user changed a value in a non-password field. The field is in
  // a frame corresponding to |driver| and has a renderer id |renderer_id|.
  // |value| is the current value of the field.
  virtual void OnUserModifiedNonPasswordField(
      PasswordManagerDriver* driver,
      autofill::FieldRendererId renderer_id,
      const std::u16string& value,
      bool autocomplete_attribute_has_username,
      bool is_likely_otp) = 0;

  // Update the `generation_element` and `type` for `form_id`.
  virtual void SetGenerationElementAndTypeForForm(
      PasswordManagerDriver* driver,
      autofill::FormRendererId form_id,
      autofill::FieldRendererId generation_element,
      autofill::password_generation::PasswordGenerationType type) = 0;

  // Presaves the form with generated password. |driver| is needed to find the
  // matched form manager.
  virtual void OnPresaveGeneratedPassword(
      PasswordManagerDriver* driver,
      const autofill::FormData& form,
      const std::u16string& generated_password) = 0;

  // Processes the server predictions received from Autofill.
  virtual void ProcessAutofillPredictions(
      PasswordManagerDriver* driver,
      const autofill::FormData& form,
      const base::flat_map<autofill::FieldGlobalId,
                           autofill::AutofillType::ServerPrediction>&
          field_predictions) = 0;

  // Getter for the PasswordManagerClient.
  virtual PasswordManagerClient* GetClient() = 0;

  // Returns the observed parsed password form to which the field with the
  // renderer id `field_id` belongs.
  virtual const PasswordForm* GetParsedObservedForm(
      PasswordManagerDriver* driver,
      autofill::FieldRendererId field_id) const = 0;
  // Returns the submitted form.
  virtual std::optional<password_manager::PasswordForm>
  GetSubmittedCredentials() const = 0;

  // Checks whether all |FormFetcher|s belonging to the |driver|-corresponding
  // frame have finished fetching logins.
  // Used to determine whether manual password generation can be offered
  // Automatic password generation already waits for that signal.
  virtual bool HaveFormManagersReceivedData(
      const PasswordManagerDriver* driver) const = 0;

#if BUILDFLAG(IS_IOS)
  // Handles a subframe form submission. In contrast to OnPasswordFormSubmitted
  // this method does not wait for OnPasswordFormsRendered before invoking
  // OnLoginSuccessful), but rather invokes ProvisionallySave immediately and
  // then calls OnLoginSuccessful if applicable. It is the iOS pendant to
  // PasswordManager::OnDynamicFormSubmission.
  virtual void OnSubframeFormSubmission(
      PasswordManagerDriver* driver,
      const autofill::FormData& form_data) = 0;

  // Updates the state in the PasswordFormManager which corresponds to the form
  // with `form_id` if available or the one that has `field_id` as a fallback.
  // In case there is a presaved credential, it updates the presaved credential.
  // Cross-platform method PasswordManager::OnInformAboutUserInput cannot
  // replace this method, as it needs an observed FormData object on every
  // keystroke and parsing the full FormData on iOS is more expensive operation,
  // than in Blink.
  virtual void UpdateStateOnUserInput(
      PasswordManagerDriver* driver,
      const autofill::FieldDataManager& field_data_manager,
      std::optional<autofill::FormRendererId> form_id,
      autofill::FieldRendererId field_id,
      const std::u16string& field_value) = 0;

  // Stops treating a password as generated.
  virtual void OnPasswordNoLongerGenerated() = 0;

  // Call when one or more forms are removed. This class will determine whether
  // any of them were submitted.
  //  - removed_forms: The renderer identifiers of the removed password forms.
  //  - removed_unowned_fields: The renderer identifiers of the removed form
  //  fields not owned by a form element. Used to detect formless form
  //  submissions.
  virtual void OnPasswordFormsRemoved(
      PasswordManagerDriver* driver,
      const autofill::FieldDataManager& field_data_manager,
      const std::set<autofill::FormRendererId>& removed_forms,
      const std::set<autofill::FieldRendererId>& removed_unowned_fields) = 0;

  // Checks if there is a submitted PasswordFormManager for a form from the
  // detached frame.
  virtual void OnIframeDetach(
      const std::string& frame_id,
      PasswordManagerDriver* driver,
      const autofill::FieldDataManager& field_data_manager) = 0;

  // Propagates all available field data manager info to existing form managers
  // and provisionally saves them if the relevant data is retrieved.
  virtual void PropagateFieldDataManagerInfo(
      const autofill::FieldDataManager& field_data_manager,
      const PasswordManagerDriver* driver) = 0;
#endif
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_INTERFACE_H_
