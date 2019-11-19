// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_connector_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/tether/connect_tethering_operation.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/components/tether/fake_disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/fake_host_scan_cache.h"
#include "chromeos/components/tether/fake_notification_presenter.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/components/tether/fake_wifi_hotspot_connector.h"
#include "chromeos/components/tether/fake_wifi_hotspot_disconnector.h"
#include "chromeos/components/tether/mock_host_connection_metrics_logger.h"
#include "chromeos/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using testing::StrictMock;

namespace chromeos {

namespace tether {

namespace {

const char kSuccessResult[] = "success";

const char kSsid[] = "ssid";
const char kPassword[] = "password";

const char kWifiNetworkGuid[] = "wifiNetworkGuid";

std::string CreateWifiConfigurationJsonString() {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << kWifiNetworkGuid << "\","
     << "  \"Type\": \"" << shill::kTypeWifi << "\","
     << "  \"State\": \"" << shill::kStateIdle << "\""
     << "}";
  return ss.str();
}

class FakeConnectTetheringOperation : public ConnectTetheringOperation {
 public:
  FakeConnectTetheringOperation(
      multidevice::RemoteDeviceRef device_to_connect,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      TetherHostResponseRecorder* tether_host_response_recorder,
      bool setup_required)
      : ConnectTetheringOperation(device_to_connect,
                                  device_sync_client,
                                  secure_channel_client,
                                  tether_host_response_recorder,
                                  setup_required),
        setup_required_(setup_required) {}

  ~FakeConnectTetheringOperation() override = default;

  void NotifyConnectTetheringRequestSent() {
    ConnectTetheringOperation::NotifyConnectTetheringRequestSent();
  }

  void SendSuccessfulResponse(const std::string& ssid,
                              const std::string& password) {
    NotifyObserversOfSuccessfulResponse(ssid, password);
  }

  void SendFailedResponse(
      ConnectTetheringOperation::HostResponseErrorCode error_code) {
    NotifyObserversOfConnectionFailure(error_code);
  }

  multidevice::RemoteDeviceRef GetRemoteDevice() {
    EXPECT_EQ(1u, remote_devices().size());
    return remote_devices()[0];
  }

  bool setup_required() { return setup_required_; }

 private:
  bool setup_required_;
};

class FakeConnectTetheringOperationFactory
    : public ConnectTetheringOperation::Factory {
 public:
  FakeConnectTetheringOperationFactory() = default;
  virtual ~FakeConnectTetheringOperationFactory() = default;

  std::vector<FakeConnectTetheringOperation*>& created_operations() {
    return created_operations_;
  }

 protected:
  // ConnectTetheringOperation::Factory:
  std::unique_ptr<ConnectTetheringOperation> BuildInstance(
      multidevice::RemoteDeviceRef device_to_connect,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      TetherHostResponseRecorder* tether_host_response_recorder,
      bool setup_required) override {
    FakeConnectTetheringOperation* operation =
        new FakeConnectTetheringOperation(
            device_to_connect, device_sync_client, secure_channel_client,
            tether_host_response_recorder, setup_required);
    created_operations_.push_back(operation);
    return base::WrapUnique(operation);
  }

 private:
  std::vector<FakeConnectTetheringOperation*> created_operations_;
};

}  // namespace

class TetherConnectorImplTest : public testing::Test {
 public:
  TetherConnectorImplTest()
      : test_devices_(multidevice::CreateRemoteDeviceRefListForTest(2u)) {}
  ~TetherConnectorImplTest() override = default;

  void SetUp() override {
    helper_.network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TECHNOLOGY_ENABLED);

    fake_operation_factory_ =
        base::WrapUnique(new FakeConnectTetheringOperationFactory());
    ConnectTetheringOperation::Factory::SetInstanceForTesting(
        fake_operation_factory_.get());

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    fake_wifi_hotspot_connector_ = std::make_unique<FakeWifiHotspotConnector>(
        helper_.network_state_handler());
    fake_active_host_ = std::make_unique<FakeActiveHost>();
    fake_tether_host_fetcher_ =
        std::make_unique<FakeTetherHostFetcher>(test_devices_);
    mock_tether_host_response_recorder_ =
        std::make_unique<MockTetherHostResponseRecorder>();
    device_id_tether_network_guid_map_ =
        std::make_unique<DeviceIdTetherNetworkGuidMap>();
    fake_host_scan_cache_ = std::make_unique<FakeHostScanCache>();
    fake_notification_presenter_ =
        std::make_unique<FakeNotificationPresenter>();
    mock_host_connection_metrics_logger_ =
        base::WrapUnique(new StrictMock<MockHostConnectionMetricsLogger>(
            fake_active_host_.get()));
    fake_disconnect_tethering_request_sender_ =
        std::make_unique<FakeDisconnectTetheringRequestSender>();
    fake_wifi_hotspot_disconnector_ =
        std::make_unique<FakeWifiHotspotDisconnector>();

