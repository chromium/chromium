// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_LOGIN_STATE_SCOPED_TEST_PUBLIC_SESSION_LOGIN_STATE_H_
#define CHROMEOS_LOGIN_LOGIN_STATE_SCOPED_TEST_PUBLIC_SESSION_LOGIN_STATE_H_

#include "base/macros.h"
#include "chromeos/login/login_state/login_state.h"

namespace chromeos {

// A class to start and shutdown public session state for a test. Only one
// instance is allowed to exist at a given time. To be instantiated on the stack
// (so it nicely cleans up after going out of scope).
class ScopedTestPublicSessionLoginState {
 public:
  ScopedTestPublicSessionLoginState(
      LoginState::LoggedInUserType user_type =
          LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
  ~ScopedTestPublicSessionLoginState();

 private:
  bool needs_shutdown_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedTestPublicSessionLoginState);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_LOGIN_STATE_SCOPED_TEST_PUBLIC_SESSION_LOGIN_STATE_H_
