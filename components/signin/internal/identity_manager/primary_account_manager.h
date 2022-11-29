// Copyright 2014 The Chromium Authors
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

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"

class AccountTrackerService;
class PrefRegistrySimple;
class PrefService;
class ProfileOAuth2TokenService;

namespace signin_metrics {
enum ProfileSignout : int;
enum class SignoutDelete;
}  // namespace signin_metrics

class PrimaryAccountManager : public ProfileOAuth2TokenServiceObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when there is a change in the primary account or in the consent
    // level for the primary account.
    virtual void OnPrimaryAccountChanged(
        const signin::PrimaryAccountChangeEvent& event_details) = 0;
  };

  // Used to remove accounts from the token service and the account tracker.
  enum class RemoveAccountsOption {
    // Do not remove accounts.
    kKeepAllAccounts = 0,
    // Remove all the accounts.
    kRemoveAllAccounts,
  };

  PrimaryAccountManager(SigninClient* client,
                        ProfileOAuth2TokenService* token_service,
                        AccountTrackerService* account_tracker_service);

  PrimaryAccountManager(const PrimaryAccountManager&) = delete;
  PrimaryAccountManager& operator=(const PrimaryAccountManager&) = delete;

  ~PrimaryAccountManager() override;

  // Registers per-profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Registers per-install prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // If user was signed in, load tokens from DB if available.
  void Initialize(PrefService* local_state);
  bool IsInitialized() const;

  // Returns whether the user's primary account is available. If consent is
  // |ConsentLevel::kSync| then true implies that the user has blessed this
  // account for sync.
  bool HasPrimaryAccount(signin::ConsentLevel consent_level) const;

  // Provides access to the core information of the user's primary account.
  // The primary account may or may not be blessed with the sync consent.
  // Returns an empty struct if no such info is available, either because there
  // is no primary account yet or because the user signed out or the |consent|
  // level required |ConsentLevel::kSync| was not granted.
  // Returns a non-empty struct if the primary account exists and was granted
  // the required consent level.
  CoreAccountInfo GetPrimaryAccountInfo(
      signin::ConsentLevel consent_level) const;

  // Provides access to the account ID of the user's primary account. Simple
  // convenience wrapper over GetPrimaryAccountInfo().account_id.
  CoreAccountId GetPrimaryAccountId(signin::ConsentLevel consent_level) const;

  // Sets the primary account with the required consent level. The primary
  // account can only be changed if the user has not consented for sync. If the
  // user has consented for sync already, then use ClearPrimaryAccount() or
  // RevokeSync() instead.
  void SetPrimaryAccountInfo(const CoreAccountInfo& account_info,
                             signin::ConsentLevel consent_level);

  // Updates the primary account information from AccountTrackerService.
  void UpdatePrimaryAccountInfo();

  // Signout API surfaces (not supported on ChromeOS, where signout is not
  // permitted).
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Clears the primary account, erasing all keys associated with the primary
  // account (also cancels all auth in progress).
  // It removes all accounts from the identity manager by revoking all refresh
  // tokens.
  void ClearPrimaryAccount(signin_metrics::ProfileSignout signout_source_metric,
                           signin_metrics::SignoutDelete signout_delete_metric);

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // Rovokes the sync consent but leaves the primary account and the rest of
  // the accounts untouched.
  void RevokeSyncConsent(signin_metrics::ProfileSignout signout_source_metric,
                         signin_metrics::SignoutDelete signout_delete_metric);

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  class ScopedPrefCommit;

  // Sets the primary account id, when the user has consented to sync.
  // If the user has consented for sync with the same account, then this method
  // is a no-op.
  // It is forbidden to call this method if the user has already consented for
  // sync  with a different account (this method will DCHECK in that case).
  // |account_id| must not be empty.
  void SetSyncPrimaryAccountInternal(const CoreAccountInfo& account_info);

  // Sets |primary_account_info_| and updates the associated preferences.
  void SetPrimaryAccountInternal(const CoreAccountInfo& account_info,
                                 bool consented_to_sync,
                                 ScopedPrefCommit& scoped_pref_commit);

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

  // Returns the current state of the primary account.
  signin::PrimaryAccountChangeEvent::State GetPrimaryAccountState() const;

  // Fires OnPrimaryAccountChanged() notifications on all observers.
  void FirePrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent::State& previous_state);

  // ProfileOAuth2TokenServiceObserver:
  void OnRefreshTokensLoaded() override;

  const CoreAccountInfo& primary_account_info() const {
    return primary_account_info_;
  }

  raw_ptr<SigninClient> client_;

  // The ProfileOAuth2TokenService instance associated with this object. Must
  // outlive this object.
  raw_ptr<ProfileOAuth2TokenService> token_service_ = nullptr;
  raw_ptr<AccountTrackerService> account_tracker_service_ = nullptr;

  bool initialized_ = false;

  // The primary account information. The account may or may not be consented
  // for Sync.
  // Must be kept in sync with prefs. Use SetPrimaryAccountInternal() to change
  // this field.
  CoreAccountInfo primary_account_info_;

  base::ObserverList<Observer> observers_;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MANAGER_H_