    result_.clear();

    tether_connector_ = base::WrapUnique(new TetherConnectorImpl(
        fake_device_sync_client_.get(), fake_secure_channel_client_.get(),
        helper_.network_state_handler(), fake_wifi_hotspot_connector_.get(),
        fake_active_host_.get(), fake_tether_host_fetcher_.get(),
        mock_tether_host_response_recorder_.get(),
        device_id_tether_network_guid_map_.get(), fake_host_scan_cache_.get(),
        fake_notification_presenter_.get(),
        mock_host_connection_metrics_logger_.get(),
        fake_disconnect_tethering_request_sender_.get(),
        fake_wifi_hotspot_disconnector_.get()));

    SetUpTetherNetworks();
  }

  void TearDown() override {
    // Must delete |fake_wifi_hotspot_connector_| before NetworkStateHandler is
    // destroyed to ensure that NetworkStateHandler has zero observers by the
    // time it reaches its destructor.
    fake_wifi_hotspot_connector_.reset();
  }

  std::string GetTetherNetworkGuid(const std::string& device_id) {
    return device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
        device_id);
  }

  void SetUpTetherNetworks() {
    // Add a tether network corresponding to both of the test devices. These
    // networks are expected to be added already before
    // TetherConnectorImpl::ConnectToNetwork is called.
    AddTetherNetwork(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
                     "TetherNetworkName1", "TetherNetworkCarrier1",
                     85 /* battery_percentage */, 75 /* signal_strength */,
                     true /* has_connected_to_host */,
                     false /* setup_required */);
    AddTetherNetwork(GetTetherNetworkGuid(test_devices_[1].GetDeviceId()),
                     "TetherNetworkName2", "TetherNetworkCarrier2",
                     90 /* battery_percentage */, 50 /* signal_strength */,
                     true /* has_connected_to_host */,
                     true /* setup_required */);
  }

  virtual void AddTetherNetwork(const std::string& tether_network_guid,
                                const std::string& device_name,
                                const std::string& carrier,
                                int battery_percentage,
                                int signal_strength,
                                bool has_connected_to_host,
                                bool setup_required) {
    helper_.network_state_handler()->AddTetherNetworkState(
        tether_network_guid, device_name, carrier, battery_percentage,
        signal_strength, has_connected_to_host);
    fake_host_scan_cache_->SetHostScanResult(
        *HostScanCacheEntry::Builder()
             .SetTetherNetworkGuid(tether_network_guid)
             .SetDeviceName(device_name)
             .SetCarrier(carrier)
             .SetBatteryPercentage(battery_percentage)
             .SetSignalStrength(signal_strength)
             .SetSetupRequired(setup_required)
             .Build());
  }

  void SuccessfullyJoinWifiNetwork() {
    helper_.ConfigureService(CreateWifiConfigurationJsonString());
    fake_wifi_hotspot_connector_->CallMostRecentCallback(kWifiNetworkGuid);
  }

  void SuccessCallback() { result_ = kSuccessResult; }

  void ErrorCallback(const std::string& error_name) { result_ = error_name; }

  void CallConnect(const std::string& tether_network_guid) {
    tether_connector_->ConnectToNetwork(
        tether_network_guid,
        base::Bind(&TetherConnectorImplTest::SuccessCallback,
                   base::Unretained(this)),
        base::Bind(&TetherConnectorImplTest::ErrorCallback,
                   base::Unretained(this)));
  }

