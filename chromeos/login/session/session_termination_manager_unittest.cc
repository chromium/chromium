// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/session/session_termination_manager.h"

#include "base/macros.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class SessionTerminationManagerTest : public testing::Test {
 public:
  SessionTerminationManagerTest() {
    PowerManagerClient::InitializeFake();
    power_client_ = FakePowerManagerClient::Get();
    CryptohomeClient::InitializeFake();
    SessionManagerClient::InitializeFake();
  }
  ~SessionTerminationManagerTest() override {
    CryptohomeClient::Shutdown();
    PowerManagerClient::Shutdown();
    SessionManagerClient::Shutdown();
  }

 protected:
  FakePowerManagerClient* power_client_;
  SessionTerminationManager session_termination_manager_;

  DISALLOW_COPY_AND_ASSIGN(SessionTerminationManagerTest);
};

// The device is not locked to single user. Check that no reboot is triggered
// on sign out.
TEST_F(SessionTerminationManagerTest, NoRebootTest) {
  session_termination_manager_.StopSession();
  EXPECT_EQ(0, power_client_->num_request_restart_calls());
}

// The device is locked to single user. Check that reboot is triggered on user
// sign out.
TEST_F(SessionTerminationManagerTest, RebootTest) {
  session_termination_manager_.SetDeviceLockedToSingleUser();
  session_termination_manager_.StopSession();
  EXPECT_EQ(1, power_client_->num_request_restart_calls());
}

}  // namespace chromeos
