// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_AUTH_HELPER_H_
#define CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_AUTH_HELPER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/signin/public/base/signin_buildflags.h"

namespace password_manager {
class PasswordFeatureManager;
}

namespace signin {
enum class ReauthResult;
class IdentityManager;
}  // namespace signin

namespace signin_metrics {
enum class ReauthAccessPoint;
}

class SigninViewController;
class Profile;

// Responsible for triggering authentication flows related to the passwords
// account storage. Used only by desktop.
class AccountStorageAuthHelper {
 public:
  // |identity_manager| can be null (e.g. in incognito).
  // |password_feature_manager| must be non-null and outlive this object.
  // |signin_view_controller_getter| is passed rather than SigninViewController
  // because the controller is per window, while this helper is per tab. It
  // may return null.
  AccountStorageAuthHelper(
      Profile* profile,
      signin::IdentityManager* identity_manager,
      password_manager::PasswordFeatureManager* password_feature_manager,
      base::RepeatingCallback<SigninViewController*()>
          signin_view_controller_getter);
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

  // Shows a sign-in prompt to the user. |access_point| is used for metrics
  // recording and represents where the sign-in was triggered.
  void TriggerSignIn(signin_metrics::AccessPoint access_point);

 private:
  void OnOptInReauthCompleted(
      base::OnceCallback<
          void(password_manager::PasswordManagerClient::ReauthSucceeded)>
          reauth_callback,
      signin::ReauthResult result);

  const raw_ptr<Profile> profile_;

  const raw_ptr<signin::IdentityManager> identity_manager_;

  const raw_ptr<password_manager::PasswordFeatureManager>
      password_feature_manager_;

  const base::RepeatingCallback<SigninViewController*()>
      signin_view_controller_getter_;

  // Aborts ongoing reauths if AccountStorageAuthHelper gets destroyed.
  std::unique_ptr<SigninViewController::ReauthAbortHandle> reauth_abort_handle_;

  base::WeakPtrFactory<AccountStorageAuthHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_AUTH_HELPER_H_
