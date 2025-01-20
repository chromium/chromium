// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/fake_user_manager.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"

namespace user_manager {

FakeUserManager::FakeUserManager(PrefService* local_state)
    : UserManagerImpl(std::make_unique<FakeUserManagerDelegate>(),
                      local_state,
                      ash::CrosSettings::IsInitialized()
                          ? ash::CrosSettings::Get()
                          : nullptr) {}

FakeUserManager::~FakeUserManager() = default;

std::string FakeUserManager::GetFakeUsernameHash(const AccountId& account_id) {
  // Consistent with the
  // kUserDataDirNameSuffix in fake_userdataauth_client.cc and
  // UserDataAuthClient::GetStubSanitizedUsername.
  // TODO(crbug.com/1347837): After resolving the dependent code,
  // consolidate the all implementation to cryptohome utilities,
  // and remove this.
  DCHECK(account_id.is_valid());
  return account_id.GetUserEmail() + "-hash";
}

User* FakeUserManager::AddKioskAppUser(const AccountId& account_id) {
  User* user = User::CreateKioskAppUser(account_id);
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

void FakeUserManager::LogoutAllUsers() {
  primary_user_ = nullptr;
  active_user_ = nullptr;

  logged_in_users_.clear();
  lru_logged_in_users_.clear();
}

void FakeUserManager::SetUserNonCryptohomeDataEphemeral(
    const AccountId& account_id,
    bool is_ephemeral) {
  if (is_ephemeral) {
    accounts_with_ephemeral_non_cryptohome_data_.insert(account_id);
  } else {
    accounts_with_ephemeral_non_cryptohome_data_.erase(account_id);
  }
}

void FakeUserManager::SetUserCryptohomeDataEphemeral(
    const AccountId& account_id,
    bool is_ephemeral) {
  accounts_with_ephemeral_cryptohome_data_.insert({account_id, is_ephemeral});
}

void FakeUserManager::UserLoggedIn(const AccountId& account_id,
                                   const std::string& username_hash,
                                   bool browser_restart,
                                   bool is_child) {
  // Please keep the implementation in sync with
  // FakeChromeUserManager::UserLoggedIn. We're in process to merge.
  for (auto& user : user_storage_) {
    if (user->GetAccountId() == account_id) {
      user->set_is_logged_in(true);
      user->set_username_hash(username_hash);
      logged_in_users_.push_back(user.get());
      if (!primary_user_) {
        primary_user_ = user.get();
      }
      if (active_user_) {
        NotifyUserAddedToSession(user.get());
      } else {
        active_user_ = user.get();
      }
      break;
    }
  }

  if (!active_user_ && IsEphemeralAccountId(account_id)) {
    // TODO(crbug.com/278643115): Temporarily duplicate the logic
    // of ephemeral user creation. This method should be unified with
    // UserManagerImpl::UserLoggedIn eventually.
    active_user_ = AddEphemeralUser(account_id, UserType::kRegular);
    SetIsCurrentUserNew(true);
    is_current_user_ephemeral_regular_user_ = true;
  }

  NotifyOnLogin();
}

void FakeUserManager::SwitchActiveUser(const AccountId& account_id) {
  for (UserList::const_iterator it = logged_in_users_.begin();
       it != logged_in_users_.end(); ++it) {
    if ((*it)->GetAccountId() == account_id) {
      active_user_ = (*it).get();
      break;
    }
  }

  if (active_user_ != nullptr) {
    NotifyActiveUserChanged(active_user_);
  }
}

bool FakeUserManager::IsUserNonCryptohomeDataEphemeral(
    const AccountId& account_id) const {
  return base::Contains(accounts_with_ephemeral_non_cryptohome_data_,
                        account_id);
}

bool FakeUserManager::IsUserCryptohomeDataEphemeral(
    const AccountId& account_id) const {
  auto is_ephemeral_overriden =
      base::Contains(accounts_with_ephemeral_cryptohome_data_, account_id);

  if (!is_ephemeral_overriden) {
    // Otherwise fall back to default behavior.
    return UserManagerImpl::IsUserCryptohomeDataEphemeral(account_id);
  }

  return accounts_with_ephemeral_cryptohome_data_.at(account_id);
}

}  // namespace user_manager
