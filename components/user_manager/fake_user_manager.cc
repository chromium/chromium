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

const User* FakeUserManager::AddUser(const AccountId& account_id) {
  return AddUserWithAffiliation(account_id, false);
}

const User* FakeUserManager::AddChildUser(const AccountId& account_id) {
  User* user = User::CreateRegularUser(account_id, UserType::kChild);
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

const User* FakeUserManager::AddGuestUser(const AccountId& account_id) {
  User* user = User::CreateGuestUser(account_id);
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

const User* FakeUserManager::AddKioskAppUser(const AccountId& account_id) {
  User* user = User::CreateKioskAppUser(account_id);
  user->set_username_hash(GetFakeUsernameHash(account_id));
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

const User* FakeUserManager::AddUserWithAffiliation(const AccountId& account_id,
                                                    bool is_affiliated) {
  User* user = User::CreateRegularUser(account_id, UserType::kRegular);
  user->SetAffiliated(is_affiliated);
  user->set_username_hash(GetFakeUsernameHash(account_id));
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

const user_manager::User* FakeUserManager::AddPublicAccountUser(
    const AccountId& account_id) {
  User* user = User::CreatePublicAccountUserForTesting(account_id);
  user_storage_.emplace_back(user);
  users_.push_back(user);
  return user;
}

const UserList& FakeUserManager::GetUsers() const {
  return users_;
}

UserList FakeUserManager::GetUsersAllowedForMultiProfile() const {
  UserList result;
  for (UserList::const_iterator it = users_.begin(); it != users_.end(); ++it) {
    if ((*it)->GetType() == UserType::kRegular && !(*it)->is_logged_in()) {
      result.push_back(*it);
    }
  }
  return result;
}

void FakeUserManager::UpdateUserAccountData(
    const AccountId& account_id,
    const UserAccountData& account_data) {
  for (User* user : users_) {
    if (user->GetAccountId() == account_id) {
      user->set_display_name(account_data.display_name());
      user->set_given_name(account_data.given_name());
      return;
    }
  }
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
  for (user_manager::User* user : users_) {
    if (user->GetAccountId() == account_id) {
      user->set_is_logged_in(true);
      user->set_username_hash(username_hash);
      logged_in_users_.push_back(user);
      if (!primary_user_) {
        primary_user_ = user;
      }
      if (active_user_) {
        NotifyUserAddedToSession(user);
      } else {
        active_user_ = user;
      }
      break;
    }
  }

  if (!active_user_ && IsEphemeralAccountId(account_id)) {
    RegularUserLoggedInAsEphemeral(account_id, UserType::kRegular);
  }

  NotifyOnLogin();
}

User* FakeUserManager::GetActiveUserInternal() const {
  if (active_user_ != nullptr)
    return active_user_;

  if (!users_.empty()) {
    if (active_account_id_.is_valid()) {
      for (UserList::const_iterator it = users_.begin(); it != users_.end();
           ++it) {
        if ((*it)->GetAccountId() == active_account_id_)
          return *it;
      }
    }
    return users_[0];
  }
  return nullptr;
}

const User* FakeUserManager::GetActiveUser() const {
  return GetActiveUserInternal();
}

User* FakeUserManager::GetActiveUser() {
  return GetActiveUserInternal();
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

void FakeUserManager::SaveUserDisplayName(const AccountId& account_id,
                                          const std::u16string& display_name) {
  for (UserList::iterator it = users_.begin(); it != users_.end(); ++it) {
    if ((*it)->GetAccountId() == account_id) {
      (*it)->set_display_name(display_name);
      return;
    }
  }
}

const UserList& FakeUserManager::GetLRULoggedInUsers() const {
  return users_;
}

UserList FakeUserManager::GetUnlockUsers() const {
  return users_;
}

const AccountId& FakeUserManager::GetOwnerAccountId() const {
  return owner_account_id_;
}

bool FakeUserManager::IsKnownUser(const AccountId& account_id) const {
  return true;
}

const User* FakeUserManager::FindUser(const AccountId& account_id) const {
  if (active_user_ != nullptr && active_user_->GetAccountId() == account_id)
    return active_user_;

  for (const User* user : users_) {
    if (user->GetAccountId() == account_id) {
      return user;
    }
  }

  return nullptr;
}

User* FakeUserManager::FindUserAndModify(const AccountId& account_id) {
  return const_cast<User*>(FindUser(account_id));
}

std::optional<std::string> FakeUserManager::GetOwnerEmail() {
  return GetLocalState() ? UserManagerImpl::GetOwnerEmail() : std::nullopt;
}

bool FakeUserManager::IsCurrentUserNonCryptohomeDataEphemeral() const {
  return false;
}

bool FakeUserManager::IsUserLoggedIn() const {
  return logged_in_users_.size() > 0;
}

bool FakeUserManager::IsLoggedInAsUserWithGaiaAccount() const {
  return true;
}

bool FakeUserManager::IsLoggedInAsManagedGuestSession() const {
  const User* active_user = GetActiveUser();
  return active_user && active_user->GetType() == UserType::kPublicAccount;
}

bool FakeUserManager::IsLoggedInAsGuest() const {
  const User* active_user = GetActiveUser();
  return active_user && active_user->GetType() == UserType::kGuest;
}

bool FakeUserManager::IsLoggedInAsKioskApp() const {
  const User* active_user = GetActiveUser();
  return active_user ? active_user->GetType() == UserType::kKioskApp : false;
}

bool FakeUserManager::IsLoggedInAsWebKioskApp() const {
  const User* active_user = GetActiveUser();
  return active_user ? active_user->GetType() == UserType::kWebKioskApp : false;
}

bool FakeUserManager::IsLoggedInAsAnyKioskApp() const {
  const User* active_user = GetActiveUser();
  return active_user && active_user->IsKioskType();
}

bool FakeUserManager::IsLoggedInAsStub() const {
  return false;
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

bool FakeUserManager::IsGuestSessionAllowed() const {
  return true;
}

bool FakeUserManager::IsGaiaUserAllowed(const User& user) const {
  return true;
}

bool FakeUserManager::IsUserAllowed(const User& user) const {
  return true;
}

bool FakeUserManager::IsDeviceLocalAccountMarkedForRemoval(
    const AccountId& account_id) const {
  return false;
}

bool FakeUserManager::IsDeprecatedSupervisedAccountId(
    const AccountId& account_id) const {
  return false;
}

}  // namespace user_manager
