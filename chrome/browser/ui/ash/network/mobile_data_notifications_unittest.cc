// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/mobile_data_notifications.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const char kCellularDevicePath[] = "/device/stub_cellular_device1";
const char kCellularServicePath[] = "/service/cellular1";
const char kCellularGuid[] = "cellular1_guid";

const char kNotificationId[] = "chrome://settings/internet/mobile_data";
const char kTestUserName[] = "test-user@example.com";

class NetworkConnectTestDelegate : public ash::NetworkConnect::Delegate {
 public:
  NetworkConnectTestDelegate() {}

  NetworkConnectTestDelegate(const NetworkConnectTestDelegate&) = delete;
  NetworkConnectTestDelegate& operator=(const NetworkConnectTestDelegate&) =
      delete;

  ~NetworkConnectTestDelegate() override {}

  void ShowNetworkConfigure(const std::string& network_id) override {}
  void ShowNetworkSettings(const std::string& network_id) override {}
  bool ShowEnrollNetwork(const std::string& network_id) override {
    return false;
  }
  void ShowMobileSetupDialog(const std::string& network_id) override {}
  void ShowCarrierAccountDetail(const std::string& network_id) override {}
  void ShowCarrierUnlockNotification() override {}
  void ShowPortalSignin(const std::string& network_id,
                        ash::NetworkConnect::Source source) override {}
  void ShowNetworkConnectError(const std::string& error_name,
                               const std::string& network_id) override {}
  void ShowMobileActivationError(const std::string& network_id) override {}
};

class MobileDataNotificationsTest : public testing::Test {
 public:
  MobileDataNotificationsTest() {}

  MobileDataNotificationsTest(const MobileDataNotificationsTest&) = delete;
  MobileDataNotificationsTest& operator=(const MobileDataNotificationsTest&) =
      delete;

  ~MobileDataNotificationsTest() override {}

  void SetUp() override {
    session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
    testing::Test::SetUp();
    ash::LoginState::Initialize();
    SetupUserManagerAndProfileManager();
    SetupSystemNotifications();
    AddUserAndSetActive(kTestUserName);
    SetupNetworkShillState();
    base::RunLoop().RunUntilIdle();
    network_connect_delegate_ = std::make_unique<NetworkConnectTestDelegate>();
    ash::NetworkConnect::Initialize(network_connect_delegate_.get());
    mobile_data_notifications_ = std::make_unique<MobileDataNotifications>();
  }

  void TearDown() override {
    mobile_data_notifications_.reset();
    ash::NetworkConnect::Shutdown();
    network_connect_delegate_.reset();
    profile_manager_.reset();
    user_manager_enabler_.reset();
    ash::LoginState::Shutdown();
    testing::Test::TearDown();
  }

 protected:
  void SetupSystemNotifications() {
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    // Passing nullptr sets up |display_service_| with system notifications.
    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);
  }
  void SetupUserManagerAndProfileManager() {
    user_manager_ = new ash::FakeChromeUserManager;
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(user_manager_.get()));

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
  }

  void SetupNetworkShillState() {
    base::RunLoop().RunUntilIdle();

    // Create a cellular device with provider.
    ash::ShillDeviceClient::TestInterface* device_test =
        network_handler_test_helper_.device_test();
    device_test->ClearDevices();
    device_test->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                           "stub_cellular_device1");
    base::Value::Dict home_provider;
    home_provider.Set("name", "Cellular1_Provider");
    home_provider.Set("country", "us");
    device_test->SetDeviceProperty(kCellularDevicePath,
                                   shill::kHomeProviderProperty,
                                   base::Value(std::move(home_provider)),
                                   /*notify_changed=*/true);

    // Create a cellular network and activate it.
    ash::ShillServiceClient::TestInterface* service_test =
        network_handler_test_helper_.service_test();
    service_test->ClearServices();
    service_test->AddService(kCellularServicePath, kCellularGuid,
                             "cellular1" /* name */, shill::kTypeCellular,
                             shill::kStateIdle, true /* visible */);
    service_test->SetServiceProperty(
        kCellularServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateActivated));
    service_test->SetServiceProperty(
        kCellularServicePath, shill::kConnectableProperty, base::Value(true));
  }

  PrefService* pref_service() {
    return ProfileManager::GetActiveUserProfile()->GetPrefs();
  }

  void AddUserAndSetActive(std::string email) {
    const AccountId test_account_id(AccountId::FromUserEmail(email));
    TestingProfile* profile =
        profile_manager_->CreateTestingProfile(test_account_id.GetUserEmail());
    user_manager_->AddUser(test_account_id);
    user_manager_->LoginUser(test_account_id);
    user_manager_->SwitchActiveUser(test_account_id);
    ASSERT_TRUE(ProfileManager::GetActiveUserProfile() == profile);
  }

  content::BrowserTaskEnvironment task_environment_;
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<MobileDataNotifications> mobile_data_notifications_;
  std::unique_ptr<NetworkConnectTestDelegate> network_connect_delegate_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

// Verify that basic network setup does not trigger notification.
TEST_F(MobileDataNotificationsTest, SimpleSetup) {
  pref_service()->SetBoolean(prefs::kShowMobileDataNotification, true);
  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));
}

// Verify that switching to cellular whiile pref is false does not display a
// notification.
TEST_F(MobileDataNotificationsTest, NotificationAlreadyShown) {
  pref_service()->SetBoolean(prefs::kShowMobileDataNotification, false);

  ash::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  // Wait for async ConnectToNetworkId to take effect.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));
}

// Verify that switching to cellular while pref is true displays notification.
TEST_F(MobileDataNotificationsTest, DisplayNotification) {
  pref_service()->SetBoolean(prefs::kShowMobileDataNotification, true);

  ash::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  // Wait for async ConnectToNetworkId to take effect.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(display_service_->GetNotification(kNotificationId));
}

// Verify that displaying the notification toggles the profile pref.
TEST_F(MobileDataNotificationsTest, TogglesPref) {
  pref_service()->SetBoolean(prefs::kShowMobileDataNotification, true);

  ash::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  // Wait for async ConnectToNetworkId to take effect.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(pref_service()->GetBoolean(prefs::kShowMobileDataNotification));
}

// Verify that session changes display the notification if cellular is
// connected.
TEST_F(MobileDataNotificationsTest, SessionUpdateDisplayNotification) {
  // Set up cellular network, don't trigger notification.
  ash::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  pref_service()->SetBoolean(prefs::kShowMobileDataNotification, false);
  // Process network observer update.
  base::RunLoop().RunUntilIdle();
  // Make sure notification hasn't been triggered.
  EXPECT_FALSE(pref_service()->GetBoolean(prefs::kShowMobileDataNotification));

  AddUserAndSetActive("other-user@example.com");

  EXPECT_TRUE(display_service_->GetNotification(kNotificationId));
}

// Verify that session changes does not dispalay the notification if celluar is
// not connected.
TEST_F(MobileDataNotificationsTest, SessionUpdateNoNotification) {
  pref_service()->SetBoolean(prefs::kShowMobileDataNotification, true);

  AddUserAndSetActive("other-user@example.com");

  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));
}

}  // namespace
