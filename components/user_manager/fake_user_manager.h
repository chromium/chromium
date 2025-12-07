// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_
#define COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/user_manager/user_manager_impl.h"

class AccountId;
class PrefService;

namespace user_manager {

// DEPRECATED: please use UserManagerImpl with TestHelper.
// Fake user manager with a barebones implementation. Users can be added
// and set as logged in, and those users can be returned.
class USER_MANAGER_EXPORT FakeUserManager : public UserManagerImpl {
 public:
  explicit FakeUserManager(PrefService* local_state = nullptr);

  FakeUserManager(const FakeUserManager&) = delete;
  FakeUserManager& operator=(const FakeUserManager&) = delete;

  ~FakeUserManager() override;

  // DEPRECATED: please use TestHelper::GetFakeUsernameHash.
  static std::string GetFakeUsernameHash(const AccountId& account_id);

  // UserManager overrides.
  void UserLoggedIn(const AccountId& account_id,
                    const std::string& username_hash) override;
  bool EnsureUser(const AccountId& account_id,
                  UserType user_type,
                  bool is_ephemeral) override;
  void SwitchActiveUser(const AccountId& account_id) override;

  // Just make it public for tests.
  using UserManagerImpl::AddEphemeralUser;
  using UserManagerImpl::AddGaiaUser;
  using UserManagerImpl::AddGuestUser;
  using UserManagerImpl::AddPublicAccountUser;
  using UserManagerImpl::ResetOwnerId;
  using UserManagerImpl::SetEphemeralModeConfig;
  using UserManagerImpl::SetOwnerId;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_
