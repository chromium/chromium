// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_submission_observer.h"
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

  // Update the `generation_element` and `type` for `form_id`.
  virtual void SetGenerationElementAndTypeForForm(
      PasswordManagerDriver* driver,
      autofill::FormRendererId form_id,
      autofill::FieldRendererId generation_element,
      autofill::password_generation::PasswordGenerationType type) = 0;

  // Getter for the PasswordManagerClient.
  virtual PasswordManagerClient* GetClient() = 0;

#if BUILDFLAG(IS_IOS)
  // Handles a subframe form submission. In contrast to OnPasswordFormSubmitted
  // this method does not wait for OnPasswordFormsRendered before invoking
  // OnLoginSuccessful), but rather invokes ProvisionallySave immediately and
  // then calls OnLoginSuccessful if applicable. It is the iOS pendant to
  // PasswordManager::OnDynamicFormSubmission.
  virtual void OnSubframeFormSubmission(
      PasswordManagerDriver* driver,
      const autofill::FormData& form_data) = 0;

  // Presaves the form with |generated_password|. This function is called once
  // when the user accepts the generated password. The password was generated in
  // the field with identifier |generation_element|. |driver| corresponds to the
  // |form| parent frame.
  virtual void PresaveGeneratedPassword(
      PasswordManagerDriver* driver,
      const autofill::FormData& form,
      const std::u16string& generated_password,
      autofill::FieldRendererId generation_element) = 0;

  // Updates the state in the PasswordFormManager which corresponds to the form
  // with |form_identifier|. In case there is a presaved credential, it
  // updates the presaved credential.
  // Cross-platform method PasswordManager::OnInformAboutUserInput cannot
  // replace this method, as it needs an observed FormData object on every
  // keystroke and parsing the full FormData on iOS is more expensive operation,
  // than in Blink.
  virtual void UpdateStateOnUserInput(PasswordManagerDriver* driver,
                                      autofill::FormRendererId form_id,
                                      autofill::FieldRendererId field_id,
                                      const std::u16string& field_value) = 0;

  // Stops treating a password as generated. |driver| corresponds to the
  // form parent frame.
  virtual void OnPasswordNoLongerGenerated(PasswordManagerDriver* driver) = 0;

  // Call when a form is removed so that this class can decide if whether or not
  // the form was submitted.
  virtual void OnPasswordFormRemoved(
      PasswordManagerDriver* driver,
      const autofill::FieldDataManager& field_data_manager,
      autofill::FormRendererId form_id) = 0;

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