  void VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode response_code,
      bool setup_required,
      HostConnectionMetricsLogger::ConnectionToHostResult expected_event_type) {
    EXPECT_CALL(*mock_host_connection_metrics_logger_,
                RecordConnectionToHostResult(
                    expected_event_type,
                    test_devices_[setup_required ? 1 : 0].GetDeviceId()));

    EXPECT_FALSE(
        fake_notification_presenter_->is_setup_required_notification_shown());

    // test_devices_[0] does not require first-time setup, but test_devices_[1]
    // does require first-time setup. See SetUpTetherNetworks().
    multidevice::RemoteDeviceRef test_device =
        test_devices_[setup_required ? 1 : 0];

    CallConnect(GetTetherNetworkGuid(test_device.GetDeviceId()));
    EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
              fake_active_host_->GetActiveHostStatus());
    EXPECT_EQ(test_device.GetDeviceId(),
              fake_active_host_->GetActiveHostDeviceId());
    EXPECT_EQ(GetTetherNetworkGuid(test_device.GetDeviceId()),
              fake_active_host_->GetTetherNetworkGuid());
    EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());
    EXPECT_FALSE(
        fake_notification_presenter_->is_setup_required_notification_shown());
    EXPECT_EQ(
        setup_required,
        fake_operation_factory_->created_operations()[0]->setup_required());

    // Simulate a failed connection attempt (either the host cannot provide
    // tethering at this time or a timeout occurs).
    EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());
    fake_operation_factory_->created_operations()[0]
        ->NotifyConnectTetheringRequestSent();
    fake_operation_factory_->created_operations()[0]->SendFailedResponse(
        response_code);

    EXPECT_FALSE(
        fake_notification_presenter_->is_setup_required_notification_shown());

    // The failure should have resulted in the host being disconnected.
    EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
              fake_active_host_->GetActiveHostStatus());
    EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed,
              GetResultAndReset());
    EXPECT_TRUE(fake_notification_presenter_
                    ->is_connection_failed_notification_shown());
  }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(result_);
    return result;
  }

  const multidevice::RemoteDeviceRefList test_devices_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{true /* use_default_devices_and_services */};

  std::unique_ptr<FakeConnectTetheringOperationFactory> fake_operation_factory_;
  std::unique_ptr<FakeWifiHotspotConnector> fake_wifi_hotspot_connector_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::SecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<MockTetherHostResponseRecorder>
      mock_tether_host_response_recorder_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;
  std::unique_ptr<FakeHostScanCache> fake_host_scan_cache_;
  std::unique_ptr<FakeNotificationPresenter> fake_notification_presenter_;
  std::unique_ptr<StrictMock<MockHostConnectionMetricsLogger>>
      mock_host_connection_metrics_logger_;
  std::unique_ptr<FakeDisconnectTetheringRequestSender>
      fake_disconnect_tethering_request_sender_;
  std::unique_ptr<FakeWifiHotspotDisconnector> fake_wifi_hotspot_disconnector_;

  std::string result_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<TetherConnectorImpl> tether_connector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherConnectorImplTest);
};

TEST_F(TetherConnectorImplTest, DISABLED_TestCannotFetchDevice) {
  // Base64-encoded version of "nonexistentDeviceId".
  const char kNonexistentDeviceId[] = "bm9uZXhpc3RlbnREZXZpY2VJZA==";

  EXPECT_CALL(
      *mock_host_connection_metrics_logger_,
      RecordConnectionToHostResult(
          HostConnectionMetricsLogger::ConnectionToHostResult::
              CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_INTERNAL_ERROR,
          kNonexistentDeviceId));

  CallConnect(GetTetherNetworkGuid(kNonexistentDeviceId));

  // Since an invalid device ID was used, no connection should have been
  // started.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed, GetResultAndReset());
  EXPECT_TRUE(
      fake_notification_presenter_->is_connection_failed_notification_shown());
}

TEST_F(TetherConnectorImplTest, DISABLED_TestCancelWhileOperationActive) {
  EXPECT_CALL(
      *mock_host_connection_metrics_logger_,
      RecordConnectionToHostResult(
          HostConnectionMetricsLogger::ConnectionToHostResult::
              CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_CANCELED_BY_USER,
          test_devices_[0].GetDeviceId()));

  CallConnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());

  // Simulate a failed connection attempt (either the host cannot provide
  // tethering at this time or a timeout occurs).
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_FALSE(
      fake_operation_factory_->created_operations()[0]->setup_required());
  tether_connector_->CancelConnectionAttempt(
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));

  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectCanceled,
            GetResultAndReset());
  EXPECT_EQ(
      std::vector<std::string>{test_devices_[0].GetDeviceId()},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());
  EXPECT_FALSE(
      fake_notification_presenter_->is_connection_failed_notification_shown());
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestConnectTetheringOperationFails_SetupNotRequired) {
  VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode::UNKNOWN_ERROR,
      false /* setup_required */,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR);
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestConnectTetheringOperationFails_SetupRequired) {
  VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode::UNKNOWN_ERROR,
      true /* setup_required */,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR);
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestConnectTetheringOperationFails_ProvisioningFailed) {
  VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode::PROVISIONING_FAILED,
      false /* setup_required */,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_PROVISIONING_FAILED);
}

