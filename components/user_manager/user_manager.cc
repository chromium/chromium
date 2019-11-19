// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_manager.h"

#include "base/logging.h"
#include "components/account_id/account_id.h"

namespace user_manager {

const char kRegularUsersPref[] = "LoggedInUsers";

UserManager* UserManager::instance = nullptr;

UserManager::Observer::~Observer() = default;

void UserManager::Observer::LocalStateChanged(UserManager* user_manager) {}

void UserManager::Observer::OnUserImageChanged(const User& user) {}

void UserManager::Observer::OnUserProfileImageUpdateFailed(const User& user) {}

void UserManager::Observer::OnUserProfileImageUpdated(
    const User& user,
    const gfx::ImageSkia& profile_image) {}

void UserManager::Observer::OnUsersSignInConstraintsChanged() {}

void UserManager::UserSessionStateObserver::ActiveUserChanged(
    User* active_user) {}

void UserManager::UserSessionStateObserver::UserAddedToSession(
    const User* active_user) {}

void UserManager::UserSessionStateObserver::ActiveUserHashChanged(
    const std::string& hash) {}

UserManager::UserSessionStateObserver::~UserSessionStateObserver() {}

UserManager::UserAccountData::UserAccountData(
    const base::string16& display_name,
    const base::string16& given_name,
    const std::string& locale)
    : display_name_(display_name), given_name_(given_name), locale_(locale) {}

UserManager::UserAccountData::~UserAccountData() {}

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
  if (IsGuestAccountId(account_id))
    return USER_TYPE_GUEST;

  // This may happen after browser crash after device account was marked for
  // removal, but before clean exit.
  if (browser_restart && IsDeviceLocalAccountMarkedForRemoval(account_id))
    return USER_TYPE_PUBLIC_ACCOUNT;

  // If user already exists
  if (user) {
    // This branch works for any other user type, including PUBLIC_ACCOUNT.
    const UserType user_type = user->GetType();
    if (user_type == USER_TYPE_CHILD || user_type == USER_TYPE_REGULAR) {
      const UserType new_user_type =
          is_child ? USER_TYPE_CHILD : USER_TYPE_REGULAR;
      if (new_user_type != user_type) {
        LOG(WARNING) << "Child user type has changed: " << user_type << " => "
                     << new_user_type;
      }
      return new_user_type;
    } else if (is_child) {
      LOG(FATAL) << "Incorrect child user type " << user_type;
    }

    // TODO (rsorokin): Check for reverse: account_id AD type should imply
    // AD user type.
    if (user_type == USER_TYPE_ACTIVE_DIRECTORY &&
        account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY) {
      LOG(FATAL) << "Incorrect AD user type " << user_type;
    }

    return user_type;
  }

  // User is new
  if (is_child)
    return USER_TYPE_CHILD;

  if (IsSupervisedAccountId(account_id))
    return USER_TYPE_SUPERVISED;

  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY)
    return USER_TYPE_ACTIVE_DIRECTORY;

  return USER_TYPE_REGULAR;
}

}  // namespace user_manager
