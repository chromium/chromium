// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_ASH_LOGIN_SIGNIN_UI_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_SIGNIN_UI_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/encryption_migration_mode.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/login/base_screen_handler_utils.h"
#include "components/prefs/pref_service.h"

namespace ash {

enum class SigninError {
  kCaptivePortalError,
  kGoogleAccountNotAllowed,
  kOwnerRequired,
  kTpmUpdateRequired,
  kKnownUserFailedNetworkNotConnected,
  kNewUserFailedNetworkNotConnected,
  kNewUserFailedNetworkConnected,
  kKnownUserFailedNetworkConnected,
  kOwnerKeyLost,
  kChallengeResponseAuthMultipleClientCerts,
  kChallengeResponseAuthInvalidClientCert,
  kCookieWaitTimeout,
  kFailedToFetchSamlRedirect,
};

// This class represents an interface between code that performs sign-in
// operations and code that handles sign-in UI. It is used to encapsulate UI
// implementation details and declare the required set of parameters that need
// to be set for particular UI.
class SigninUI {
 public:
  SigninUI() = default;
  virtual ~SigninUI() = default;
  SigninUI(const SigninUI&) = delete;
  SigninUI& operator=(const SigninUI&) = delete;

  // Starts user onboarding after successful sign-in for new users.
  virtual void StartUserOnboarding() = 0;
  // Resumes user onboarding after successful sign-in for returning users.
  virtual void ResumeUserOnboarding(const PrefService& prefs,
                                    OobeScreenId screen_id) = 0;
  // Show UI for management transition flow.
  virtual void StartManagementTransition() = 0;
  // Show additional terms of service on login.
  virtual void ShowTosForExistingUser() = 0;
  // After users update from CloudReady to a new OS version show them new
  // license agreement and data collection consent.
  virtual void ShowNewTermsForFlexUsers() = 0;

  virtual void StartEncryptionMigration(
      std::unique_ptr<UserContext> user_context,
      EncryptionMigrationMode migration_mode,
      base::OnceCallback<void(std::unique_ptr<UserContext>)>
          skip_migration_callback) = 0;

  // Might store authentication data so that additional auth factors can be
  // added during user onboarding.
  virtual void SetAuthSessionForOnboarding(const UserContext& user_context) = 0;

  // Clears authentication data that were stored for user onboarding.
  virtual void ClearOnboardingAuthSession() = 0;

  // Start authentication flow that would use factors beyond
  // online authentication factor (Recovery, old online password,
  // fallback to local password/PIN, etc).
  virtual void UseAlternativeAuthentication(
      std::unique_ptr<UserContext> user_context,
      bool online_password_mismatch) = 0;

  // Runs an extra step of local authentication.
  virtual void RunLocalAuthentication(
      std::unique_ptr<UserContext> user_context) = 0;

  virtual void ShowSigninError(SigninError error,
                               const std::string& details) = 0;

  // Show the SAML Confirm Password screen and continue authentication after
  // that (or show the error screen).
  virtual void SAMLConfirmPassword(
      ::login::StringList scraped_passwords,
      std::unique_ptr<UserContext> user_context) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_SIGNIN_UI_H_
