// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mgs/managed_guest_session_test_utils.h"

#include "chromeos/ash/components/login/login_state/login_state.h"

namespace chromeos {

FakeManagedGuestSession::FakeManagedGuestSession(bool initialize_login_state)
    : initialize_login_state_(initialize_login_state) {
  SetUpFakeManagedGuestSession();
}

FakeManagedGuestSession::~FakeManagedGuestSession() {
  TearDownFakeManagedGuestSession();
}

void FakeManagedGuestSession::SetUpFakeManagedGuestSession() {
  if (initialize_login_state_) {
    ash::LoginState::Initialize();
  }
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      ash::LoginState::LoggedInUserType::LOGGED_IN_USER_PUBLIC_ACCOUNT);
}

void FakeManagedGuestSession::TearDownFakeManagedGuestSession() {
  if (initialize_login_state_) {
    ash::LoginState::Shutdown();
  }
}

}  // namespace chromeos
