// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_CLIENT_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_CLIENT_UI_DELEGATE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/prefs/pref_service.h"

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordFormManagerForUI;
}

namespace url {
class Origin;
}

// An interface for ChromePasswordManagerClient implemented by
// ManagePasswordsUIController. Allows to push a new state for the tab.
class PasswordsClientUIDelegate {
 public:
  // Called when the user submits a form containing login information, so the
  // later requests to save or blocklist can be handled.
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

  // Called by user's explicit action to show the details of a credential, e.g.
  // by choosing the "View details" option for a manual fallback password
  // suggestion.
  virtual void OnOpenPasswordDetailsBubble(
      const password_manager::PasswordForm& form) = 0;

  // Called when the site asks user to choose from credentials. This triggers
  // the UI to prompt the user. |local_credentials| shouldn't be empty. |origin|
  // is a URL of the site that requested a credential.
  // Returns true when the UI is shown. |callback| is called when the user made
  // a decision. If the UI isn't shown the method returns false and doesn't call
  // |callback|.
  virtual bool OnChooseCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_credentials,
      const url::Origin& origin,
      base::OnceCallback<void(const password_manager::PasswordForm*)>
          callback) = 0;

  // Called when user is auto signed in to the site. |local_forms[0]| contains
  // the credential returned to the site. |origin| is a URL of the site.
  virtual void OnAutoSignin(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin) = 0;

  // Called when it's the right time to enable autosign-in explicitly.
  virtual void OnPromptEnableAutoSignin() = 0;

  // Called when the password will be saved automatically, but we still wish to
  // visually inform the user that the save has occured.
  virtual void OnAutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager,
      bool is_update_confirmation) = 0;

  // Called when a form is autofilled with login information, so we can manage
  // password credentials for the current site which are stored in
  // |password_forms|. This stores a copy of |password_forms| and shows
  // the manage password icon. |federated_matches| contain the matching stored
  // federated credentials to display in the UI.
  virtual void OnPasswordAutofilled(
      base::span<const password_manager::PasswordForm> password_forms,
      const url::Origin& origin,
      base::span<const password_manager::PasswordForm> federated_matches) = 0;

  // Called when user credentials were leaked. This triggers the UI to prompt
  // the user whether they would like to check their passwords.
  virtual void OnCredentialLeak(password_manager::CredentialLeakType leak_type,
                                const GURL& url,
                                const std::u16string& username) = 0;

  // Called after a form was submitted. This triggers a bubble that allows to
  // move the just used profile credential in |form| to the user's account.
  virtual void OnShowMoveToAccountBubble(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          form_to_move) = 0;

  // Called when trying to enable biometric authentication for filling from
  // bubble promp.
  virtual void OnBiometricAuthenticationForFilling(
      PrefService* pref_service) = 0;

  // Called when trying to access saved passwords when keychain is not
  // available.
  virtual void OnKeychainError() = 0;

  // Called when a passkey has just been saved to display a confirmation of that
  // to the user. If GPM pin was created in the same flow, then the confirmation
  // of that is also displayed in the title.
  virtual void OnPasskeySaved(bool gpm_pin_created) = 0;

  // Called when a passkey has just been deleted to display a confirmation of
  // that to the user.
  virtual void OnPasskeyDeleted() = 0;

  // Called when a passkey has just been updated to display a confirmation of
  // that to the user.
  virtual void OnPasskeyUpdated() = 0;

  // Called when a passkey has just been deleted because it was not present on
  // an all accepted credentials report.
  virtual void OnPasskeyNotAccepted() = 0;

 protected:
  virtual ~PasswordsClientUIDelegate() = default;
};

// Returns ManagePasswordsUIController instance for |contents|
PasswordsClientUIDelegate* PasswordsClientUIDelegateFromWebContents(
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_CLIENT_UI_DELEGATE_H_
