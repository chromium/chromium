// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_HELPER_H_

#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

// Helper class for PasswordManagerClients. It extracts some common logic for
// ChromePasswordManagerClient and IOSChromePasswordManagerClient.
class PasswordManagerClientHelper {
 public:
  explicit PasswordManagerClientHelper(PasswordManagerClient* delegate);
  ~PasswordManagerClientHelper();

  // Implementation of PasswordManagerClient::NotifyUserCouldBeAutoSignedIn.
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<autofill::PasswordForm> form);

  // Implementation of
  // PasswordManagerClient::NotifySuccessfulLoginWithExistingPassword.
  void NotifySuccessfulLoginWithExistingPassword(
      const autofill::PasswordForm& form);

  // Called as a response to
  // PasswordManagerClient::PromptUserToChooseCredentials. nullptr in |form|
  // means that nothing was chosen. |one_local_credential| is true if there was
  // just one local credential to be chosen from.
  void OnCredentialsChosen(
      const PasswordManagerClient::CredentialsCallback& callback,
      bool one_local_credential,
      const autofill::PasswordForm* form);

  // Common logic for IOSChromePasswordManagerClient and
  // ChromePasswordManagerClient implementation of NotifyStorePasswordCalled.
  // Notifies PasswordManager corresponding to the client.
  void NotifyStorePasswordCalled();

  // Common logic for IOSChromePasswordManagerClient and
  // ChromePasswordManagerClient implementation of NotifyUserAutoSignin. After
  // calling this helper method both need to show UI in their own way.
  void NotifyUserAutoSignin();

 private:
  // Returns whether it is necessary to prompt user to enable auto sign-in. This
  // is the case for first run experience, and only for non-incognito mode.
  bool ShouldPromptToEnableAutoSignIn() const;

  PasswordManagerClient* delegate_;

  // Set during 'NotifyUserCouldBeAutoSignedIn' in order to store the
  // form for potential use during 'NotifySuccessfulLoginWithExistingPassword'.
  std::unique_ptr<autofill::PasswordForm> possible_auto_sign_in_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_HELPER_H_
