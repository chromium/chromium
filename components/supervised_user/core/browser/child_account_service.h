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
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/list_family_members_service.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "net/base/backoff_entry.h"

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS))
#include "base/feature_list.h"
#include "components/supervised_user/core/common/features.h"
#endif

class PrefService;
class ChildAccountServiceFactory;

namespace supervised_user {
class PermissionRequestCreator;

// This class handles detection of child accounts (on sign-in as well as on
// browser restart), and triggers the appropriate behavior (e.g. enable the
// supervised user experience, fetch information about the parent(s)).
class ChildAccountService : public KeyedService,
                            public signin::IdentityManager::Observer,
                            public SupervisedUserService::Delegate {
 public:
  enum class AuthState { AUTHENTICATED, NOT_AUTHENTICATED, PENDING };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
    registry->RegisterBooleanPref(prefs::kChildAccountStatusKnown, false);
  }

  ChildAccountService(const ChildAccountService&) = delete;
  ChildAccountService& operator=(const ChildAccountService&) = delete;

  ~ChildAccountService() override;

  void Init();

  // Responds whether at least one request for child status was successful.
  // And we got answer whether the account is a child account or not.
  bool IsChildAccountStatusKnown();

  // KeyedService:
  void Shutdown() override;

  void AddChildStatusReceivedCallback(base::OnceClosure callback);

  // Returns whether or not the user is authenticated on Google web properties
  // based on the state of the cookie jar. Returns AuthState::PENDING if
  // authentication state can't be determined at the moment.
  AuthState GetGoogleAuthState();

  // Subscribes to changes to the Google authentication state
  // (see IsGoogleAuthenticated()). Can send a notification even if the
  // authentication state has not changed.
  base::CallbackListSubscription ObserveGoogleAuthState(
      const base::RepeatingCallback<void()>& callback);

  // Use |ChildAccountServiceFactory::GetForProfile(...)| to get an instance of
  // this service.
  ChildAccountService(
      PrefService& user_prefs,
      SupervisedUserService& supervised_user_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::RepeatingCallback<std::unique_ptr<PermissionRequestCreator>()>
          permission_creator_callback,
      base::OnceCallback<void(bool)> check_user_child_status_callback,
      ListFamilyMembersService& list_family_members_service);

 private:
  // SupervisedUserService::Delegate implementation.
  void SetActive(bool active) override;

  // Sets whether the signed-in account is a supervised account.
  void SetSupervisionStatusAndNotifyObservers(
      bool is_subject_to_parental_controls);

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  // IdentityManager::Observer implementation.
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  bool active_{false};

  // Enables or disables scheduled fetch of family members list.
  const raw_ref<ListFamilyMembersService> list_family_members_service_;

  // Subscription to binding between list_family_members_service_ and
  // family_preferences_service_.
  base::CallbackListSubscription set_family_members_subscription_;

  const raw_ptr<signin::IdentityManager> identity_manager_;

  const raw_ref<PrefService> user_prefs_;

  raw_ref<SupervisedUserService> supervised_user_service_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::RepeatingClosureList google_auth_state_observers_;

  // Creates a new instance of a PermissionRequestCreator.
  base::RepeatingCallback<std::unique_ptr<PermissionRequestCreator>()>
      permission_creator_callback_;

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
