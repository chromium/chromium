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

  // Creates and adds a new Kiosk user.
  User* AddKioskAppUser(const AccountId& account_id);

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
  void UserLoggedIn(const AccountId& account_id,
                    const std::string& username_hash,
                    bool browser_restart,
                    bool is_child) override;
  void SwitchActiveUser(const AccountId& account_id) override;
  bool IsUserNonCryptohomeDataEphemeral(
      const AccountId& account_id) const override;
  bool IsUserCryptohomeDataEphemeral(
      const AccountId& account_id) const override;

  // Just make it public for tests.
  using UserManagerImpl::AddEphemeralUser;
  using UserManagerImpl::AddGaiaUser;
  using UserManagerImpl::AddGuestUser;
  using UserManagerImpl::AddPublicAccountUser;
  using UserManagerImpl::ResetOwnerId;
  using UserManagerImpl::SetEphemeralModeConfig;
  using UserManagerImpl::SetOwnerId;

 private:
  // Contains AccountIds for which IsCurrentUserNonCryptohomeDataEphemeral will
  // return true.
  std::set<AccountId> accounts_with_ephemeral_non_cryptohome_data_;

  // Contains AccountIds for which IsCurrentUserCryptohomeDataEphemeral will
  // return the specific value.
  base::flat_map<AccountId, bool> accounts_with_ephemeral_cryptohome_data_;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_
