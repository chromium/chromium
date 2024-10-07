// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_
#define COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_impl.h"

namespace user_manager {

// Fake user manager with a barebones implementation. Users can be added
// and set as logged in, and those users can be returned.
class USER_MANAGER_EXPORT FakeUserManager : public UserManagerImpl {
 public:
  explicit FakeUserManager(PrefService* local_state = nullptr);

  FakeUserManager(const FakeUserManager&) = delete;
  FakeUserManager& operator=(const FakeUserManager&) = delete;

  ~FakeUserManager() override;

  // Returns the fake username hash for testing.
  // Valid AccountId must be used, otherwise DCHECKed.
  static std::string GetFakeUsernameHash(const AccountId& account_id);

  // Create and add a new user. Created user is not affiliated with the domain,
  // that owns the device.
  const User* AddUser(const AccountId& account_id);
  const User* AddChildUser(const AccountId& account_id);
  const User* AddGuestUser(const AccountId& account_id);
  const User* AddKioskAppUser(const AccountId& account_id);

  // The same as AddUser() but allows to specify user affiliation with the
  // domain, that owns the device.
  const User* AddUserWithAffiliation(const AccountId& account_id,
                                     bool is_affiliated);

  // Create and add a new public account. Created user is not affiliated with
  // the domain, that owns the device.
  virtual const user_manager::User* AddPublicAccountUser(
      const AccountId& account_id);

  void LogoutAllUsers();

  // Subsequent calls to IsCurrentUserNonCryptohomeDataEphemeral for
  // |account_id| will return |is_ephemeral|.
  void SetUserNonCryptohomeDataEphemeral(const AccountId& account_id,
                                         bool is_ephemeral);

  // Subsequent calls to IsCurrentUserCryptohomeDataEphemeral for
  // |account_id| will return |is_ephemeral|.
  void SetUserCryptohomeDataEphemeral(const AccountId& account_id,
                                      bool is_ephemeral);

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
                           const std::u16string& display_name) override;

  // Not implemented.
  void Shutdown() override {}
  const UserList& GetLRULoggedInUsers() const override;
  UserList GetUnlockUsers() const override;
  const AccountId& GetOwnerAccountId() const override;
  void OnSessionStarted() override {}
  bool IsKnownUser(const AccountId& account_id) const override;
  const User* FindUser(const AccountId& account_id) const override;
  User* FindUserAndModify(const AccountId& account_id) override;
  void SaveUserOAuthStatus(const AccountId& account_id,
                           User::OAuthTokenStatus oauth_token_status) override {
  }
  void SaveForceOnlineSignin(const AccountId& account_id,
                             bool force_online_signin) override {}
  void SaveUserDisplayEmail(const AccountId& account_id,
                            const std::string& display_email) override {}
  std::optional<std::string> GetOwnerEmail() override;
  bool IsCurrentUserNonCryptohomeDataEphemeral() const override;
  bool IsUserLoggedIn() const override;
  bool IsLoggedInAsUserWithGaiaAccount() const override;
  bool IsLoggedInAsManagedGuestSession() const override;
  bool IsLoggedInAsGuest() const override;
  bool IsLoggedInAsKioskApp() const override;
  bool IsLoggedInAsWebKioskApp() const override;
  bool IsLoggedInAsAnyKioskApp() const override;
  bool IsLoggedInAsStub() const override;
  bool IsUserNonCryptohomeDataEphemeral(
      const AccountId& account_id) const override;
  bool IsUserCryptohomeDataEphemeral(
      const AccountId& account_id) const override;
  bool IsGuestSessionAllowed() const override;
  bool IsGaiaUserAllowed(const User& user) const override;
  bool IsUserAllowed(const User& user) const override;
  bool IsDeprecatedSupervisedAccountId(
      const AccountId& account_id) const override;

  // UserManagerImpl overrides:
  bool IsDeviceLocalAccountMarkedForRemoval(
      const AccountId& account_id) const override;
  void SetUserAffiliated(const AccountId& account_id,
                         bool is_affiliated) override {}

  // Just make it public for tests.
  using UserManagerImpl::ResetOwnerId;
  using UserManagerImpl::SetEphemeralModeConfig;
  using UserManagerImpl::SetOwnerId;

 protected:
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

  // Contains AccountIds for which IsCurrentUserNonCryptohomeDataEphemeral will
  // return true.
  std::set<AccountId> accounts_with_ephemeral_non_cryptohome_data_;

  // Contains AccountIds for which IsCurrentUserCryptohomeDataEphemeral will
  // return the specific value.
  base::flat_map<AccountId, bool> accounts_with_ephemeral_cryptohome_data_;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_
