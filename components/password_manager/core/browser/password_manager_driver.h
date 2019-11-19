// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/util/type_safety/strong_alias.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"

namespace autofill {
class AutofillDriver;
struct PasswordFormGenerationData;
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {

class PasswordAutofillManager;
class PasswordGenerationFrameHelper;
class PasswordManager;

// Interface that allows PasswordManager core code to interact with its driver
// (i.e., obtain information from it and give information to it).
class PasswordManagerDriver
    : public base::SupportsWeakPtr<PasswordManagerDriver> {
 public:
  using ShowVirtualKeyboard =
      util::StrongAlias<class ShowVirtualKeyboardTag, bool>;

  PasswordManagerDriver() = default;
  virtual ~PasswordManagerDriver() = default;

  // Returns driver id which is unique in the current tab.
  virtual int GetId() const = 0;

  // Fills forms matching |form_data|.
  virtual void FillPasswordForm(
      const autofill::PasswordFormFillData& form_data) = 0;

  // Informs the driver that there are no saved credentials in the password
  // store for the current page.
  // TODO(https://crbug.com/621355): Remove and observe FormFetcher instead.
  virtual void InformNoSavedCredentials() {}

  // Notifies the driver that a password can be generated on the fields
  // identified by |form|.
  virtual void FormEligibleForGenerationFound(
      const autofill::PasswordFormGenerationData& form) {}

  // Notifies the driver that the user has accepted a generated password.
  // TODO(crbug/936011): delete this method. The UI should call the one below.
  virtual void GeneratedPasswordAccepted(const base::string16& password) = 0;

  // Notifies the password manager that the user has accepted a generated
  // password. The password manager can bring up some disambiguation UI in
  // response.
  virtual void GeneratedPasswordAccepted(const autofill::FormData& form_data,
                                         uint32_t generation_element_id,
                                         const base::string16& password) {}

  virtual void TouchToFillClosed(ShowVirtualKeyboard show_virtual_keyboard) {}

  // Tells the driver to fill the form with the |username| and |password|.
  virtual void FillSuggestion(const base::string16& username,
                              const base::string16& password) = 0;

  // Tells the renderer to fill the given credential into the focused element.
  // Always calls |completed_callback| with a status indicating success/error.
  virtual void FillIntoFocusedField(
      bool is_password,
      const base::string16& user_provided_credential) {}

  // Tells the driver to preview filling form with the |username| and
  // |password|.
  virtual void PreviewSuggestion(const base::string16& username,
                                 const base::string16& password) = 0;

  // Tells the driver to clear previewed password and username fields.
  virtual void ClearPreviewedForm() = 0;

  // Returns the PasswordGenerationFrameHelper associated with this instance.
  virtual PasswordGenerationFrameHelper* GetPasswordGenerationHelper() = 0;

  // Returns the PasswordManager associated with this instance.
  virtual PasswordManager* GetPasswordManager() = 0;

  // Returns the PasswordAutofillManager associated with this instance.
  virtual PasswordAutofillManager* GetPasswordAutofillManager() = 0;

  // Sends a message to the renderer whether logging to
  // chrome://password-manager-internals is available.
  virtual void SendLoggingAvailability() {}

  // Return the associated AutofillDriver.
  virtual autofill::AutofillDriver* GetAutofillDriver() = 0;

  // Return true iff the driver corresponds to the main frame.
  virtual bool IsMainFrame() const = 0;

  // Returns the last committed URL of the frame.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Annotate password related (username, password) DOM input elements with
  // corresponding HTML attributes. It is used only for debugging.
  virtual void AnnotateFieldsWithParsingResult(
      const autofill::ParsingResult& parsing_result) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerDriver);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_
