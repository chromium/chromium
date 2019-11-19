// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_CLIENT_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_CLIENT_UI_DELEGATE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordFormManagerForUI;
}

// An interface for ChromePasswordManagerClient implemented by
// ManagePasswordsUIController. Allows to push a new state for the tab.
class PasswordsClientUIDelegate {
 public:
  // Called when the user submits a form containing login information, so the
  // later requests to save or blacklist can be handled.
  // This stores the provided object and triggers the UI to prompt the user
  // about whether they would like to save the password.
  virtual void OnPasswordSubmitted(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          form_manager) = 0;

  // Called when the user submits a new password for an existing credential.
  // This stores the provided object and triggers the UI to prompt the user
  // about whether they would like to update the password.
  virtual void OnUpdatePasswordSubmitted(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          form_manager) = 0;

  // Called when the user starts typing in a password field. This switches the
  // icon to a pending state, a user can click on the icon and open a
  // save/update bubble.
  virtual void OnShowManualFallbackForSaving(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager,
      bool has_generated_password,
      bool is_update) = 0;

  // Called when the user cleared the password field. This switches the icon
  // back to manage or inactive state.
  virtual void OnHideManualFallbackForSaving() = 0;

  // Called when the site asks user to choose from credentials. This triggers
  // the UI to prompt the user. |local_credentials| shouldn't be empty. |origin|
  // is a URL of the site that requested a credential.
  // Returns true when the UI is shown. |callback| is called when the user made
  // a decision. If the UI isn't shown the method returns false and doesn't call
  // |callback|.
  virtual bool OnChooseCredentials(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials,
      const GURL& origin,
      const base::Callback<void(const autofill::PasswordForm*)>& callback) = 0;

  // Called when user is auto signed in to the site. |local_forms[0]| contains
  // the credential returned to the site. |origin| is a URL of the site.
  virtual void OnAutoSignin(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin) = 0;

  // Called when it's the right time to enable autosign-in explicitly.
  virtual void OnPromptEnableAutoSignin() = 0;

  // Called when the password will be saved automatically, but we still wish to
  // visually inform the user that the save has occured.
  virtual void OnAutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          form_manager) = 0;

  // Called when a form is autofilled with login information, so we can manage
  // password credentials for the current site which are stored in
  // |password_forms|. This stores a copy of |password_forms| and shows
  // the manage password icon. |federated_matches| contain the matching stored
  // federated credentials to display in the UI.
  virtual void OnPasswordAutofilled(
      const std::vector<const autofill::PasswordForm*>& password_forms,
      const GURL& origin,
      const std::vector<const autofill::PasswordForm*>* federated_matches) = 0;

  // Called when user credentials were leaked. This triggers the UI to prompt
  // the user whether they would like to check their passwords.
  virtual void OnCredentialLeak(password_manager::CredentialLeakType leak_type,
                                const GURL& origin) = 0;

 protected:
  virtual ~PasswordsClientUIDelegate() = default;
};

// Returns ManagePasswordsUIController instance for |contents|
PasswordsClientUIDelegate* PasswordsClientUIDelegateFromWebContents(
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_CLIENT_UI_DELEGATE_H_
