// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_TEST_ATTEMPT_STATE_H_
#define CHROMEOS_LOGIN_AUTH_TEST_ATTEMPT_STATE_H_

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/login/auth/auth_attempt_state.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

namespace chromeos {

class UserContext;

class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) TestAttemptState
    : public AuthAttemptState {
 public:
  TestAttemptState(const UserContext& credentials);

  ~TestAttemptState() override;

  // Act as though an online login attempt completed already.
  void PresetOnlineLoginComplete();

  // Act as though an cryptohome login attempt completed already.
  void PresetCryptohomeStatus(cryptohome::MountError cryptohome_code);

  // To allow state to be queried on the main thread during tests.
  bool online_complete() override;
  bool cryptohome_complete() override;
  cryptohome::MountError cryptohome_code() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAttemptState);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source code migration is finished.
namespace ash {
using ::chromeos::TestAttemptState;
}

#endif  // CHROMEOS_LOGIN_AUTH_TEST_ATTEMPT_STATE_H_