TEST_F(
    TetherConnectorImplTest,
    DISABLED_TestConnectTetheringOperationFails_TetheringTimeout_SetupNotRequired) {
  VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode::TETHERING_TIMEOUT,
      false /* setup_required */,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_NOT_REQUIRED);
}

TEST_F(
    TetherConnectorImplTest,
    DISABLED_TestConnectTetheringOperationFails_TetheringTimeout_SetupRequired) {
  VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode::TETHERING_TIMEOUT,
      true /* setup_required */,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_REQUIRED);
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestConnectTetheringOperationFails_TetheringUnsupported) {
  VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode::TETHERING_UNSUPPORTED,
      false /* setup_required */,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_TETHERING_UNSUPPORTED);
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestConnectTetheringOperationFails_NoCellData) {
  VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode::NO_CELL_DATA,
      false /* setup_required */,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_NO_CELL_DATA);
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestConnectTetheringOperationFails_EnableHotspotFailed) {
  VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode::ENABLING_HOTSPOT_FAILED,
      false /* setup_required */,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_FAILED);
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestConnectTetheringOperationFails_EnableHotspotTimeout) {
  VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode::
          ENABLING_HOTSPOT_TIMEOUT,
      false /* setup_required */,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_TIMEOUT);
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestConnectTetheringOperationFails_NoResponse) {
  VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode::NO_RESPONSE,
      false /* setup_required */,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_NO_RESPONSE);
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestConnectTetheringOperationFails_InvalidHotspotCredentials) {
  VerifyConnectTetheringOperationFails(
      ConnectTetheringOperation::HostResponseErrorCode::
          INVALID_HOTSPOT_CREDENTIALS,
      false /* setup_required */,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_INVALID_HOTSPOT_CREDENTIALS);
}

TEST_F(TetherConnectorImplTest,
       DISABLED_ConnectionToHostFailedNotificationRemovedWhenConnectionStarts) {
  // Start with the "connection to host failed" notification showing.
  fake_notification_presenter_->NotifyConnectionToHostFailed();

  // Starting a connection should result in it being removed.
  CallConnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_FALSE(
      fake_notification_presenter_->is_connection_failed_notification_shown());
}

TEST_F(TetherConnectorImplTest, DISABLED_TestConnectingToWifiFails) {
  EXPECT_CALL(*mock_host_connection_metrics_logger_,
              RecordConnectionToHostResult(
                  HostConnectionMetricsLogger::ConnectionToHostResult::
                      CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_TIMEOUT,
                  test_devices_[0].GetDeviceId()));

  CallConnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());

  // Receive a successful response. We should still be connecting.
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_FALSE(
      fake_operation_factory_->created_operations()[0]->setup_required());
  fake_operation_factory_->created_operations()[0]
      ->NotifyConnectTetheringRequestSent();
  fake_operation_factory_->created_operations()[0]->SendSuccessfulResponse(
      kSsid, kPassword);
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());

  // |fake_wifi_hotspot_connector_| should have received the SSID and password
  // above. Verify this, then return an empty string, signaling a failure to
  // connect.
  EXPECT_EQ(kSsid, fake_wifi_hotspot_connector_->most_recent_ssid());
  EXPECT_EQ(kPassword, fake_wifi_hotspot_connector_->most_recent_password());
  EXPECT_EQ(fake_active_host_->GetTetherNetworkGuid(),
            fake_wifi_hotspot_connector_->most_recent_tether_network_guid());
  fake_wifi_hotspot_connector_->CallMostRecentCallback("");

  // The failure should have resulted in the host being disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed, GetResultAndReset());
  EXPECT_TRUE(
      fake_notification_presenter_->is_connection_failed_notification_shown());
}

