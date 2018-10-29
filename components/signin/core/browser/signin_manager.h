// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The signin manager encapsulates some functionality tracking
// which user is signed in. See SigninManagerBase for full description of
// responsibilities. The class defined in this file provides functionality
// required by all platforms except Chrome OS.
//
// When a user is signed in, a ClientLogin request is run on their behalf.
// Auth tokens are fetched from Google and the results are stored in the
// TokenService.
// TODO(tim): Bug 92948, 226464. ClientLogin is all but gone from use.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_H_

#include "build/build_config.h"

#if defined(OS_CHROMEOS)

#include "components/signin/core/browser/signin_manager_base.h"

#else

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "net/cookies/canonical_cookie.h"

class GaiaCookieManagerService;
class GoogleServiceAuthError;
class PrefService;
class ProfileOAuth2TokenService;
class SigninClient;
class SigninErrorController;

namespace identity {
class IdentityManager;
}

class SigninManager : public SigninManagerBase,
                      public AccountTrackerService::Observer,
                      public OAuth2TokenService::Observer {
 public:
  // The callback invoked once the OAuth token has been fetched during signin,
  // but before the profile transitions to the "signed-in" state. This allows
  // callers to load policy and prompt the user appropriately before completing
  // signin. The callback is passed the just-fetched OAuth login refresh token.
  typedef base::Callback<void(const std::string&)> OAuthTokenFetchedCallback;

  // Used to remove accounts from the token service and the account tracker.
  enum class RemoveAccountsOption {
    // Do not remove accounts.
    kKeepAllAccounts,
    // Remove all the accounts.
    kRemoveAllAccounts,
    // Removes the authenticated account if it is in authentication error.
    kRemoveAuthenticatedAccountIfInError
  };

  // This is used to distinguish URLs belonging to the special web signin flow
  // running in the special signin process from other URLs on the same domain.
  // We do not grant WebUI privilieges / bindings to this process or to URLs of
  // this scheme; enforcement of privileges is handled separately by
  // OneClickSigninHelper.
  static const char kChromeSigninEffectiveSite[];

  SigninManager(SigninClient* client,
                ProfileOAuth2TokenService* token_service,
                AccountTrackerService* account_tracker_service,
                GaiaCookieManagerService* cookie_manager_service,
                SigninErrorController* signin_error_controller,
                signin::AccountConsistencyMethod account_consistency);
  ~SigninManager() override;

  // Returns true if the username is allowed based on the policy string.
  static bool IsUsernameAllowedByPolicy(const std::string& username,
                                        const std::string& policy);

  // Returns |manager| as a SigninManager instance. Relies on the fact that on
  // platforms where signin_manager.* is built, all SigninManagerBase instances
  // are actually SigninManager instances.
  static SigninManager* FromSigninManagerBase(SigninManagerBase* manager);

  // Attempt to sign in this user with a refresh token.
  // If |refresh_token| is not empty, then SigninManager will add it to the
  // |token_service_| when the sign-in flow is completed.
  // If non-null, the passed |oauth_fetched_callback| callback is invoked once
  // sign-in has been completed.
  // The callback should invoke SignOut() or CompletePendingSignin() to either
  // continue or cancel the in-process signin.
  virtual void StartSignInWithRefreshToken(
      const std::string& refresh_token,
      const std::string& gaia_id,
      const std::string& username,
      const std::string& password,
      const OAuthTokenFetchedCallback& oauth_fetched_callback);

  // Copies auth credentials from one SigninManager to this one. This is used
  // when creating a new profile during the signin process to transfer the
  // in-progress credentials to the new profile.
  virtual void CopyCredentialsFrom(const SigninManager& source);

  // Signs a user out, removing the preference, erasing all keys
  // associated with the authenticated user, and canceling all auth in progress.
  // On mobile and on desktop pre-DICE, this also removes all accounts from
  // Chrome by revoking all refresh tokens.
  // On desktop with DICE enabled, this will remove the authenticated account
  // from Chrome only if it is in authentication error. No other accounts are
  // removed.
  void SignOut(signin_metrics::ProfileSignout signout_source_metric,
               signin_metrics::SignoutDelete signout_delete_metric);

  // Signs a user out, removing the preference, erasing all keys
  // associated with the authenticated user, and canceling all auth in progress.
  // It removes all accounts from Chrome by revoking all refresh tokens.
  void SignOutAndRemoveAllAccounts(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric);

  // Signs a user out, removing the preference, erasing all keys
  // associated with the authenticated user, and canceling all auth in progress.
  // Does not remove the accounts from the token service.
  void SignOutAndKeepAllAccounts(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric);

  // On platforms where SigninManager is responsible for dealing with
  // invalid username policy updates, we need to check this during
  // initialization and sign the user out.
  void Initialize(PrefService* local_state) override;
  void Shutdown() override;

  // If applicable, merge the signed in account into the cookie jar.
  void MergeSigninCredentialIntoCookieJar();

  // Invoked from an OAuthTokenFetchedCallback to complete user signin.
  virtual void CompletePendingSignin();

  // Invoked from SigninManagerAndroid to indicate that the sign-in process
  // has completed for the email |username|.  SigninManager assumes that
  // |username| can be used to look up the corresponding account_id and gaia_id
  // for this email.
  void OnExternalSigninCompleted(const std::string& username);

  // Returns true if there's a signin in progress.
  bool AuthInProgress() const override;

  bool IsSigninAllowed() const override;

  // Returns true if the passed username is allowed by policy. Virtual for
  // mocking in tests.
  virtual bool IsAllowedUsername(const std::string& username) const;

  // If an authentication is in progress, return the account id being
  // authenticated. Returns an empty string if no auth is in progress.
  const std::string& GetAccountIdForAuthInProgress() const;

  // If an authentication is in progress, return the gaia id being
  // authenticated. Returns an empty string if no auth is in progress.
  const std::string& GetGaiaIdForAuthInProgress() const;

  // If an authentication is in progress, return the username being
  // authenticated. Returns an empty string if no auth is in progress.
  const std::string& GetUsernameForAuthInProgress() const;

 protected:
  // The sign out process which is started by SigninClient::PreSignOut()
  virtual void OnSignoutDecisionReached(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric,
      RemoveAccountsOption remove_option,
      SigninClient::SignoutDecision signout_decision);

 private:
  enum SigninType {
    SIGNIN_TYPE_NONE,
    SIGNIN_TYPE_WITH_REFRESH_TOKEN,
    SIGNIN_TYPE_WITHOUT_REFRESH_TOKEN
  };

  std::string SigninTypeToString(SigninType type);
  friend class FakeSigninManager;
  friend class identity::IdentityManager;
  FRIEND_TEST_ALL_PREFIXES(SigninManagerTest, ClearTransientSigninData);
  FRIEND_TEST_ALL_PREFIXES(SigninManagerTest, ProvideSecondFactorSuccess);
  FRIEND_TEST_ALL_PREFIXES(SigninManagerTest, ProvideSecondFactorFailure);

  // Called to setup the transient signin data during one of the
  // StartSigninXXX methods.  |type| indicates which of the methods is being
  // used to perform the signin while |username| and |password| identify the
  // account to be signed in. Returns false and generates an auth error if the
  // passed |username| is not allowed by policy.  |gaia_id| is the obfuscated
  // gaia id corresponding to |username|.
  bool PrepareForSignin(SigninType type,
                        const std::string& gaia_id,
                        const std::string& username,
                        const std::string& password);

  // Persists |account_id| as the currently signed-in account, and triggers
  // a sign-in success notification.
  void OnSignedIn();

  // Send all observers |GoogleSigninSucceeded| notifications.
  void FireGoogleSigninSucceeded();

  // Send all observers |GoogleSignedOut| notifications.
  void FireGoogleSignedOut(const std::string& account_id,
                           const AccountInfo& account_info);

  // Waits for the AccountTrackerService, then sends GoogleSigninSucceeded to
  // the client and clears the local password.
  void PostSignedIn();

  // AccountTrackerService::Observer:
  void OnAccountUpdated(const AccountInfo& info) override;
  void OnAccountUpdateFailed(const std::string& account_id) override;

  // OAuth2TokenService::Observer:
  void OnRefreshTokensLoaded() override;

  // Called when a new request to re-authenticate a user is in progress.
  // Will clear in memory data but leaves the db as such so when the browser
  // restarts we can use the old token(which might throw a password error).
  void ClearTransientSigninData();

  // Called to handle an error from a GAIA auth fetch.  Sets the last error
  // to |error|, sends out a notification of login failure and clears the
  // transient signin data.
  void HandleAuthError(const GoogleServiceAuthError& error);

  // Starts the sign out process.
  void StartSignOut(signin_metrics::ProfileSignout signout_source_metric,
                    signin_metrics::SignoutDelete signout_delete_metric,
                    RemoveAccountsOption remove_option);

  void OnSigninAllowedPrefChanged();
  void OnGoogleServicesUsernamePatternChanged();

  std::string possibly_invalid_account_id_;
  std::string possibly_invalid_gaia_id_;
  std::string possibly_invalid_email_;
  std::string password_;  // This is kept empty whenever possible.

  // The type of sign being performed.  This value is valid only between a call
  // to one of the StartSigninXXX methods and when the sign in is either
  // successful or not.
  SigninType type_;

  // Temporarily saves the oauth2 refresh token.  It will be passed to the
  // token service so that it does not need to mint new ones.
  std::string temp_refresh_token_;

  // The SigninClient object associated with this object. Must outlive this
  // object.
  SigninClient* client_;

  // The ProfileOAuth2TokenService instance associated with this object. Must
  // outlive this object.
  ProfileOAuth2TokenService* token_service_;

  // Object used to use the token to push a GAIA cookie into the cookie jar.
  GaiaCookieManagerService* cookie_manager_service_;

  // Helper object to listen for changes to signin preferences stored in non-
  // profile-specific local prefs (like kGoogleServicesUsernamePattern).
  PrefChangeRegistrar local_state_pref_registrar_;

  // Helper object to listen for changes to the signin allowed preference.
  BooleanPrefMember signin_allowed_;

  signin::AccountConsistencyMethod account_consistency_;

  // Two gate conditions for when PostSignedIn should be called. Verify
  // that the SigninManager has reached OnSignedIn() and the AccountTracker
  // has completed calling GetUserInfo.
  bool signin_manager_signed_in_;
  bool user_info_fetched_by_account_tracker_;

  base::WeakPtrFactory<SigninManager> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManager);
};

#endif  // !defined(OS_CHROMEOS)

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_H_
