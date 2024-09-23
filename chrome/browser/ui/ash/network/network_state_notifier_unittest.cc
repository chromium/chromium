// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_state_notifier.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/carrier_lock/carrier_lock_manager.h"
#include "chromeos/ash/components/carrier_lock/common.h"
#include "chromeos/ash/components/carrier_lock/fake_fcm_topic_subscriber.h"
#include "chromeos/ash/components/carrier_lock/fake_provisioning_config_fetcher.h"
#include "chromeos/ash/components/carrier_lock/fake_psm_claim_verifier.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/fake_network_3gpp_handler.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/platform_test.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

const char kWiFi1Guid[] = "wifi1_guid";
const char kCellular1ServicePath[] = "/service/cellular1";
const char kCellular1Guid[] = "cellular1_guid";
const char kCellular1NetworkName[] = "cellular1";
const char16_t kCellular1NetworkName16[] = u"cellular1";
const char kTestEsimProfileName[] = "test_profile_name";
const char16_t kTestEsimProfileName16[] = u"test_profile_name";
const char kCellularEsimServicePath[] = "/service/cellular_esim1";
const char kCellularDevicePath[] = "/device/cellular1";
const char kTestTopic[] = "/topics/test";

class NetworkConnectTestDelegate : public NetworkConnect::Delegate {
 public:
  NetworkConnectTestDelegate(
      std::unique_ptr<NetworkStateNotifier> network_state_notifier)
      : network_state_notifier_(std::move(network_state_notifier)) {}

  NetworkConnectTestDelegate(const NetworkConnectTestDelegate&) = delete;
  NetworkConnectTestDelegate& operator=(const NetworkConnectTestDelegate&) =
      delete;

  ~NetworkConnectTestDelegate() override = default;

  // NetworkConnect::Delegate
  void ShowNetworkConfigure(const std::string& network_id) override {}
  void ShowNetworkSettings(const std::string& network_id) override {}
  bool ShowEnrollNetwork(const std::string& network_id) override {
    return false;
  }
  void ShowMobileSetupDialog(const std::string& service_path) override {}
  void ShowCarrierAccountDetail(const std::string& service_path) override {}
  void ShowCarrierUnlockNotification() override {
    network_state_notifier_->ShowCarrierUnlockNotification();
  }
  void ShowPortalSignin(const std::string& service_path,
                        NetworkConnect::Source source) override {}
  void ShowNetworkConnectError(const std::string& error_name,
                               const std::string& network_id) override {
    network_state_notifier_->ShowNetworkConnectErrorForGuid(error_name,
                                                            network_id);
  }
  void ShowMobileActivationError(const std::string& network_id) override {}

 private:
  std::unique_ptr<NetworkStateNotifier> network_state_notifier_;
};

}  // namespace

class NetworkStateNotifierTest : public BrowserWithTestWindowTest {
 public:
  NetworkStateNotifierTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  NetworkStateNotifierTest(const NetworkStateNotifierTest&) = delete;
  NetworkStateNotifierTest& operator=(const NetworkStateNotifierTest&) = delete;

  ~NetworkStateNotifierTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->RegisterPrefs(user_prefs_.registry(),
                                                local_state_.registry());

