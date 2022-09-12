// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/login_state/scoped_test_public_session_login_state.h"

namespace chromeos {

namespace {

bool g_instance_exists = false;

}  // namespace

ScopedTestPublicSessionLoginState::ScopedTestPublicSessionLoginState(
    LoginState::LoggedInUserType user_type) {
  // Sanity check - allow only public session state.
  CHECK(user_type == LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT ||
        user_type == LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT_MANAGED);
  // Allow only one instance of this class.
  CHECK(!g_instance_exists);
  g_instance_exists = true;

  // Set Public Session state.
  if (!LoginState::IsInitialized()) {
    LoginState::Initialize();
    needs_shutdown_ = true;
  }
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE, user_type);
}

ScopedTestPublicSessionLoginState::~ScopedTestPublicSessionLoginState() {
  // Reset state at the end of test.
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);
  if (needs_shutdown_)
    LoginState::Shutdown();

  g_instance_exists = false;
}

}  // namespace chromeos
