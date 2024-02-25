// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/session/session_termination_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class SessionTerminationManagerTest : public testing::Test {
 public:
  SessionTerminationManagerTest() {
    chromeos::PowerManagerClient::InitializeFake();
    power_client_ = chromeos::FakePowerManagerClient::Get();
    CryptohomeMiscClient::InitializeFake();
    SessionManagerClient::InitializeFake();
  }

  SessionTerminationManagerTest(const SessionTerminationManagerTest&) = delete;
  SessionTerminationManagerTest& operator=(
      const SessionTerminationManagerTest&) = delete;

  ~SessionTerminationManagerTest() override {
    CryptohomeMiscClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    SessionManagerClient::Shutdown();
  }

 protected:
  raw_ptr<chromeos::FakePowerManagerClient, DanglingUntriaged> power_client_;
  SessionTerminationManager session_termination_manager_;
};

// The device is not locked to single user. Check that no reboot is triggered
// on sign out.
TEST_F(SessionTerminationManagerTest, NoRebootTest) {
  session_termination_manager_.StopSession(
      login_manager::SessionStopReason::OWNER_REQUIRED);
  EXPECT_EQ(0, power_client_->num_request_restart_calls());
}

// The device is locked to single user. Check that reboot is triggered on user
// sign out.
TEST_F(SessionTerminationManagerTest, RebootLockedToSingleUserTest) {
  session_termination_manager_.SetDeviceLockedToSingleUser();
  session_termination_manager_.StopSession(
      login_manager::SessionStopReason::REQUEST_FROM_SESSION_MANAGER);
  EXPECT_EQ(1, power_client_->num_request_restart_calls());
}

// The reboot command asked to reboot on sign out. Check that reboot is
// triggered on user sign out.
TEST_F(SessionTerminationManagerTest, RebootForRemoteCommandTest) {
  bool callback_called = false;
  session_termination_manager_.SetDeviceRebootOnSignoutForRemoteCommand(
      base::BindLambdaForTesting(
          [&callback_called]() { callback_called = true; }));
  session_termination_manager_.StopSession(
      login_manager::SessionStopReason::REQUEST_FROM_SESSION_MANAGER);
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(1, power_client_->num_request_restart_calls());
}

}  // namespace ash