    network_handler_test_helper_->InitializePrefs(&user_prefs_, &local_state_);
  }

  void TearDown() override {
    carrier_lock_manager_.reset();
    NetworkConnect::Shutdown();
    network_connect_delegate_.reset();
    network_handler_test_helper_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  void SetupCarrierLock() {
    ash::carrier_lock::CarrierLockManager::RegisterLocalPrefs(
        local_state_.registry());

    fake_modem_handler_ = std::make_unique<FakeNetwork3gppHandler>();
    fake_config_fetcher_ =
        std::make_unique<carrier_lock::FakeProvisioningConfigFetcher>();
    fake_psm_verifier_ = std::make_unique<carrier_lock::FakePsmClaimVerifier>();
    fake_fcm_subscriber_ =
        std::make_unique<carrier_lock::FakeFcmTopicSubscriber>();
  }

  void RunCarrierLock() {
    local_state_.SetBoolean(carrier_lock::kDisableManagerPref, false);
    local_state_.SetString(carrier_lock::kFcmTopicPref, std::string());
    local_state_.SetString(carrier_lock::kLastImeiPref, std::string());
    local_state_.SetInteger(carrier_lock::kErrorCounterPref, 0);

    carrier_lock_manager_ = carrier_lock::CarrierLockManager::CreateForTesting(
        &local_state_, fake_modem_handler_.get(),
        std::move(fake_fcm_subscriber_), std::move(fake_psm_verifier_),
        std::move(fake_config_fetcher_));
  }

  void Init() {
    SetupDefaultShillState();
    base::RunLoop().RunUntilIdle();

    auto notifier = std::make_unique<NetworkStateNotifier>();
    notifier->set_system_tray_client(&test_system_tray_client_);

    network_connect_delegate_ =
        std::make_unique<NetworkConnectTestDelegate>(std::move(notifier));
    NetworkConnect::Initialize(network_connect_delegate_.get());
  }

 protected:
  void SetupESimNetwork() {
    const char kTestEuiccPath[] = "euicc_path";
    const char kTestEidName[] = "eid";
    const char kTestIccid[] = "iccid";

    // Disable stub cellular networks so that eSIM profile and service can both
    // be added at the same time without stub network interfering.
    NetworkHandler::Get()
        ->network_state_handler()
        ->set_stub_cellular_networks_provider(nullptr);
    ShillServiceClient::TestInterface* service_test =
        ShillServiceClient::Get()->GetTestInterface();
    service_test->ClearServices();

    hermes_manager_test_ = HermesManagerClient::Get()->GetTestInterface();
    hermes_euicc_test_ = HermesEuiccClient::Get()->GetTestInterface();

    hermes_manager_test_->AddEuicc(dbus::ObjectPath(kTestEuiccPath),
                                   kTestEidName, /*is_active=*/true,
                                   /*physical_slot=*/0);

    hermes_euicc_test_->AddCarrierProfile(
        dbus::ObjectPath(kCellularEsimServicePath),
        dbus::ObjectPath(kTestEuiccPath), kTestIccid, kTestEsimProfileName,
        kTestEsimProfileName, "service_provider", "activation_code",
        kCellularEsimServicePath, hermes::profile::State::kActive,
        hermes::profile::ProfileClass::kOperational,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
    base::RunLoop().RunUntilIdle();
  }

  void SetCellularDeviceLocked(std::string lock_type) {
    ShillDeviceClient::TestInterface* device_test =
        network_handler_test_helper_->device_test();

    base::Value::Dict sim_lock_status;
    sim_lock_status.Set(shill::kSIMLockTypeProperty, lock_type);
    device_test->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMLockStatusProperty,
        base::Value(std::move(sim_lock_status)), /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetupDefaultShillState() {
    ShillDeviceClient::TestInterface* device_test =
        network_handler_test_helper_->device_test();
    device_test->ClearDevices();

    ShillServiceClient::TestInterface* service_test =
        network_handler_test_helper_->service_test();
    service_test->ClearServices();

    // Set up Wi-Fi device, and add a single network with a passphrase failure.
    const char kWiFi1ServicePath[] = "/service/wifi1";
    device_test->AddDevice("/device/stub_wifi_device1", shill::kTypeWifi,
                           "stub_wifi_device1");
    service_test->AddService(kWiFi1ServicePath, kWiFi1Guid, "wifi1",
                             shill::kTypeWifi, shill::kStateIdle, true);
    service_test->SetServiceProperty(kWiFi1ServicePath,
                                     shill::kSecurityClassProperty,
                                     base::Value(shill::kSecurityClassWep));
    service_test->SetServiceProperty(
        kWiFi1ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test->SetServiceProperty(
        kWiFi1ServicePath, shill::kPassphraseProperty, base::Value("failure"));

    // Set up Cellular device, and add a single locked network.
    const char kCellular1Iccid[] = "iccid";
    device_test->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                           "stub_cellular_device1");
    service_test->AddService(kCellular1ServicePath, kCellular1Guid,
                             kCellular1NetworkName, shill::kTypeCellular,
                             shill::kStateIdle, true);
    service_test->SetServiceProperty(kCellular1ServicePath,
                                     shill::kIccidProperty,
                                     base::Value(kCellular1Iccid));
    service_test->SetServiceProperty(
        kCellular1ServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateActivated));

    base::Value::List sim_slot_infos;
    base::Value::Dict slot_info_item;
    slot_info_item.Set(shill::kSIMSlotInfoICCID, kCellular1Iccid);
    slot_info_item.Set(shill::kSIMSlotInfoPrimary, true);
    sim_slot_infos.Append(std::move(slot_info_item));
    device_test->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMSlotInfoProperty,
        base::Value(std::move(sim_slot_infos)), /*notify_changed=*/true);

    base::RunLoop().RunUntilIdle();
  }

  raw_ptr<HermesManagerClient::TestInterface, DanglingUntriaged>
      hermes_manager_test_;
  raw_ptr<HermesEuiccClient::TestInterface, DanglingUntriaged>
      hermes_euicc_test_;
  TestSystemTrayClient test_system_tray_client_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<NetworkConnectTestDelegate> network_connect_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple user_prefs_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<carrier_lock::CarrierLockManager> carrier_lock_manager_;
  std::unique_ptr<FakeNetwork3gppHandler> fake_modem_handler_;
  std::unique_ptr<carrier_lock::FakeFcmTopicSubscriber> fake_fcm_subscriber_;
  std::unique_ptr<carrier_lock::FakePsmClaimVerifier> fake_psm_verifier_;
  std::unique_ptr<carrier_lock::FakeProvisioningConfigFetcher>
      fake_config_fetcher_;
};

