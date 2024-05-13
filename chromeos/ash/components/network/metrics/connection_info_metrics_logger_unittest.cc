// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/connection_info_metrics_logger.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/metrics/connection_results.h"
#include "chromeos/ash/components/network/metrics/network_metrics_helper.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// Note: The actual network types used does not matter as networks are kept
// track of by GUID. This test uses Cellular and Wifi, but any combination of
// network type may be used.
const char kCellularConnectResultAllHistogram[] =
    "Network.Ash.Cellular.ConnectionResult.All";
const char kWifiConnectResultAllHistogram[] =
    "Network.Ash.WiFi.ConnectionResult.All";

const char kCellularConnectResultNonUserInitiatedHistogram[] =
    "Network.Ash.Cellular.ConnectionResult.NonUserInitiated";
const char kWifiConnectResultNonUserInitiatedHistogram[] =
    "Network.Ash.WiFi.ConnectionResult.NonUserInitiated";

const char kCellularConnectResultUserInitiatedHistogram[] =
    "Network.Ash.Cellular.ConnectionResult.UserInitiated";
const char kWifiConnectResultUserInitiatedHistogram[] =
    "Network.Ash.WiFi.ConnectionResult.UserInitiated";

const char kCellularConnectionStateHistogram[] =
    "Network.Ash.Cellular.DisconnectionsWithoutUserAction";
const char kWifiConnectionStateHistogram[] =
    "Network.Ash.WiFi.DisconnectionsWithoutUserAction";

const char kCellularGuid[] = "test_guid";
const char kCellularServicePath[] = "/service/network";
const char kCellularName[] = "network_name";

const char kWifiGuid[] = "test_guid2";
const char kWifiServicePath[] = "/service/network2";
const char kWifiName[] = "network_name2";

}  // namespace

class ConnectionInfoMetricsLoggerTest : public testing::Test {
 public:
  ConnectionInfoMetricsLoggerTest() = default;
  ConnectionInfoMetricsLoggerTest(const ConnectionInfoMetricsLoggerTest&) =
      delete;
  ConnectionInfoMetricsLoggerTest& operator=(
      const ConnectionInfoMetricsLoggerTest&) = delete;
  ~ConnectionInfoMetricsLoggerTest() override = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    shill_service_client_ = ShillServiceClient::Get()->GetTestInterface();
    shill_service_client_->ClearServices();
    base::RunLoop().RunUntilIdle();

    network_handler_test_helper_->RegisterPrefs(profile_prefs_.registry(),
                                                local_state_.registry());

