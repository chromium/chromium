// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_TEST_ATTEMPT_STATE_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_TEST_ATTEMPT_STATE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "chromeos/ash/components/login/auth/auth_attempt_state.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

namespace ash {

class UserContext;

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) TestAttemptState
    : public AuthAttemptState {
 public:
  explicit TestAttemptState(std::unique_ptr<UserContext> credentials);

  TestAttemptState(const TestAttemptState&) = delete;
  TestAttemptState& operator=(const TestAttemptState&) = delete;

  ~TestAttemptState() override;

  // Act as though an online login attempt completed already.
  void PresetOnlineLoginComplete();

  // Act as though an cryptohome login attempt completed already.
  void PresetCryptohomeStatus(cryptohome::MountError cryptohome_code);

  // To allow state to be queried on the main thread during tests.
  bool online_complete() override;
  bool cryptohome_complete() override;
  cryptohome::MountError cryptohome_code() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_TEST_ATTEMPT_STATE_H_