TEST_F(TetherConnectorImplTest, DISABLED_TestCancelWhileConnectingToWifi) {
  EXPECT_CALL(
      *mock_host_connection_metrics_logger_,
      RecordConnectionToHostResult(
          HostConnectionMetricsLogger::ConnectionToHostResult::
              CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_CANCELED_BY_USER,
          test_devices_[0].GetDeviceId()));

  CallConnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());

  // Receive a successful response. We should still be connecting.
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_FALSE(
      fake_operation_factory_->created_operations()[0]->setup_required());
  fake_operation_factory_->created_operations()[0]
      ->NotifyConnectTetheringRequestSent();
  fake_operation_factory_->created_operations()[0]->SendSuccessfulResponse(
      kSsid, kPassword);
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());

  tether_connector_->CancelConnectionAttempt(
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));

  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectCanceled,
            GetResultAndReset());
  EXPECT_EQ(
      std::vector<std::string>{test_devices_[0].GetDeviceId()},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());
  EXPECT_FALSE(
      fake_notification_presenter_->is_connection_failed_notification_shown());

  // Now, simulate the Wi-Fi connection connecting. |tether_connector_| should
  // request that the connection be disconnected.
  SuccessfullyJoinWifiNetwork();
  EXPECT_EQ(
      kWifiNetworkGuid,
      fake_wifi_hotspot_disconnector_->last_disconnected_wifi_network_guid());
}

TEST_F(TetherConnectorImplTest, DISABLED_TestSuccessfulConnection) {
  EXPECT_CALL(*mock_host_connection_metrics_logger_,
              RecordConnectionToHostResult(
                  HostConnectionMetricsLogger::ConnectionToHostResult::
                      CONNECTION_RESULT_SUCCESS,
                  test_devices_[0].GetDeviceId()));

  CallConnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());
  EXPECT_FALSE(
      fake_notification_presenter_->is_setup_required_notification_shown());

  // Receive a successful response. We should still be connecting.
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_FALSE(
      fake_operation_factory_->created_operations()[0]->setup_required());
  fake_operation_factory_->created_operations()[0]
      ->NotifyConnectTetheringRequestSent();
  fake_operation_factory_->created_operations()[0]->SendSuccessfulResponse(
      kSsid, kPassword);
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());

  // |fake_wifi_hotspot_connector_| should have received the SSID and password
  // above. Verify this, then return the GUID corresponding to the connected
  // Wi-Fi network.
  EXPECT_EQ(kSsid, fake_wifi_hotspot_connector_->most_recent_ssid());
  EXPECT_EQ(kPassword, fake_wifi_hotspot_connector_->most_recent_password());
  EXPECT_EQ(fake_active_host_->GetTetherNetworkGuid(),
            fake_wifi_hotspot_connector_->most_recent_tether_network_guid());

  SuccessfullyJoinWifiNetwork();

  // The active host should now be connected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTED,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_EQ(kWifiNetworkGuid, fake_active_host_->GetWifiNetworkGuid());

  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_FALSE(
      fake_notification_presenter_->is_connection_failed_notification_shown());
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestSuccessfulConnection_SetupRequired) {
  EXPECT_CALL(*mock_host_connection_metrics_logger_,
              RecordConnectionToHostResult(
                  HostConnectionMetricsLogger::ConnectionToHostResult::
                      CONNECTION_RESULT_SUCCESS,
                  test_devices_[1].GetDeviceId()));
  EXPECT_FALSE(
      fake_notification_presenter_->is_setup_required_notification_shown());

  CallConnect(GetTetherNetworkGuid(test_devices_[1].GetDeviceId()));
  EXPECT_FALSE(
      fake_notification_presenter_->is_setup_required_notification_shown());
  EXPECT_TRUE(
      fake_operation_factory_->created_operations()[0]->setup_required());

  fake_operation_factory_->created_operations()[0]
      ->NotifyConnectTetheringRequestSent();
  EXPECT_TRUE(
      fake_notification_presenter_->is_setup_required_notification_shown());

  fake_operation_factory_->created_operations()[0]->SendSuccessfulResponse(
      kSsid, kPassword);
  EXPECT_TRUE(
      fake_notification_presenter_->is_setup_required_notification_shown());

  SuccessfullyJoinWifiNetwork();

  EXPECT_FALSE(
      fake_notification_presenter_->is_setup_required_notification_shown());

  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_FALSE(
      fake_notification_presenter_->is_connection_failed_notification_shown());
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestNewConnectionAttemptDuringOperation_DifferentDevice) {
  EXPECT_CALL(
      *mock_host_connection_metrics_logger_,
      RecordConnectionToHostResult(
          HostConnectionMetricsLogger::ConnectionToHostResult::
              CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_CANCELED_BY_USER,
          test_devices_[0].GetDeviceId()));
  EXPECT_CALL(*mock_host_connection_metrics_logger_,
              RecordConnectionToHostResult(
                  HostConnectionMetricsLogger::ConnectionToHostResult::
                      CONNECTION_RESULT_SUCCESS,
                  test_devices_[1].GetDeviceId()));

  CallConnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());

  // An operation should have been created.
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());

  // Before the created operation replies, start a new connection to device 1.
  CallConnect(GetTetherNetworkGuid(test_devices_[1].GetDeviceId()));
  // The first connection attempt should have resulted in a connect canceled
  // error.
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectCanceled,
            GetResultAndReset());
  EXPECT_FALSE(
      fake_notification_presenter_->is_connection_failed_notification_shown());

  // Now, the active host should be the second device.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[1].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[1].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());

  // A second operation should have been created.
  EXPECT_EQ(2u, fake_operation_factory_->created_operations().size());

  // No connection should have been started.
  EXPECT_TRUE(fake_wifi_hotspot_connector_->most_recent_ssid().empty());
  EXPECT_TRUE(fake_wifi_hotspot_connector_->most_recent_password().empty());
  EXPECT_TRUE(
      fake_wifi_hotspot_connector_->most_recent_tether_network_guid().empty());

  // The second operation replies successfully, and this response should
  // result in a Wi-Fi connection attempt.
  fake_operation_factory_->created_operations()[1]
      ->NotifyConnectTetheringRequestSent();
  fake_operation_factory_->created_operations()[1]->SendSuccessfulResponse(
      kSsid, kPassword);
  EXPECT_EQ(kSsid, fake_wifi_hotspot_connector_->most_recent_ssid());
  EXPECT_EQ(kPassword, fake_wifi_hotspot_connector_->most_recent_password());
  EXPECT_EQ(fake_active_host_->GetTetherNetworkGuid(),
            fake_wifi_hotspot_connector_->most_recent_tether_network_guid());

  SuccessfullyJoinWifiNetwork();
}