    network_handler_test_helper_->InitializePrefs(&profile_prefs_,
                                                  &local_state_);
  }

  void TearDown() override {
    shill_service_client_->ClearServices();
    network_handler_test_helper_.reset();
  }

  void SetUpGenericCellularNetwork() {
    shill_service_client_->AddService(kCellularServicePath, kCellularGuid,
                                      kCellularName, shill::kTypeCellular,
                                      shill::kStateIdle,
                                      /*visible=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetUpGenericWifiNetwork() {
    shill_service_client_->AddService(kWifiServicePath, kWifiGuid, kWifiName,
                                      shill::kTypeWifi, shill::kStateIdle,
                                      /*visible=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetShillState(const std::string& service_path,
                     const std::string& shill_state) {
    shill_service_client_->SetServiceProperty(
        service_path, shill::kStateProperty, base::Value(shill_state));
    base::RunLoop().RunUntilIdle();
  }

  void SetShillError(const std::string& service_path,
                     const std::string& shill_error) {
    shill_service_client_->SetServiceProperty(
        service_path, shill::kErrorProperty, base::Value(shill_error));
    base::RunLoop().RunUntilIdle();
  }

  void TriggerUserInitiatedConnectRequested(const std::string& service_path) {
    NetworkHandler::Get()
        ->connection_info_metrics_logger_->ConnectToNetworkRequested(
            service_path);
  }

  void TriggerUserInitiatedConnectSuccess(const std::string& service_path) {
    NetworkHandler::Get()
        ->connection_info_metrics_logger_->ConnectSucceeded(service_path);
  }

  void TriggerUserInitiatedConnectFailure(const std::string& service_path,
                                          const std::string& error_name) {
    NetworkHandler::Get()
        ->connection_info_metrics_logger_->ConnectFailed(service_path,
                                                         error_name);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  raw_ptr<ShillServiceClient::TestInterface, DanglingUntriaged>
      shill_service_client_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(ConnectionInfoMetricsLoggerTest, ConnectionState) {
  SetUpGenericCellularNetwork();
  SetUpGenericWifiNetwork();

  // Successful Cellular connect from disconnected to connected.
  SetShillState(kCellularServicePath, shill::kStateIdle);
  histogram_tester_->ExpectTotalCount(kCellularConnectionStateHistogram, 0);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  histogram_tester_->ExpectTotalCount(kCellularConnectionStateHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kCellularConnectionStateHistogram,
      NetworkMetricsHelper::ConnectionState::kConnected, 1);

  // Disconnected Cellular with error.
  SetShillError(kCellularServicePath, shill::kErrorConnectFailed);
  SetShillState(kCellularServicePath, shill::kStateIdle);
  histogram_tester_->ExpectTotalCount(kCellularConnectionStateHistogram, 2);
  histogram_tester_->ExpectBucketCount(
      kCellularConnectionStateHistogram,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction, 1);

  // Successful Wifi connect from disconnected to connected.
  SetShillState(kWifiServicePath, shill::kStateIdle);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateHistogram, 0);
  SetShillState(kWifiServicePath, shill::kStateOnline);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kWifiConnectionStateHistogram,
      NetworkMetricsHelper::ConnectionState::kConnected, 1);

  // Disconnecting Wifi due to suspend, state changes to idle with Shill error.
  SetShillError(kWifiServicePath, shill::kErrorConnectFailed);
  SetShillState(kWifiServicePath, shill::kStateIdle);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kWifiConnectionStateHistogram,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction, 0);

  // Disconnecting Wifi due to some system error, state changes to failure with
  // Shill error.
  SetShillState(kWifiServicePath, shill::kStateOnline);
  SetShillError(kWifiServicePath, shill::kErrorConnectFailed);
  SetShillState(kWifiServicePath, shill::kStateFailure);
  histogram_tester_->ExpectBucketCount(
      kWifiConnectionStateHistogram,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction, 1);
}

TEST_F(ConnectionInfoMetricsLoggerTest, UserInitiatedConnectDisconnect) {
  SetUpGenericCellularNetwork();
  SetShillState(kCellularServicePath, shill::kStateIdle);
  SetUpGenericWifiNetwork();
  SetShillState(kWifiServicePath, shill::kStateIdle);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultUserInitiatedHistogram, 0);

  TriggerUserInitiatedConnectRequested(kCellularServicePath);
  SetShillState(kCellularServicePath, shill::kStateAssociation);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  TriggerUserInitiatedConnectSuccess(kCellularServicePath);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultNonUserInitiatedHistogram, 0);

  // Auto connect followed by user-initiated connect.
  SetShillState(kCellularServicePath, shill::kStateIdle);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultNonUserInitiatedHistogram, 1);

  TriggerUserInitiatedConnectRequested(kWifiServicePath);
  TriggerUserInitiatedConnectSuccess(kWifiServicePath);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultUserInitiatedHistogram,
                                      1);

  TriggerUserInitiatedConnectRequested(kCellularServicePath);
  TriggerUserInitiatedConnectFailure(
      kCellularServicePath, NetworkConnectionHandler::kErrorConnectFailed);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultUserInitiatedHistogram, 2);

  TriggerUserInitiatedConnectRequested(kWifiServicePath);
  TriggerUserInitiatedConnectFailure(
      kWifiServicePath, NetworkConnectionHandler::kErrorConnectFailed);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultUserInitiatedHistogram,
                                      2);
  histogram_tester_->ExpectTotalCount(
      kWifiConnectResultNonUserInitiatedHistogram, 0);
}

TEST_F(ConnectionInfoMetricsLoggerTest, AutoStatusTransitions) {
  SetUpGenericCellularNetwork();

  // Successful connect from disconnected to connected.
  SetShillState(kCellularServicePath, shill::kStateIdle);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 0);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectBucketCount(kCellularConnectResultAllHistogram,
                                       ShillConnectResult::kSuccess, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultNonUserInitiatedHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kCellularConnectResultNonUserInitiatedHistogram,
      ShillConnectResult::kSuccess, 1);

  // Successful connect from connecting to connected.
  SetShillState(kCellularServicePath, shill::kStateAssociation);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 2);
  histogram_tester_->ExpectBucketCount(kCellularConnectResultAllHistogram,
                                       ShillConnectResult::kSuccess, 2);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultNonUserInitiatedHistogram, 2);
  histogram_tester_->ExpectBucketCount(
      kCellularConnectResultNonUserInitiatedHistogram,
      ShillConnectResult::kSuccess, 2);

  // Fail to connect from connecting to disconnecting, no valid shill error.
  SetShillState(kCellularServicePath, shill::kStateAssociation);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 2);
  SetShillState(kCellularServicePath, shill::kStateDisconnecting);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 2);

  // Fail to connect from disconnecting to disconnected.
  SetShillError(kCellularServicePath, shill::kErrorConnectFailed);
  SetShillState(kCellularServicePath, shill::kStateIdle);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 3);
  histogram_tester_->ExpectBucketCount(kCellularConnectResultAllHistogram,
                                       ShillConnectResult::kErrorConnectFailed,
                                       1);
}

TEST_F(ConnectionInfoMetricsLoggerTest, MultipleNetworksStatusRecorded) {
  SetUpGenericCellularNetwork();
  SetUpGenericWifiNetwork();

  SetShillState(kCellularServicePath, shill::kStateIdle);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 0);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 0);

  SetShillState(kCellularServicePath, shill::kStateOnline);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 0);
  histogram_tester_->ExpectBucketCount(kCellularConnectResultAllHistogram,
                                       ShillConnectResult::kSuccess, 1);

  SetShillState(kWifiServicePath, shill::kStateOnline);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 1);
  histogram_tester_->ExpectBucketCount(kWifiConnectResultAllHistogram,
                                       ShillConnectResult::kSuccess, 1);

  SetShillState(kWifiServicePath, shill::kStateAssociation);
  SetShillError(kWifiServicePath, shill::kErrorConnectFailed);
  SetShillState(kWifiServicePath, shill::kStateIdle);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 2);
  histogram_tester_->ExpectBucketCount(kWifiConnectResultAllHistogram,
                                       ShillConnectResult::kSuccess, 1);
}

}  // namespace ash
