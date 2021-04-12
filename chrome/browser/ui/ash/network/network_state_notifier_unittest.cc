// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_state_notifier.h"

#include <memory>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_handler.h"
#include "testing/platform_test.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {
namespace {

const char kWiFi1Guid[] = "wifi1_guid";
const char kCellular1Guid[] = "cellular1_guid";

class NetworkConnectTestDelegate : public NetworkConnect::Delegate {
 public:
  NetworkConnectTestDelegate(
      std::unique_ptr<NetworkStateNotifier> network_state_notifier)
      : network_state_notifier_(std::move(network_state_notifier)) {}
  ~NetworkConnectTestDelegate() override = default;

  // NetworkConnect::Delegate
  void ShowNetworkConfigure(const std::string& network_id) override {}
  void ShowNetworkSettings(const std::string& network_id) override {}
  bool ShowEnrollNetwork(const std::string& network_id) override {
    return false;
  }
  void ShowMobileSetupDialog(const std::string& service_path) override {}
  void ShowCarrierAccountDetail(const std::string& service_path) override {}
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

}  // namespace

class NetworkStateNotifierTest : public BrowserWithTestWindowTest {
 public:
  NetworkStateNotifierTest() = default;
  ~NetworkStateNotifierTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    shill_clients::InitializeFakes();
    SetupDefaultShillState();
    NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();

    auto notifier = std::make_unique<NetworkStateNotifier>();
    notifier->set_system_tray_client(&test_system_tray_client_);

    network_connect_delegate_ =
        std::make_unique<NetworkConnectTestDelegate>(std::move(notifier));

    NetworkConnect::Initialize(network_connect_delegate_.get());
  }

  void TearDown() override {
    NetworkConnect::Shutdown();
    network_connect_delegate_.reset();
    NetworkHandler::Shutdown();
    shill_clients::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  void SetupDefaultShillState() {
    ShillDeviceClient::TestInterface* device_test =
        ShillDeviceClient::Get()->GetTestInterface();
    device_test->ClearDevices();

    ShillServiceClient::TestInterface* service_test =
        ShillServiceClient::Get()->GetTestInterface();
    service_test->ClearServices();

    // Set up Wi-Fi device, and add a single network with a passphrase failure.
    const char kWiFi1ServicePath[] = "/service/wifi1";
    device_test->AddDevice("/device/stub_wifi_device1", shill::kTypeWifi,
                           "stub_wifi_device1");
    service_test->AddService(kWiFi1ServicePath, kWiFi1Guid, "wifi1",
                             shill::kTypeWifi, shill::kStateIdle, true);
    service_test->SetServiceProperty(kWiFi1ServicePath,
                                     shill::kSecurityClassProperty,
                                     base::Value(shill::kSecurityWep));
    service_test->SetServiceProperty(
        kWiFi1ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test->SetServiceProperty(
        kWiFi1ServicePath, shill::kPassphraseProperty, base::Value("failure"));

    // Set up Cellular device, and add a single locked network.
    const char kCellularDevicePath[] = "/device/cellular1";
    const char kCellular1ServicePath[] = "/service/cellular1";
    const char kCellular1Iccid[] = "iccid";
    device_test->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                           "stub_cellular_device1");
    service_test->AddService(kCellular1ServicePath, kCellular1Guid, "cellular1",
                             shill::kTypeCellular, shill::kStateIdle, true);
    service_test->SetServiceProperty(kCellular1ServicePath,
                                     shill::kIccidProperty,
                                     base::Value(kCellular1Iccid));
    service_test->SetServiceProperty(
        kCellular1ServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateActivated));
    base::Value sim_lock_status(base::Value::Type::DICTIONARY);
    sim_lock_status.SetKey(shill::kSIMLockTypeProperty,
                           base::Value(shill::kSIMLockPin));
    device_test->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMLockStatusProperty,
        std::move(sim_lock_status), /*notify_changed=*/true);
    base::Value::ListStorage sim_slot_infos;
    base::Value slot_info_item(base::Value::Type::DICTIONARY);
    slot_info_item.SetKey(shill::kSIMSlotInfoICCID,
                          base::Value(kCellular1Iccid));
    slot_info_item.SetBoolKey(shill::kSIMSlotInfoPrimary, true);
    sim_slot_infos.push_back(std::move(slot_info_item));
    device_test->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMSlotInfoProperty,
        base::Value(sim_slot_infos), /*notify_changed=*/true);

    base::RunLoop().RunUntilIdle();
  }

  ash::TestSystemTrayClient test_system_tray_client_;
  std::unique_ptr<NetworkConnectTestDelegate> network_connect_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateNotifierTest);
};

TEST_F(NetworkStateNotifierTest, WiFiConnectionFailure) {
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  NotificationDisplayServiceTester tester(nullptr /* profile */);
  NetworkConnect::Get()->ConnectToNetworkId(kWiFi1Guid);
  base::RunLoop().RunUntilIdle();
  // Failure should spawn a notification.
  EXPECT_TRUE(tester.GetNotification(
      NetworkStateNotifier::kNetworkConnectNotificationId));
}

TEST_F(NetworkStateNotifierTest, CellularLockedSimConnectionFailure) {
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  NotificationDisplayServiceTester tester(nullptr /* profile */);
  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
  base::RunLoop().RunUntilIdle();

  // Failure should spawn a notification.
  base::Optional<message_center::Notification> notification =
      tester.GetNotification(
          NetworkStateNotifier::kNetworkConnectNotificationId);
  EXPECT_TRUE(notification);

  // Clicking the notification should open SIM unlock settings.
  notification->delegate()->Click(/*button_index=*/base::nullopt,
                                  /*reply=*/base::nullopt);
  EXPECT_EQ(1, test_system_tray_client_.show_sim_unlock_settings_count());
}

}  // namespace chromeos
