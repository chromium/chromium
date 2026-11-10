// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/mobile_data_notifications.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/check_deref.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/message_center/message_center.h"

namespace {

const char kCellularDevicePath[] = "/device/stub_cellular_device1";
const char kCellularServicePath[] = "/service/cellular1";
const char kCellularGuid[] = "cellular1_guid";

const char kNotificationId[] = "chrome://settings/internet/mobile_data";
constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test-user@example.com",
                                            GaiaId::Literal("1111111111"));
constexpr AccountId::Literal kOtherAccountId =
    AccountId::Literal::FromUserEmailGaiaId("other-user@example.com",
                                            GaiaId::Literal("2222222222"));

class NetworkConnectTestDelegate : public ash::NetworkConnect::Delegate {
 public:
  NetworkConnectTestDelegate() = default;

  NetworkConnectTestDelegate(const NetworkConnectTestDelegate&) = delete;
  NetworkConnectTestDelegate& operator=(const NetworkConnectTestDelegate&) =
      delete;

  ~NetworkConnectTestDelegate() override = default;

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
  MobileDataNotificationsTest() = default;

  MobileDataNotificationsTest(const MobileDataNotificationsTest&) = delete;
  MobileDataNotificationsTest& operator=(const MobileDataNotificationsTest&) =
      delete;

  ~MobileDataNotificationsTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    ash::LoginState::Initialize();
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        browser_process->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process));
    // Every user has to be registered before the first session is created.
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(kTestAccountId));
    ASSERT_TRUE(
        user_session_test_environment_->AddRegularUser(kOtherAccountId));
    message_center::MessageCenter::Initialize();
    LogIn(kTestAccountId);
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
    message_center::MessageCenter::Shutdown();
    user_session_test_environment_.reset();
    ash::LoginState::Shutdown();
    testing::Test::TearDown();
  }

 protected:

  void SetupNetworkShillState() {
    base::RunLoop().RunUntilIdle();

    // Create a cellular device with provider.
    ash::ShillDeviceClient::TestInterface* device_test =
        network_handler_test_helper_.device_test();
    device_test->ClearDevices();
    device_test->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                           "stub_cellular_device1");
    base::DictValue home_provider;
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

  // The prefs of `account_id`, read the way the code under test reads them.
  PrefService& GetPrefService(const AccountId& account_id) {
    user_manager::User& user = CHECK_DEREF(
        user_manager::UserManager::Get()->FindUserAndModify(account_id));
    return CHECK_DEREF(user.GetProfilePrefs());
  }

  // Starts a session for `account_id` and makes it the active one. The profile
  // is created along the way, which is what attaches the prefs to the User.
  void LogIn(const AccountId& account_id) {
    user_session_test_environment_->LogIn(account_id);
  }

  const message_center::Notification* GetNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        kNotificationId);
  }

  content::BrowserTaskEnvironment task_environment_;
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<MobileDataNotifications> mobile_data_notifications_;
  std::unique_ptr<NetworkConnectTestDelegate> network_connect_delegate_;
};

// Verify that basic network setup does not trigger notification.
TEST_F(MobileDataNotificationsTest, SimpleSetup) {
  GetPrefService(kTestAccountId)
      .SetBoolean(ash::prefs::kShowMobileDataNotification, true);
  EXPECT_FALSE(GetNotification());
}

// Verify that switching to cellular whiile pref is false does not display a
// notification.
TEST_F(MobileDataNotificationsTest, NotificationAlreadyShown) {
  GetPrefService(kTestAccountId)
      .SetBoolean(ash::prefs::kShowMobileDataNotification, false);

  ash::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  // Wait for async ConnectToNetworkId to take effect.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetNotification());
}

// Verify that switching to cellular while pref is true displays notification.
TEST_F(MobileDataNotificationsTest, DisplayNotification) {
  GetPrefService(kTestAccountId)
      .SetBoolean(ash::prefs::kShowMobileDataNotification, true);

  ash::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  // Wait for async ConnectToNetworkId to take effect.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetNotification());
}

// Verify that displaying the notification toggles the profile pref.
TEST_F(MobileDataNotificationsTest, TogglesPref) {
  GetPrefService(kTestAccountId)
      .SetBoolean(ash::prefs::kShowMobileDataNotification, true);

  ash::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  // Wait for async ConnectToNetworkId to take effect.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetPrefService(kTestAccountId)
                   .GetBoolean(ash::prefs::kShowMobileDataNotification));
}

// Verify that session changes display the notification if cellular is
// connected.
TEST_F(MobileDataNotificationsTest, SessionUpdateDisplayNotification) {
  // Set up cellular network, don't trigger notification.
  ash::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  GetPrefService(kTestAccountId)
      .SetBoolean(ash::prefs::kShowMobileDataNotification, false);
  // Process network observer update.
  base::RunLoop().RunUntilIdle();
  // Make sure notification hasn't been triggered.
  EXPECT_FALSE(GetPrefService(kTestAccountId)
                   .GetBoolean(ash::prefs::kShowMobileDataNotification));

  LogIn(kOtherAccountId);

  EXPECT_TRUE(GetNotification());
}

// Verify that a session change answers for the user becoming active, not for
// the one being switched away from.
TEST_F(MobileDataNotificationsTest, SessionUpdateReadsTheNewUsersPref) {
  // Connect cellular while the active user is owed nothing, so that the
  // connection itself shows no notification.
  GetPrefService(kTestAccountId)
      .SetBoolean(ash::prefs::kShowMobileDataNotification, false);
  ash::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(GetNotification());

  // Put the user being switched away from back in line for a notification.
  GetPrefService(kTestAccountId)
      .SetBoolean(ash::prefs::kShowMobileDataNotification, true);

  LogIn(kOtherAccountId);

  // The notification belongs to the user who became active, so it is their
  // pref that is spent and the other one is left alone.
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetPrefService(kOtherAccountId)
                   .GetBoolean(ash::prefs::kShowMobileDataNotification));
  EXPECT_TRUE(GetPrefService(kTestAccountId)
                  .GetBoolean(ash::prefs::kShowMobileDataNotification));
}

// Verify that session changes does not dispalay the notification if celluar is
// not connected.
TEST_F(MobileDataNotificationsTest, SessionUpdateNoNotification) {
  GetPrefService(kTestAccountId)
      .SetBoolean(ash::prefs::kShowMobileDataNotification, true);

  LogIn(kOtherAccountId);

  EXPECT_FALSE(GetNotification());
}

}  // namespace
