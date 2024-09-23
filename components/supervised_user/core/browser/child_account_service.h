// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_CHILD_ACCOUNT_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_CHILD_ACCOUNT_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/list_family_members_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "net/base/backoff_entry.h"

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS))
#include "base/feature_list.h"
#include "components/supervised_user/core/common/features.h"
#endif

class PrefService;
class ChildAccountServiceFactory;

namespace supervised_user {

// This class handles detection of child accounts (on sign-in as well as on
// browser restart), and triggers the appropriate behavior (e.g. enable the
// supervised user experience, fetch information about the parent(s)).
class ChildAccountService : public KeyedService,
                            public signin::IdentityManager::Observer {
 public:
  enum class AuthState {
    // The user is signed in to Chrome, and has both a valid refresh token and
    // valid Google account cookies for the primary account.
    AUTHENTICATED,

    // The user is not signed in to Chrome.
    NOT_AUTHENTICATED,

    // The user is signed in to Chrome, but at least one of the refresh token
    // and Google account cookie is not present or invalid.
    PENDING,

    // The user is in a state where, without user input, they may be about to
    // transition between the three states above. Code should may choose to
    // wait for a subsequent update to get the next stable state.
    TRANSIENT_MOVING_TO_AUTHENTICATED,
  };

  ChildAccountService(const ChildAccountService&) = delete;
  ChildAccountService& operator=(const ChildAccountService&) = delete;

  ~ChildAccountService() override;

  void Init();

  // KeyedService:
  void Shutdown() override;

#if BUILDFLAG(IS_CHROMEOS)
  // Responds whether at least one request for child status was successful.
  // And we got answer whether the account is a child account or not.
  bool IsChildAccountStatusKnown();

  void AddChildStatusReceivedCallback(base::OnceClosure callback);
#endif

  // Returns the status of the user's Google authentication credentials (see
  // `AuthState` comments for details).
  AuthState GetGoogleAuthState() const;

  // Subscribes to changes to the Google authentication state (see
  // GetGoogleAuthState()). Can send a notification even if the authentication
  // state has not changed.
  base::CallbackListSubscription ObserveGoogleAuthState(
      const base::RepeatingCallback<void()>& callback);

  // Use |ChildAccountServiceFactory::GetForProfile(...)| to get an instance of
  // this service.
  ChildAccountService(
      PrefService& user_prefs,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::OnceCallback<void(bool)> check_user_child_status_callback,
      ListFamilyMembersService& list_family_members_service);

 private:
  // Sets whether the signed-in account is a supervised account.
  void SetSupervisionStatusAndNotifyObservers(
      bool is_subject_to_parental_controls);

  // Updates whether Google SafeSearch should be forced.
  void UpdateForceGoogleSafeSearch();

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnRefreshTokenUpdatedForAccount(const CoreAccountInfo& account_info) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  void OnAuthStateUpdated();

  // Subscription to set custodian preferences from successful fetch of
  // ListFamilyMembersService.
  base::CallbackListSubscription set_custodian_prefs_subscription_;

  const raw_ptr<signin::IdentityManager> identity_manager_;

  const raw_ref<PrefService> user_prefs_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::RepeatingClosureList google_auth_state_observers_;

  // Callback relevant on Chrome OS platform.
  // Asserts that a supervised user matches the child status of the primary
  // user. Terminates user session in case of status mismatch.
  base::OnceCallback<void(bool)> check_user_child_status_callback_;

  // Callbacks to run when the user status becomes known.
  std::vector<base::OnceClosure> status_received_callback_list_;

  base::WeakPtrFactory<ChildAccountService> weak_ptr_factory_{this};
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_CHILD_ACCOUNT_SERVICE_H_
