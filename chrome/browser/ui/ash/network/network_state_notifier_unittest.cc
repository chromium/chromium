// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_state_notifier.h"

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_handler.h"
#include "testing/platform_test.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {
namespace test {

namespace {

const char kWiFi1ServicePath[] = "/service/wifi1";
const char kWiFi1Guid[] = "wifi1_guid";

}  // namespace

class NetworkConnectTestDelegate : public NetworkConnect::Delegate {
 public:
  NetworkConnectTestDelegate()
      : network_state_notifier_(new NetworkStateNotifier()) {}
  ~NetworkConnectTestDelegate() override {}

  // NetworkConnect::Delegate
  void ShowNetworkConfigure(const std::string& network_id) override {}
  void ShowNetworkSettings(const std::string& network_id) override {}
  bool ShowEnrollNetwork(const std::string& network_id) override {
    return false;
  }
  void ShowMobileSetupDialog(const std::string& service_path) override {}
  void ShowNetworkConnectError(const std::string& error_name,
                               const std::string& network_id) override {
    network_state_notifier_->ShowNetworkConnectErrorForGuid(error_name,
                                                            network_id);
  }
  void ShowMobileActivationError(const std::string& network_id) override {}

 private:
  std::unique_ptr<NetworkStateNotifier> network_state_notifier_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectTestDelegate);
};

class NetworkStateNotifierTest : public BrowserWithTestWindowTest {
 public:
  NetworkStateNotifierTest() {}
  ~NetworkStateNotifierTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    LoginState::Initialize();
    shill_clients::InitializeFakes();
    SetupDefaultShillState();
    NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();
    network_connect_delegate_ = std::make_unique<NetworkConnectTestDelegate>();
    NetworkConnect::Initialize(network_connect_delegate_.get());
  }

  void TearDown() override {
    NetworkConnect::Shutdown();
    network_connect_delegate_.reset();
    LoginState::Shutdown();
    NetworkHandler::Shutdown();
    shill_clients::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  void SetupDefaultShillState() {
    base::RunLoop().RunUntilIdle();
    ShillDeviceClient::TestInterface* device_test =
        ShillDeviceClient::Get()->GetTestInterface();
    device_test->ClearDevices();
    device_test->AddDevice("/device/stub_wifi_device1", shill::kTypeWifi,
                           "stub_wifi_device1");
    device_test->AddDevice("/device/stub_cellular_device1",
                           shill::kTypeCellular, "stub_cellular_device1");

    ShillServiceClient::TestInterface* service_test =
        ShillServiceClient::Get()->GetTestInterface();
    service_test->ClearServices();
    const bool add_to_visible = true;
    // Create a wifi network and set to online.
    service_test->AddService(kWiFi1ServicePath, kWiFi1Guid, "wifi1",
                             shill::kTypeWifi, shill::kStateIdle,
                             add_to_visible);
    service_test->SetServiceProperty(kWiFi1ServicePath,
                                     shill::kSecurityClassProperty,
                                     base::Value(shill::kSecurityWep));
    service_test->SetServiceProperty(
        kWiFi1ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test->SetServiceProperty(
        kWiFi1ServicePath, shill::kPassphraseProperty, base::Value("failure"));
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<NetworkConnectTestDelegate> network_connect_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateNotifierTest);
};

TEST_F(NetworkStateNotifierTest, ConnectionFailure) {
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  NotificationDisplayServiceTester tester(nullptr /* profile */);
  NetworkConnect::Get()->ConnectToNetworkId(kWiFi1Guid);
  base::RunLoop().RunUntilIdle();
  // Failure should spawn a notification.
  EXPECT_TRUE(tester.GetNotification(
      NetworkStateNotifier::kNetworkConnectNotificationId));
}

}  // namespace test
}  // namespace chromeos
