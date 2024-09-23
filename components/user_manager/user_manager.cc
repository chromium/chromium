// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_manager.h"

#include "base/logging.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"

namespace user_manager {

UserManager* UserManager::instance = nullptr;

UserManager::Observer::~Observer() = default;

void UserManager::Observer::LocalStateChanged(UserManager* user_manager) {}

void UserManager::Observer::OnUserListLoaded() {}

void UserManager::Observer::OnDeviceLocalUserListUpdated() {}

void UserManager::Observer::OnUserLoggedIn(const User& user) {}

void UserManager::Observer::OnUserImageChanged(const User& user) {}

void UserManager::Observer::OnUserImageIsEnterpriseManagedChanged(
    const User& user,
    bool is_enterprise_managed) {}

void UserManager::Observer::OnUserProfileCreated(const User& user) {}

void UserManager::Observer::OnUserProfileImageUpdateFailed(const User& user) {}

void UserManager::Observer::OnUserProfileImageUpdated(
    const User& user,
    const gfx::ImageSkia& profile_image) {}

void UserManager::Observer::OnUsersSignInConstraintsChanged() {}

void UserManager::Observer::OnUserAffiliationUpdated(const User& user) {}

void UserManager::Observer::OnUserRemoved(const AccountId& account_id,
                                          UserRemovalReason reason) {}

void UserManager::Observer::OnUserToBeRemoved(const AccountId& account_id) {}

void UserManager::Observer::OnUserNotAllowed(const std::string& user_email) {}

void UserManager::UserSessionStateObserver::ActiveUserChanged(
    User* active_user) {}

void UserManager::UserSessionStateObserver::UserAddedToSession(
    const User* active_user) {}

void UserManager::UserSessionStateObserver::OnLoginStateUpdated(
    const User* active_user) {}

UserManager::UserSessionStateObserver::~UserSessionStateObserver() {}

UserManager::UserAccountData::UserAccountData(
    const std::u16string& display_name,
    const std::u16string& given_name,
    const std::string& locale)
    : display_name_(display_name), given_name_(given_name), locale_(locale) {}

UserManager::UserAccountData::~UserAccountData() {}

UserManager::DeviceLocalAccountInfo::DeviceLocalAccountInfo(std::string user_id,
                                                            UserType type)
    : user_id(std::move(user_id)), type(type) {}

UserManager::DeviceLocalAccountInfo::DeviceLocalAccountInfo(
    const UserManager::DeviceLocalAccountInfo&) = default;

UserManager::DeviceLocalAccountInfo&
UserManager::DeviceLocalAccountInfo::operator=(
    const UserManager::DeviceLocalAccountInfo&) = default;

UserManager::DeviceLocalAccountInfo::~DeviceLocalAccountInfo() = default;

void UserManager::Initialize() {
  DCHECK(!UserManager::instance);
  UserManager::SetInstance(this);
}

// static
bool UserManager::IsInitialized() {
  return UserManager::instance;
}

void UserManager::Destroy() {
  DCHECK(UserManager::instance == this);
  UserManager::SetInstance(nullptr);
}

// static
UserManager* user_manager::UserManager::Get() {
  CHECK(UserManager::instance);
  return UserManager::instance;
}

UserManager::~UserManager() {
}

// static
void UserManager::SetInstance(UserManager* user_manager) {
  UserManager::instance = user_manager;
}

// static
UserManager* user_manager::UserManager::GetForTesting() {
  return UserManager::instance;
}

// static
UserManager* UserManager::SetForTesting(UserManager* user_manager) {
  UserManager* previous_instance = UserManager::instance;
  UserManager::instance = user_manager;
  return previous_instance;
}

UserType UserManager::CalculateUserType(const AccountId& account_id,
                                        const User* user,
                                        const bool browser_restart,
                                        const bool is_child) const {
  if (account_id == GuestAccountId()) {
    return UserType::kGuest;
  }

  // This may happen after browser crash after device account was marked for
  // removal, but before clean exit.
  if (browser_restart && IsDeviceLocalAccountMarkedForRemoval(account_id))
    return UserType::kPublicAccount;

  // If user already exists
  if (user) {
    // This branch works for any other user type, including PUBLIC_ACCOUNT.
    const UserType user_type = user->GetType();
    if (user_type == UserType::kChild || user_type == UserType::kRegular) {
      const UserType new_user_type =
          is_child ? UserType::kChild : UserType::kRegular;
      if (new_user_type != user_type) {
        LOG(WARNING) << "Child user type has changed: " << user_type << " => "
                     << new_user_type;
      }
      return new_user_type;
    } else if (is_child) {
      LOG(FATAL) << "Incorrect child user type " << user_type;
    }

    // TODO(rsorokin): Check for reverse: account_id AD type should imply
    // AD user type.
    if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY) {
      LOG(FATAL) << "Incorrect AD user type " << user_type;
    }

    return user_type;
  }

  // User is new
  if (is_child)
    return UserType::kChild;

  CHECK(account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY);

  return UserType::kRegular;
}

bool UserManager::IsUserAllowed(const user_manager::User& user,
                                bool is_guest_allowed,
                                bool is_user_allowlisted) {
  DCHECK(user.GetType() == UserType::kRegular ||
         user.GetType() == UserType::kGuest ||
         user.GetType() == UserType::kChild);

  if (user.GetType() == UserType::kGuest && !is_guest_allowed) {
    return false;
  }
  if (user.HasGaiaAccount() && !is_user_allowlisted) {
    return false;
  }
  return true;
}

}  // namespace user_manager
