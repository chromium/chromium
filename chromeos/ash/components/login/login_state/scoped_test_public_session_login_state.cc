// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/login_state/scoped_test_public_session_login_state.h"

#include "chromeos/ash/components/login/login_state/login_state.h"

namespace ash {

namespace {

bool g_instance_exists = false;

}  // namespace

ScopedTestPublicSessionLoginState::ScopedTestPublicSessionLoginState() {
  // Allow only one instance of this class.
  CHECK(!g_instance_exists);
  g_instance_exists = true;

  // Set Public Session state.
  if (!LoginState::IsInitialized()) {
    LoginState::Initialize();
    needs_shutdown_ = true;
  }
  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
}

ScopedTestPublicSessionLoginState::~ScopedTestPublicSessionLoginState() {
  // Reset state at the end of test.
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);
  if (needs_shutdown_)
    LoginState::Shutdown();

  g_instance_exists = false;
}

}  // namespace ash
