// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_LOGIN_STATE_SCOPED_TEST_PUBLIC_SESSION_LOGIN_STATE_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_LOGIN_STATE_SCOPED_TEST_PUBLIC_SESSION_LOGIN_STATE_H_

namespace ash {

// A class to start and shutdown public session state for a test. Only one
// instance is allowed to exist at a given time. To be instantiated on the stack
// (so it nicely cleans up after going out of scope).
class ScopedTestPublicSessionLoginState {
 public:
  ScopedTestPublicSessionLoginState();
  ~ScopedTestPublicSessionLoginState();
  ScopedTestPublicSessionLoginState(const ScopedTestPublicSessionLoginState&) =
      delete;
  ScopedTestPublicSessionLoginState& operator=(
      const ScopedTestPublicSessionLoginState&) = delete;

 private:
  bool needs_shutdown_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_LOGIN_STATE_SCOPED_TEST_PUBLIC_SESSION_LOGIN_STATE_H_
