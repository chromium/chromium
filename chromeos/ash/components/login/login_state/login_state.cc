// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/login_state/login_state.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

// When running a Chrome OS build outside of a device (i.e. on a developer's
// workstation) and not running as login-manager, pretend like we're always
// logged in.
bool AlwaysLoggedInByDefault() {
  return !base::SysInfo::IsRunningOnChromeOS() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kLoginManager);
}

LoginState::LoggedInUserType GetLoggedInUserTypeFromUser(
    const user_manager::User& active_user) {
  switch (active_user.GetType()) {
    case user_manager::UserType::kRegular:
      return LoginState::LOGGED_IN_USER_REGULAR;
    case user_manager::UserType::kGuest:
      return LoginState::LOGGED_IN_USER_GUEST;
    case user_manager::UserType::kPublicAccount:
      return LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT;
    case user_manager::UserType::kChild:
      return LoginState::LOGGED_IN_USER_CHILD;
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return LoginState::LOGGED_IN_USER_KIOSK;
      // Since there is no default, the compiler warns about unhandled types.
  }
  NOTREACHED_IN_MIGRATION()
      << "Invalid type for active user: " << active_user.GetType();
  return LoginState::LOGGED_IN_USER_REGULAR;
}

}  // namespace

static LoginState* g_login_state = NULL;

// static
void LoginState::Initialize() {
  CHECK(!g_login_state);
  g_login_state = new LoginState();
}

// static
void LoginState::Shutdown() {
  CHECK(g_login_state);
  delete g_login_state;
  g_login_state = NULL;
}

// static
LoginState* LoginState::Get() {
  CHECK(g_login_state) << "LoginState::Get() called before Initialize()";
  return g_login_state;
}

// static
bool LoginState::IsInitialized() {
  return g_login_state;
}

void LoginState::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void LoginState::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void LoginState::SetLoggedInState(LoggedInState state, LoggedInUserType type) {
  if (state == logged_in_state_ && type == logged_in_user_type_)
    return;
  LOGIN_LOG(EVENT) << "LoggedInState: " << state << " UserType: " << type;
  logged_in_state_ = state;
  logged_in_user_type_ = type;
  NotifyObservers();
}

LoginState::LoggedInUserType LoginState::GetLoggedInUserType() const {
  return logged_in_user_type_;
}

bool LoginState::IsUserLoggedIn() const {
  if (always_logged_in_)
    return true;
  return logged_in_state_ == LOGGED_IN_ACTIVE;
}

bool LoginState::IsInSafeMode() const {
  DCHECK(!always_logged_in_ || logged_in_state_ != LOGGED_IN_SAFE_MODE);
  return logged_in_state_ == LOGGED_IN_SAFE_MODE;
}

bool LoginState::IsGuestSessionUser() const {
  return logged_in_user_type_ == LOGGED_IN_USER_GUEST;
}

bool LoginState::IsManagedGuestSessionUser() const {
  return logged_in_user_type_ == LOGGED_IN_USER_PUBLIC_ACCOUNT;
}

bool LoginState::IsKioskSession() const {
  return logged_in_user_type_ == LOGGED_IN_USER_KIOSK;
}

bool LoginState::IsChildUser() const {
  return logged_in_user_type_ == LOGGED_IN_USER_CHILD;
}

bool LoginState::UserHasNetworkProfile() const {
  if (!IsUserLoggedIn())
    return false;
  return !IsManagedGuestSessionUser();
}

bool LoginState::IsUserAuthenticated() const {
  return logged_in_user_type_ == LOGGED_IN_USER_REGULAR ||
         logged_in_user_type_ == LOGGED_IN_USER_CHILD;
}

const std::string& LoginState::primary_user_hash() const {
  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager) {
    return base::EmptyString();
  }

  auto* primary_user = user_manager->GetPrimaryUser();
  if (!primary_user) {
    return base::EmptyString();
  }

  return primary_user->username_hash();
}

void LoginState::OnUserManagerCreated(user_manager::UserManager* user_manager) {
  observation_.Observe(user_manager);
}

void LoginState::OnUserManagerWillBeDestroyed(
    user_manager::UserManager* user_manager) {
  observation_.Reset();
}

void LoginState::OnLoginStateUpdated(const user_manager::User* active_user) {
  LoginState::LoggedInState logged_in_state = LOGGED_IN_NONE;
  LoginState::LoggedInUserType logged_in_user_type = LOGGED_IN_USER_NONE;
  if (active_user) {
    logged_in_state = LOGGED_IN_ACTIVE;
    logged_in_user_type = GetLoggedInUserTypeFromUser(*active_user);
  }
  SetLoggedInState(logged_in_state, logged_in_user_type);
}

// Private methods

LoginState::LoginState()
    : logged_in_state_(LOGGED_IN_NONE),
      logged_in_user_type_(LOGGED_IN_USER_NONE),
      always_logged_in_(AlwaysLoggedInByDefault()) {}

LoginState::~LoginState() = default;

void LoginState::NotifyObservers() {
  for (auto& observer : observer_list_)
    observer.LoggedInStateChanged();
}

}  // namespace ash
