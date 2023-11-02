// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/test_attempt_state.h"

#include <utility>

#include "components/user_manager/user_type.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

TestAttemptState::TestAttemptState(std::unique_ptr<UserContext> credentials)
    : AuthAttemptState(std::move(credentials)) {}

TestAttemptState::~TestAttemptState() = default;

void TestAttemptState::PresetOnlineLoginComplete() {
  online_complete_ = true;
}

void TestAttemptState::PresetCryptohomeStatus(
    cryptohome::MountError cryptohome_code) {
  cryptohome_complete_ = true;
  cryptohome_code_ = cryptohome_code;
}

bool TestAttemptState::online_complete() {
  return online_complete_;
}

bool TestAttemptState::cryptohome_complete() {
  return cryptohome_complete_;
}

cryptohome::MountError TestAttemptState::cryptohome_code() {
  return cryptohome_code_;
}

}  // namespace ash
