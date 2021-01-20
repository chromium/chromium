// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_AUTH_HELPER_H_
#define CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_AUTH_HELPER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {
class PasswordFeatureManager;
}

namespace signin {
enum class ReauthResult;
}

namespace signin_metrics {
enum class ReauthAccessPoint;
}

class SigninViewController;
class Profile;

// Responsible for triggering authentication flows related to the passwords
// account storage. Used only by desktop.
class AccountStorageAuthHelper {
 public:
  AccountStorageAuthHelper(
      Profile* profile,
      password_manager::PasswordFeatureManager* password_feature_manager);
  ~AccountStorageAuthHelper();

  AccountStorageAuthHelper(const AccountStorageAuthHelper&) = delete;
  AccountStorageAuthHelper& operator=(const AccountStorageAuthHelper&) = delete;

  // Requests a reauth for the primary account. In case of success, sets the
  // opt in preference for account storage. |reauth_callback| is then called
  // passing whether the reauth succeeded or not.
  // |access_point| represents where the reauth was triggered.
  void TriggerOptInReauth(
      signin_metrics::ReauthAccessPoint access_point,
      base::OnceCallback<
          void(password_manager::PasswordManagerClient::ReauthSucceeded)>
          reauth_callback);

  // Redirects the user to a sign-in in a new tab. |access_point| is used for
  // metrics recording and represents where the sign-in was triggered.
  void TriggerSignIn(signin_metrics::AccessPoint access_point);

  void SetSigninViewControllerGetterForTesting(
      base::RepeatingCallback<SigninViewController*()>
          signin_view_controller_getter) {
    signin_view_controller_getter_ = std::move(signin_view_controller_getter);
  }

 private:
  void OnOptInReauthCompleted(
      base::OnceCallback<
          void(password_manager::PasswordManagerClient::ReauthSucceeded)>
          reauth_callback,
      signin::ReauthResult result);

  Profile* const profile_;

  password_manager::PasswordFeatureManager* const password_feature_manager_;

  base::RepeatingCallback<SigninViewController*()>
      signin_view_controller_getter_;

  // Aborts ongoing reauths if AccountStorageAuthHelper gets destroyed.
  std::unique_ptr<SigninViewController::ReauthAbortHandle> reauth_abort_handle_;

  base::WeakPtrFactory<AccountStorageAuthHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_AUTH_HELPER_H_
