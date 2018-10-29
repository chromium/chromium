// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_FAKE_SIGNIN_MANAGER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_FAKE_SIGNIN_MANAGER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"

// SigninManager to use for testing.

class FakeSigninManagerBase : public SigninManagerBase {
 public:
  FakeSigninManagerBase(
      SigninClient* client,
      AccountTrackerService* account_tracker_service,
      SigninErrorController* signin_error_controller = nullptr);
  ~FakeSigninManagerBase() override;

  void SignIn(const std::string& account_id);
};

#if !defined(OS_CHROMEOS)

// A signin manager that bypasses actual authentication routines with servers
// and accepts the credentials provided to StartSignIn.
class FakeSigninManager : public SigninManager {
 public:
  FakeSigninManager(SigninClient* client,
                    ProfileOAuth2TokenService* token_service,
                    AccountTrackerService* account_tracker_service,
                    GaiaCookieManagerService* cookie_manager_service);

  FakeSigninManager(SigninClient* client,
                    ProfileOAuth2TokenService* token_service,
                    AccountTrackerService* account_tracker_service,
                    GaiaCookieManagerService* cookie_manager_service,
                    SigninErrorController* signin_error_controller);

  FakeSigninManager(SigninClient* client,
                    ProfileOAuth2TokenService* token_service,
                    AccountTrackerService* account_tracker_service,
                    GaiaCookieManagerService* cookie_manager_service,
                    SigninErrorController* signin_error_controller,
                    signin::AccountConsistencyMethod account_consistency);

  ~FakeSigninManager() override;

  void set_auth_in_progress(const std::string& account_id) {
    possibly_invalid_account_id_ = account_id;
  }

  void set_password(const std::string& password) { password_ = password; }

  void SignIn(const std::string& gaia_id,
              const std::string& username,
              const std::string& password);

  void ForceSignOut();

  void FailSignin(const GoogleServiceAuthError& error);

  void StartSignInWithRefreshToken(
      const std::string& refresh_token,
      const std::string& gaia_id,
      const std::string& username,
      const std::string& password,
      const OAuthTokenFetchedCallback& oauth_fetched_callback) override;

  void CompletePendingSignin() override;

 protected:
  void OnSignoutDecisionReached(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric,
      RemoveAccountsOption remove_option,
      SigninClient::SignoutDecision signout_decision) override;

  // Username specified in StartSignInWithRefreshToken() call.
  std::string username_;

  ProfileOAuth2TokenService* token_service_;
};

#endif  // !defined (OS_CHROMEOS)

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_FAKE_SIGNIN_MANAGER_H_
