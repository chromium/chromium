// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(TestSudoHelperClientTest, WhoAmI) {
  auto result = TestSudoHelperClient().RunCommand("whoami");
  EXPECT_EQ(result.return_code, 0);
  EXPECT_EQ(base::TrimString(result.output, " \n", base::TRIM_ALL), "root");
}

TEST(TestSudoHelperClientTest, ResetSystemStateFiles) {
  TestSudoHelperClient().RunCommand("touch /home/chronos/.oobe_completed");
  TestSudoHelperClient().RunCommand("touch /var/lib/devicesettings/test");

  ASSERT_TRUE(
      base::PathExists(base::FilePath("/home/chronos/.oobe_completed")));
  ASSERT_TRUE(base::PathExists(base::FilePath("/var/lib/devicesettings/test")));

  auto result = TestSudoHelperClient().RunCommand("./reset_dut.py");
  EXPECT_EQ(result.return_code, 0);

  EXPECT_FALSE(
      base::PathExists(base::FilePath("/home/chronos/.oobe_completed")));
  EXPECT_FALSE(
      base::PathExists(base::FilePath("/var/lib/devicesettings/test")));
}

TEST(TestSudoHelperClientTest, StartStopSessionManager) {
  content::BrowserTaskEnvironment task_environment_;
  TestSudoHelperClient client;
  base::RunLoop wait_for_stop_loop;

  auto result = client.StartSessionManager(wait_for_stop_loop.QuitClosure());
  ASSERT_EQ(result.return_code, 0);

  // Stop session_manager through `StopSessionManager`.
  ASSERT_EQ(client.StopSessionManager().return_code, 0);

  // Wait for session_manager to stop.
  wait_for_stop_loop.Run();
}

TEST(TestSudoHelperClientTest, ExitSessionManager) {
  content::BrowserTaskEnvironment task_environment_;

  ash::DBusThreadManager::Initialize();
  dbus::Bus* bus = ash::DBusThreadManager::Get()->GetSystemBus();
  ash::SessionManagerClient::Initialize(bus);

  TestSudoHelperClient client;
  base::RunLoop wait_for_stop_loop;

  ASSERT_EQ(
      client.StartSessionManager(wait_for_stop_loop.QuitClosure()).return_code,
      0);

  // Wait for session_manager dbus serive to be available.
  base::RunLoop service_availabe_wait_loop;
  auto* session_manager_client = ash::SessionManagerClient::Get();
  auto on_service_available = [&service_availabe_wait_loop](bool available) {
    EXPECT_TRUE(available);
    service_availabe_wait_loop.Quit();
  };
  session_manager_client->WaitForServiceToBeAvailable(
      base::BindLambdaForTesting(on_service_available));
  service_availabe_wait_loop.Run();

  // Stop session manager by StopSession dbus call.
  session_manager_client->StopSession(
      login_manager::SessionStopReason::USER_REQUESTS_SIGNOUT);

  // Wait for session_manager daemon to stop.
  wait_for_stop_loop.Run();

  // Helper StopSessionManager call is a no-op but should still succeed.
  ASSERT_EQ(client.StopSessionManager().return_code, 0);

  ash::SessionManagerClient::Shutdown();
  ash::DBusThreadManager::Shutdown();
}
