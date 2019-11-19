// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_
#define COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_base.h"

namespace user_manager {

// Fake user manager with a barebones implementation. Users can be added
// and set as logged in, and those users can be returned.
class USER_MANAGER_EXPORT FakeUserManager : public UserManagerBase {
 public:
  FakeUserManager();
  ~FakeUserManager() override;

  // Create and add a new user. Created user is not affiliated with the domain,
  // that owns the device.
  const User* AddUser(const AccountId& account_id);
  const User* AddChildUser(const AccountId& account_id);
  const User* AddGuestUser(const AccountId& account_id);

  // The same as AddUser() but allows to specify user affiliation with the
  // domain, that owns the device.
  const User* AddUserWithAffiliation(const AccountId& account_id,
                                     bool is_affiliated);

  // Create and add a new public account. Created user is not affiliated with
  // the domain, that owns the device.
  virtual const user_manager::User* AddPublicAccountUser(
      const AccountId& account_id);

  void set_local_state(PrefService* local_state) { local_state_ = local_state; }

  // UserManager overrides.
  const UserList& GetUsers() const override;
  UserList GetUsersAllowedForMultiProfile() const override;
  void UpdateUserAccountData(const AccountId& account_id,
                             const UserAccountData& account_data) override;

  // Set the user as logged in.
  void UserLoggedIn(const AccountId& account_id,
                    const std::string& username_hash,
                    bool browser_restart,
                    bool is_child) override;

  const User* GetActiveUser() const override;
  User* GetActiveUser() override;
  void SwitchActiveUser(const AccountId& account_id) override;
  void SaveUserDisplayName(const AccountId& account_id,
                           const base::string16& display_name) override;
  const AccountId& GetGuestAccountId() const override;
  bool IsFirstExecAfterBoot() const override;
  bool IsGuestAccountId(const AccountId& account_id) const override;
  bool IsStubAccountId(const AccountId& account_id) const override;
  bool HasBrowserRestarted() const override;

  // Not implemented.
  void Shutdown() override {}
  const UserList& GetLRULoggedInUsers() const override;
  UserList GetUnlockUsers() const override;
  const AccountId& GetOwnerAccountId() const override;
  void OnSessionStarted() override {}
  void RemoveUser(const AccountId& account_id,
                  RemoveUserDelegate* delegate) override {}
  void RemoveUserFromList(const AccountId& account_id) override;
  bool IsKnownUser(const AccountId& account_id) const override;
  const User* FindUser(const AccountId& account_id) const override;
  User* FindUserAndModify(const AccountId& account_id) override;
  const User* GetPrimaryUser() const override;
  void SaveUserOAuthStatus(const AccountId& account_id,
                           User::OAuthTokenStatus oauth_token_status) override {
  }
  void SaveForceOnlineSignin(const AccountId& account_id,
                             bool force_online_signin) override {}
  base::string16 GetUserDisplayName(const AccountId& account_id) const override;
  void SaveUserDisplayEmail(const AccountId& account_id,
                            const std::string& display_email) override {}
  bool IsCurrentUserOwner() const override;
  bool IsCurrentUserNew() const override;
  bool IsCurrentUserNonCryptohomeDataEphemeral() const override;
  bool CanCurrentUserLock() const override;
  bool IsUserLoggedIn() const override;
  bool IsLoggedInAsUserWithGaiaAccount() const override;
  bool IsLoggedInAsPublicAccount() const override;
  bool IsLoggedInAsGuest() const override;
  bool IsLoggedInAsSupervisedUser() const override;
  bool IsLoggedInAsKioskApp() const override;
  bool IsLoggedInAsArcKioskApp() const override;
  bool IsLoggedInAsWebKioskApp() const override;
  bool IsLoggedInAsAnyKioskApp() const override;
  bool IsLoggedInAsStub() const override;
  bool IsUserNonCryptohomeDataEphemeral(
      const AccountId& account_id) const override;
  void AddObserver(Observer* obs) override {}
  void RemoveObserver(Observer* obs) override {}
  void AddSessionStateObserver(UserSessionStateObserver* obs) override {}
  void RemoveSessionStateObserver(UserSessionStateObserver* obs) override {}
  void NotifyLocalStateChanged() override {}
  bool AreSupervisedUsersAllowed() const override;
  bool IsGuestSessionAllowed() const override;
  bool IsGaiaUserAllowed(const User& user) const override;
  bool IsUserAllowed(const User& user) const override;
  void UpdateLoginState(const User* active_user,
                        const User* primary_user,
                        bool is_current_user_owner) const override;
  bool GetPlatformKnownUserId(const std::string& user_email,
                              const std::string& gaia_id,
                              AccountId* out_account_id) const override;
  void AsyncRemoveCryptohome(const AccountId& account_id) const override;
  bool IsSupervisedAccountId(const AccountId& account_id) const override;
  const gfx::ImageSkia& GetResourceImagekiaNamed(int id) const override;
  base::string16 GetResourceStringUTF16(int string_id) const override;
  void ScheduleResolveLocale(const std::string& locale,
                             base::OnceClosure on_resolved_callback,
                             std::string* out_resolved_locale) const override;
  bool IsValidDefaultUserImageId(int image_index) const override;

  // UserManagerBase overrides:
  bool AreEphemeralUsersEnabled() const override;
  void SetEphemeralUsersEnabled(bool enabled) override;
  const std::string& GetApplicationLocale() const override;
  PrefService* GetLocalState() const override;
  void HandleUserOAuthTokenStatusChange(
      const AccountId& account_id,
      User::OAuthTokenStatus status) const override {}
  bool IsEnterpriseManaged() const override;
  void LoadDeviceLocalAccounts(
      std::set<AccountId>* device_local_accounts_set) override {}
  void PerformPreUserListLoadingActions() override {}
  void PerformPostUserListLoadingActions() override {}
  void PerformPostUserLoggedInActions(bool browser_restart) override {}
  bool IsDemoApp(const AccountId& account_id) const override;
  bool IsDeviceLocalAccountMarkedForRemoval(
      const AccountId& account_id) const override;
  void DemoAccountLoggedIn() override {}
  void KioskAppLoggedIn(User* user) override {}
  void ArcKioskAppLoggedIn(User* user) override {}
  void WebKioskAppLoggedIn(User* user) override {}
  void PublicAccountUserLoggedIn(User* user) override {}
  void SupervisedUserLoggedIn(const AccountId& account_id) override {}
  void OnUserRemoved(const AccountId& account_id) override {}

 protected:
  User* primary_user_;

  // Can be set by set_local_state().
  PrefService* local_state_ = nullptr;

  // If set this is the active user. If empty, the first created user is the
  // active user.
  AccountId active_account_id_ = EmptyAccountId();

 private:
  // We use this internal function for const-correctness.
  User* GetActiveUserInternal() const;

  // stub, always empty.
  AccountId owner_account_id_ = EmptyAccountId();

  // stub. Always empty.
  gfx::ImageSkia empty_image_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserManager);
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_