TEST_F(NetworkStateNotifierTest, WiFiConnectionFailure) {
  Init();
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
  Init();
  SetCellularDeviceLocked(shill::kSIMLockPin);
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  NotificationDisplayServiceTester tester(nullptr /* profile */);
  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
  base::RunLoop().RunUntilIdle();

  // Failure should spawn a notification.
  std::optional<message_center::Notification> notification =
      tester.GetNotification(
          NetworkStateNotifier::kNetworkConnectNotificationId);
  EXPECT_TRUE(notification);

  EXPECT_EQ(notification->message(),
            l10n_util::GetStringFUTF16(
                IDS_NETWORK_CONNECTION_ERROR_MESSAGE, kCellular1NetworkName16,
                l10n_util::GetStringUTF16(IDS_NETWORK_LIST_SIM_CARD_LOCKED)));

  // Clicking the notification should open SIM unlock settings.
  notification->delegate()->Click(/*button_index=*/std::nullopt,
                                  /*reply=*/std::nullopt);
  EXPECT_EQ(1, test_system_tray_client_.show_sim_unlock_settings_count());
}

TEST_F(NetworkStateNotifierTest, CellularEsimConnectionFailure) {
  Init();
  SetCellularDeviceLocked(shill::kSIMLockPin);
  SetupESimNetwork();
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  NotificationDisplayServiceTester tester(nullptr /* profile */);
  NetworkConnect::Get()->ConnectToNetworkId("esim_guidiccid");
  base::RunLoop().RunUntilIdle();

  // Failure should spawn a notification.
  std::optional<message_center::Notification> notification =
      tester.GetNotification(
          NetworkStateNotifier::kNetworkConnectNotificationId);
  EXPECT_TRUE(notification);

  EXPECT_EQ(notification->message(),
            l10n_util::GetStringFUTF16(
                IDS_NETWORK_CONNECTION_ERROR_MESSAGE, kTestEsimProfileName16,
                l10n_util::GetStringUTF16(IDS_NETWORK_LIST_SIM_CARD_LOCKED)));

  ShillServiceClient::TestInterface* service_test =
      ShillServiceClient::Get()->GetTestInterface();
  service_test->SetServiceProperty(
      kCellularEsimServicePath, shill::kConnectableProperty, base::Value(true));

  // Set device locked status to false, this will allow for network connection
  // to succeed.
  SetCellularDeviceLocked(/*lock_type=*/"");
  NetworkConnect::Get()->ConnectToNetworkId("esim_guidiccid");
  base::RunLoop().RunUntilIdle();

  // Notification is removed.
  notification = tester.GetNotification(
      NetworkStateNotifier::kNetworkConnectNotificationId);
  EXPECT_FALSE(notification);
}

TEST_F(NetworkStateNotifierTest,
       CellularInvalidApnConnectionFailureApnRevampEnabled) {
  scoped_feature_list_.InitAndEnableFeature(ash::features::kApnRevamp);
  Init();
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  NotificationDisplayServiceTester tester(nullptr /* profile */);
  network_handler_test_helper_->service_test()
      ->SetErrorForNextConnectionAttempt(shill::kErrorInvalidAPN);
  network_handler_test_helper_->service_test()->SetServiceProperty(
      kCellular1ServicePath, shill::kConnectableProperty, base::Value(true));
  network_handler_test_helper_->service_test()->SetServiceProperty(
      kCellular1ServicePath, shill::kErrorProperty,
      base::Value(shill::kErrorInvalidAPN));
  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
  base::RunLoop().RunUntilIdle();

  // Failure should spawn a notification.
  std::optional<message_center::Notification> notification =
      tester.GetNotification(
          NetworkStateNotifier::kNetworkConnectNotificationId);
  EXPECT_TRUE(notification);

  // Clicking the notification should open the APN subpage.
  notification->delegate()->Click(/*button_index=*/std::nullopt,
                                  /*reply=*/std::nullopt);
  EXPECT_EQ(1, test_system_tray_client_.show_apn_subpage_count());
  EXPECT_EQ(kCellular1Guid,
            test_system_tray_client_.last_apn_subpage_network_id());
}