TEST_F(TetherConnectorImplTest,
       DISABLED_TestNewConnectionAttemptDuringWifiConnection_DifferentDevice) {
  EXPECT_CALL(
      *mock_host_connection_metrics_logger_,
      RecordConnectionToHostResult(
          HostConnectionMetricsLogger::ConnectionToHostResult::
              CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_CANCELED_BY_USER,
          test_devices_[0].GetDeviceId()));
  EXPECT_CALL(*mock_host_connection_metrics_logger_,
              RecordConnectionToHostResult(
                  HostConnectionMetricsLogger::ConnectionToHostResult::
                      CONNECTION_RESULT_SUCCESS,
                  test_devices_[1].GetDeviceId()));

  CallConnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());

  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());
  fake_operation_factory_->created_operations()[0]
      ->NotifyConnectTetheringRequestSent();
  fake_operation_factory_->created_operations()[0]->SendSuccessfulResponse(
      kSsid, kPassword);
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(kSsid, fake_wifi_hotspot_connector_->most_recent_ssid());
  EXPECT_EQ(kPassword, fake_wifi_hotspot_connector_->most_recent_password());
  EXPECT_EQ(fake_active_host_->GetTetherNetworkGuid(),
            fake_wifi_hotspot_connector_->most_recent_tether_network_guid());

  // While the connection to the Wi-Fi network is in progress, start a new
  // connection attempt.
  CallConnect(GetTetherNetworkGuid(test_devices_[1].GetDeviceId()));
  // The first connection attempt should have resulted in a connect canceled
  // error.
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectCanceled,
            GetResultAndReset());
  EXPECT_FALSE(
      fake_notification_presenter_->is_connection_failed_notification_shown());

  // Connect successfully to the first Wi-Fi network. Even though a temporary
  // connection has succeeded, the active host should be CONNECTING to device 1.
  SuccessfullyJoinWifiNetwork();
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[1].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[1].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());

  // The second operation replies successfully, and this response should
  // result in a Wi-Fi connection attempt.
  fake_operation_factory_->created_operations()[1]
      ->NotifyConnectTetheringRequestSent();
  fake_operation_factory_->created_operations()[1]->SendSuccessfulResponse(
      kSsid, kPassword);
  EXPECT_EQ(kSsid, fake_wifi_hotspot_connector_->most_recent_ssid());
  EXPECT_EQ(kPassword, fake_wifi_hotspot_connector_->most_recent_password());
  EXPECT_EQ(fake_active_host_->GetTetherNetworkGuid(),
            fake_wifi_hotspot_connector_->most_recent_tether_network_guid());

  SuccessfullyJoinWifiNetwork();
}

}  // namespace tether

}  // namespace chromeos
