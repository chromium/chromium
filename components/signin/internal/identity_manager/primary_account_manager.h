// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The PrimaryAccountManager encapsulates some functionality tracking
// which account is the primary one.
//
// **NOTE** on semantics of PrimaryAccountManager:
//
// Once a signin is successful, the username becomes "established" and will not
// be cleared until a SignOut operation is performed (persists across
// restarts). Until that happens, the primary account manager can still be used
// to refresh credentials, but changing the username is not permitted.
//
// On ChromeOS signout is not possible, so that functionality is if-def'd out on
// that platform.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MANAGER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MANAGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/account_info.h"

class AccountTrackerService;
class PrefRegistrySimple;
class PrefService;
class PrimaryAccountPolicyManager;
class ProfileOAuth2TokenService;

namespace signin_metrics {
enum ProfileSignout : int;
enum class SignoutDelete;
}  // namespace signin_metrics

class PrimaryAccountManager : public ProfileOAuth2TokenServiceObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever a user signs into Google services such as sync.
    // Not called during a reauth.
    virtual void GoogleSigninSucceeded(const CoreAccountInfo& info) {}

    // Called whenever the unconsented primary account changes. This includes
    // the changes for the consented primary account as well.
    virtual void UnconsentedPrimaryAccountChanged(const CoreAccountInfo& info) {
    }

#if !defined(OS_CHROMEOS)
    // Called whenever the currently signed-in user has been signed out.
    virtual void GoogleSignedOut(const CoreAccountInfo& info) {}
#endif
  };

#if !defined(OS_CHROMEOS)
  // Used to remove accounts from the token service and the account tracker.
  enum class RemoveAccountsOption {
    // Do not remove accounts.
    kKeepAllAccounts = 0,
    // Remove all the accounts.
    kRemoveAllAccounts,
    // Removes the authenticated account if it is in authentication error.
    kRemoveAuthenticatedAccountIfInError
  };
#endif

  PrimaryAccountManager(
      SigninClient* client,
      ProfileOAuth2TokenService* token_service,
      AccountTrackerService* account_tracker_service,
      signin::AccountConsistencyMethod account_consistency,
      std::unique_ptr<PrimaryAccountPolicyManager> policy_manager);
  ~PrimaryAccountManager() override;

  // Registers per-profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Registers per-install prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // If user was signed in, load tokens from DB if available.
  void Initialize(PrefService* local_state);
  bool IsInitialized() const;

  // If a user has previously signed in (and has not signed out), this returns
  // the know information of the account. Otherwise, it returns an empty struct.
  CoreAccountInfo GetAuthenticatedAccountInfo() const;

  // If a user has previously signed in (and has not signed out), this returns
  // the account id. Otherwise, it returns an empty CoreAccountId.  This id is
  // the G+/Focus obfuscated gaia id of the user. It can be used to uniquely
  // identify an account, so for example as a key to map accounts to data. For
  // code that needs a unique id to represent the connected account, call this
  // method. Example: the AccountStatusMap type in
  // MutableProfileOAuth2TokenService. For code that needs to know the
  // normalized email address of the connected account, use
  // GetAuthenticatedAccountInfo().email.  Example: to show the string
  // "Signed in as XXX" in the hotdog menu.
  CoreAccountId GetAuthenticatedAccountId() const;

  // Returns true if there is an authenticated user.
  bool IsAuthenticated() const;

  // Signs a user in. PrimaryAccountManager assumes that |username| can be used
  // to look up the corresponding account_id and gaia_id for this email.
  void SignIn(const std::string& username);

  // Updates the authenticated account information from AccountTrackerService.
  void UpdateAuthenticatedAccountInfo();

  // Signout API surfaces (not supported on ChromeOS, where signout is not
  // permitted).
#if !defined(OS_CHROMEOS)
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
#endif

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Provides access to the core information of the user's unconsented primary
  // account. Returns an empty info, if there is no such account.
  CoreAccountInfo GetUnconsentedPrimaryAccountInfo() const;

  // Returns whether the user's unconsented primary account is available.
  bool HasUnconsentedPrimaryAccount() const;

  // Sets the unconsented primary account. The unconsented primary account can
  // only be changed if the user is not authenticated. If the user is
  // authenticated, use Signout() instead.
  void SetUnconsentedPrimaryAccountInfo(CoreAccountInfo account_info);

 private:
  // Sets the authenticated user's account id, when the user has consented to
  // sync.
  // If the user is already authenticated with the same account id, then this
  // method is a no-op.
  // It is forbidden to call this method if the user is already authenticated
  // with a different account (this method will DCHECK in that case).
  // |account_id| must not be empty. To log the user out, use
  // ClearAuthenticatedAccountId() instead.
  void SetAuthenticatedAccountInfo(const CoreAccountInfo& account_info);

  // Sets |primary_account_info_| and updates the associated preferences.
  void SetPrimaryAccountInternal(const CoreAccountInfo& account_info,
                                 bool consented_to_sync);

#if !defined(OS_CHROMEOS)
  // Starts the sign out process.
  void StartSignOut(signin_metrics::ProfileSignout signout_source_metric,
                    signin_metrics::SignoutDelete signout_delete_metric,
                    RemoveAccountsOption remove_option);

  // The sign out process which is started by SigninClient::PreSignOut()
  void OnSignoutDecisionReached(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric,
      RemoveAccountsOption remove_option,
      SigninClient::SignoutDecision signout_decision);

  // ProfileOAuth2TokenServiceObserver:
  void OnRefreshTokensLoaded() override;
#endif

  const CoreAccountInfo& primary_account_info() const {
    return primary_account_info_;
  }

  SigninClient* client_;

  // The ProfileOAuth2TokenService instance associated with this object. Must
  // outlive this object.
  ProfileOAuth2TokenService* token_service_ = nullptr;
  AccountTrackerService* account_tracker_service_ = nullptr;

  bool initialized_ = false;

  // Account id after successful authentication. The account may or may not be
  // consented to Sync.
  // Must be kept in sync with prefs. Use SetPrimaryAccountInternal() to change
  // this field.
  CoreAccountInfo primary_account_info_;

#if !defined(OS_CHROMEOS)
  signin::AccountConsistencyMethod account_consistency_;
#endif

  std::unique_ptr<PrimaryAccountPolicyManager> policy_manager_;
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(PrimaryAccountManager);
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MANAGER_H_