TEST_F(NetworkStateNotifierTest,
       CellularInvalidApnConnectionFailureApnRevampDisabled) {
  scoped_feature_list_.InitAndDisableFeature(features::kApnRevamp);
  Init();
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  NotificationDisplayServiceTester tester(nullptr /* profile */);
  network_handler_test_helper_->service_test()
      ->SetErrorForNextConnectionAttempt(shill::kErrorInvalidAPN);
  network_handler_test_helper_->service_test()->SetServiceProperty(
      kCellular1ServicePath, shill::kConnectableProperty, base::Value(true));
  network_handler_test_helper_->service_test()->SetServiceProperty(
      kCellular1ServicePath, shill::kErrorProperty,
      base::Value(shill::kErrorInvalidAPN));
  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
  base::RunLoop().RunUntilIdle();

  // Failure should spawn a notification.
  std::optional<message_center::Notification> notification =
      tester.GetNotification(
          NetworkStateNotifier::kNetworkConnectNotificationId);
  EXPECT_TRUE(notification);

  // Clicking the notification should open the network settings page.
  notification->delegate()->Click(/*button_index=*/std::nullopt,
                                  /*reply=*/std::nullopt);
  EXPECT_EQ(0, test_system_tray_client_.show_apn_subpage_count());
  EXPECT_EQ(1, test_system_tray_client_.show_network_settings_count());
  EXPECT_EQ(kCellular1Guid,
            test_system_tray_client_.last_network_settings_network_id());
}

TEST_F(NetworkStateNotifierTest, CellularCarrierLockedSimConnectionFailure) {
  Init();
  SetCellularDeviceLocked(shill::kSIMLockNetworkPin);
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  NotificationDisplayServiceTester tester(nullptr /* profile */);
  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
  base::RunLoop().RunUntilIdle();

  // Failure should spawn a notification.
  std::optional<message_center::Notification> notification =
      tester.GetNotification(
          NetworkStateNotifier::kNetworkConnectNotificationId);
  EXPECT_TRUE(notification);
  EXPECT_EQ(
      notification->message(),
      l10n_util::GetStringFUTF16(
          IDS_NETWORK_CONNECTION_ERROR_MESSAGE, kCellular1NetworkName16,
          l10n_util::GetStringUTF16(IDS_NETWORK_LIST_SIM_CARD_CARRIER_LOCKED)));

  // Clicking the notification should open network settings page.
  notification->delegate()->Click(/*button_index=*/std::nullopt,
                                  /*reply=*/std::nullopt);
  EXPECT_EQ(1, test_system_tray_client_.show_network_settings_count());
}

TEST_F(NetworkStateNotifierTest, CellularCarrierUnlockNotification) {
  Init();
  SetupCarrierLock();
  carrier_lock::FakeProvisioningConfigFetcher* config =
      fake_config_fetcher_.get();
  carrier_lock::FakeFcmTopicSubscriber* fcm = fake_fcm_subscriber_.get();
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  NotificationDisplayServiceTester tester(nullptr /* profile */);

  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(
      /*token*/ std::string("Token"),
      /*result*/ carrier_lock::Result::kSuccess);
  fake_psm_verifier_->SetMemberAndResult(
      /*membership*/ true,
      /*result*/ carrier_lock::Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string("LockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_DISALLOW,
      /*fcm topic*/ std::string(kTestTopic),
      /*result*/ carrier_lock::Result::kSuccess);

  // Run Carrier Lock Manager
  RunCarrierLock();
  base::RunLoop().RunUntilIdle();

  // Trigger FCM notification to unlock device
  config->SetConfigTopicAndResult(
      /*configuration*/ std::string("UnlockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_ALLOW,
      /*fcm topic*/ std::string(),
      /*result*/ carrier_lock::Result::kSuccess);
  fcm->SendNotification();
  task_environment()->FastForwardBy(carrier_lock::kConfigDelay);
  base::RunLoop().RunUntilIdle();

  // unlock should spawn a notification.
  std::optional<message_center::Notification> notification =
      tester.GetNotification(
          NetworkStateNotifier::kNetworkCarrierUnlockNotificationId);
  EXPECT_TRUE(notification);
  EXPECT_EQ(notification->message(),
            l10n_util::GetStringUTF16(IDS_NETWORK_CARRIER_UNLOCK_BODY));
  // Clicking the notification should open mobile network sub page.
  notification->delegate()->Click(/*button_index=*/std::nullopt,
                                  /*reply=*/std::nullopt);
  EXPECT_EQ(1, test_system_tray_client_.show_mobile_data_subpage_count());
}
}  // namespace ash
