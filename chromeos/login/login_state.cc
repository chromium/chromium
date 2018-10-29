// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/login_state.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "chromeos/chromeos_switches.h"
#include "components/device_event_log/device_event_log.h"

namespace chromeos {

namespace {

// When running a Chrome OS build outside of a device (i.e. on a developer's
// workstation) and not running as login-manager, pretend like we're always
// logged in.
bool AlwaysLoggedInByDefault() {
  return !base::SysInfo::IsRunningOnChromeOS() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kLoginManager);
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

void LoginState::SetLoggedInStateAndPrimaryUser(
    LoggedInState state,
    LoggedInUserType type,
    const std::string& primary_user_hash) {
  DCHECK(type != LOGGED_IN_USER_NONE);
  primary_user_hash_ = primary_user_hash;
  LOGIN_LOG(EVENT) << "LoggedInStateUser: " << primary_user_hash;
  SetLoggedInState(state, type);
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

bool LoginState::IsPublicSessionUser() const {
  return logged_in_user_type_ == LOGGED_IN_USER_PUBLIC_ACCOUNT ||
         logged_in_user_type_ == LOGGED_IN_USER_PUBLIC_ACCOUNT_MANAGED;
}

bool LoginState::ArePublicSessionRestrictionsEnabled() const {
  return logged_in_user_type_ == LOGGED_IN_USER_PUBLIC_ACCOUNT;
}

bool LoginState::IsKioskApp() const {
  return logged_in_user_type_ == LOGGED_IN_USER_KIOSK_APP;
}

bool LoginState::IsChildUser() const {
  return logged_in_user_type_ == LOGGED_IN_USER_CHILD;
}

bool LoginState::UserHasNetworkProfile() const {
  if (!IsUserLoggedIn())
    return false;
  return !IsPublicSessionUser();
}

bool LoginState::IsUserAuthenticated() const {
  return logged_in_user_type_ == LOGGED_IN_USER_REGULAR ||
         logged_in_user_type_ == LOGGED_IN_USER_OWNER ||
         logged_in_user_type_ == LOGGED_IN_USER_SUPERVISED ||
         logged_in_user_type_ == LOGGED_IN_USER_CHILD;
}

bool LoginState::IsUserGaiaAuthenticated() const {
  return logged_in_user_type_ == LOGGED_IN_USER_REGULAR ||
         logged_in_user_type_ == LOGGED_IN_USER_OWNER ||
         logged_in_user_type_ == LOGGED_IN_USER_CHILD;
}

// Private methods

LoginState::LoginState() : logged_in_state_(LOGGED_IN_NONE),
                           logged_in_user_type_(LOGGED_IN_USER_NONE),
                           always_logged_in_(AlwaysLoggedInByDefault()) {
}

LoginState::~LoginState() = default;

void LoginState::NotifyObservers() {
  for (auto& observer : observer_list_)
    observer.LoggedInStateChanged();
}

}  // namespace chromeos
