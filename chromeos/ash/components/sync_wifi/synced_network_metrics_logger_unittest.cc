// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/synced_network_metrics_logger.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/sync_wifi/network_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::sync_wifi {

class SyncedNetworkMetricsLoggerTest : public testing::Test {
 public:
  SyncedNetworkMetricsLoggerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    network_test_helper_ = std::make_unique<NetworkTestHelper>();
  }

  SyncedNetworkMetricsLoggerTest(const SyncedNetworkMetricsLoggerTest&) =
      delete;
  SyncedNetworkMetricsLoggerTest& operator=(
      const SyncedNetworkMetricsLoggerTest&) = delete;

  ~SyncedNetworkMetricsLoggerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    network_test_helper_->SetUp();
    base::RunLoop().RunUntilIdle();
    InitializeMetricsLogger();
  }

  void InitializeMetricsLogger() {
    synced_network_metrics_logger_ =
        std::make_unique<SyncedNetworkMetricsLogger>(
            network_test_helper_->network_state_test_helper()
                ->network_state_handler(),
            /* network_connection_handler */ nullptr);
  }

  void SimulateConnectionFailure(std::string error) {
    const NetworkState* network = CreateNetwork(/*from_sync=*/true);
    SetNetworkProperty(network->path(), shill::kStateProperty,
                       shill::kStateConfiguration);

    SetNetworkProperty(network->path(), shill::kErrorProperty, error);
    SetNetworkProperty(network->path(), shill::kStateProperty,
                       shill::kStateFailure);
  }

  void SimulateConnectionSuccess(bool include_connecting_state) {
    const NetworkState* network = CreateNetwork(/*from_sync=*/true);

    if (include_connecting_state) {
      SetNetworkProperty(network->path(), shill::kStateProperty,
                         shill::kStateConfiguration);
    }

    SetNetworkProperty(network->path(), shill::kStateProperty,
                       shill::kStateOnline);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  SyncedNetworkMetricsLogger* synced_network_metrics_logger() const {
    return synced_network_metrics_logger_.get();
  }

  NetworkTestHelper* network_test_helper() const {
    return network_test_helper_.get();
  }

  void SetNetworkProperty(const std::string& service_path,
                          const std::string& key,
                          const std::string& value) {
    network_test_helper_->network_state_test_helper()->SetServiceProperty(
        service_path, key, base::Value(value));
    base::RunLoop().RunUntilIdle();
  }

  const NetworkState* CreateNetwork(bool from_sync) {
    std::string guid = network_test_helper()->ConfigureWiFiNetwork(
        "ssid", /*is_secure=*/true, network_test_helper()->primary_user(),
        /*has_connected=*/true,
        /*owned_by_user=*/true, /*configured_by_sync=*/from_sync);
    return network_test_helper()
        ->network_state_helper()
        .network_state_handler()
        ->GetNetworkStateFromGuid(guid);
  }

  // Skips the system clock ahead by 10 seconds.
  void SkipAhead() { task_environment_.FastForwardBy(base::Seconds(10)); }

 private:
  std::unique_ptr<NetworkTestHelper> network_test_helper_;
  std::unique_ptr<SyncedNetworkMetricsLogger> synced_network_metrics_logger_;
};

TEST_F(SyncedNetworkMetricsLoggerTest,
       SuccessfulManualConnection_SyncedNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/true);

  synced_network_metrics_logger()->ConnectSucceeded(network->path());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(kConnectionResultManualHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kConnectionFailureReasonManualHistogram, 0);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       SuccessfulManualConnection_LocallyConfiguredNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/false);

  synced_network_metrics_logger()->ConnectSucceeded(network->path());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(kConnectionResultManualHistogram, 0);
  histogram_tester.ExpectTotalCount(kConnectionFailureReasonManualHistogram, 0);
}

TEST_F(SyncedNetworkMetricsLoggerTest, FailedManualConnection_SyncedNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/true);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateFailure);
  SetNetworkProperty(network->path(), shill::kErrorProperty,
                     shill::kErrorBadPassphrase);
  synced_network_metrics_logger()->ConnectFailed(
      network->path(), NetworkConnectionHandler::kErrorConnectFailed);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(kConnectionResultManualHistogram, false,
                                     1);
  histogram_tester.ExpectBucketCount(kConnectionFailureReasonManualHistogram,
                                     ConnectionFailureReason::kBadPassphrase,
                                     1);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       FailedManualConnection_LocallyConfiguredNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/false);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateFailure);
  SetNetworkProperty(network->path(), shill::kErrorProperty,
                     shill::kErrorBadPassphrase);
  synced_network_metrics_logger()->ConnectFailed(
      network->path(), NetworkConnectionHandler::kErrorConnectFailed);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(kConnectionResultManualHistogram, 0);
  histogram_tester.ExpectTotalCount(kConnectionFailureReasonManualHistogram, 0);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       FailedConnection_SyncedNetwork_UnknownFailure) {
  base::HistogramTester histogram_tester;
  SimulateConnectionFailure(shill::kErrorUnknownFailure);

  histogram_tester.ExpectBucketCount(kConnectionResultAllHistogram, false, 1);
  histogram_tester.ExpectBucketCount(kConnectionFailureReasonAllHistogram,
                                     ConnectionFailureReason::kUnknown, 1);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       SuccessfulConnection_SyncedNetwork_AfterLogin) {
  base::HistogramTester histogram_tester;
  SimulateConnectionSuccess(/*include_connecting_state=*/false);

  histogram_tester.ExpectBucketCount(kConnectionResultAllHistogram, true, 1);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       SuccessfulConnection_SyncedNetwork_DuringLogin) {
  const NetworkState* network = CreateNetwork(/*from_sync=*/true);
  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateOnline);

  base::HistogramTester histogram_tester;
  InitializeMetricsLogger();

  histogram_tester.ExpectBucketCount(kConnectionResultAllHistogram, true, 1);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       SuccessfulConnection_SyncedNetwork_Session) {
  base::HistogramTester histogram_tester;
  SkipAhead();
  SimulateConnectionSuccess(/*include_connecting_state=*/true);

  histogram_tester.ExpectBucketCount(kConnectionResultAllHistogram, true, 1);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       FailedConnection_SyncedNetwork_BadPassphrase) {
  base::HistogramTester histogram_tester;
  SimulateConnectionFailure(shill::kErrorBadPassphrase);

  histogram_tester.ExpectBucketCount(kConnectionResultAllHistogram, false, 1);
  histogram_tester.ExpectBucketCount(kConnectionFailureReasonAllHistogram,
                                     ConnectionFailureReason::kBadPassphrase,
                                     1);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       FailedConnection_SyncedNetwork_OutOfRange) {
  base::HistogramTester histogram_tester;
  SimulateConnectionFailure(shill::kErrorOutOfRange);

  histogram_tester.ExpectTotalCount(kConnectionResultAllHistogram, 0);
  histogram_tester.ExpectBucketCount(kConnectionFailureReasonAllHistogram,
                                     ConnectionFailureReason::kOutOfRange, 1);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       FailedConnection_LocallyConfiguredNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/false);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateConfiguration);
  synced_network_metrics_logger()->NetworkConnectionStateChanged(network);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateFailure);
  SetNetworkProperty(network->path(), shill::kErrorProperty,
                     shill::kErrorBadPassphrase);
  base::RunLoop().RunUntilIdle();
  synced_network_metrics_logger()->NetworkConnectionStateChanged(network);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(kConnectionResultAllHistogram, 0);
  histogram_tester.ExpectTotalCount(kConnectionFailureReasonAllHistogram, 0);
}

TEST_F(SyncedNetworkMetricsLoggerTest, RecordApplyNetworkFailed) {
  base::HistogramTester histogram_tester;
  synced_network_metrics_logger()->RecordApplyNetworkFailed();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(kApplyResultHistogram, false, 1);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       RecordApplyNetworkFailureReason_NoErrorString) {
  base::HistogramTester histogram_tester;
  synced_network_metrics_logger()->RecordApplyNetworkFailureReason(
      ApplyNetworkFailureReason::kTimedout, "");
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(kApplyFailureReasonHistogram,
                                     ApplyNetworkFailureReason::kTimedout, 1);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       RecordApplyNetworkFailureReason_ValidErrorString) {
  base::HistogramTester histogram_tester;
  synced_network_metrics_logger()->RecordApplyNetworkFailureReason(
      ApplyNetworkFailureReason::kTimedout,
      shill::kErrorResultPermissionDenied);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      kApplyFailureReasonHistogram,
      ApplyNetworkFailureReason::kPermissionDenied, 1);
}

TEST_F(SyncedNetworkMetricsLoggerTest, RecordApplyNetworkSuccess) {
  base::HistogramTester histogram_tester;
  synced_network_metrics_logger()->RecordApplyNetworkSuccess();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(kApplyResultHistogram, true, 1);
}

TEST_F(SyncedNetworkMetricsLoggerTest, RecordTotalCount) {
  base::HistogramTester histogram_tester;
  synced_network_metrics_logger()->RecordTotalCount(10);
  base::RunLoop().RunUntilIdle();

  // histogram_tester.ExpectTotalCount(kTotalCountHistogram, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kTotalCountHistogram),
              testing::ElementsAre(base::Bucket(/*min=*/10, /*count=*/1)));
}

TEST_F(SyncedNetworkMetricsLoggerTest, NetworkStatusChange_DuringLogout) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/true);
  NetworkHandler::Get()->ShutdownPrefServices();
  synced_network_metrics_logger()->NetworkConnectionStateChanged(network);
  base::RunLoop().RunUntilIdle();

  // Expect that there is no crash, and no Wi-Fi sync histograms recorded.
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Network.Wifi.Synced"),
              testing::ContainerEq(base::HistogramTester::CountsMap()));
}

TEST_F(SyncedNetworkMetricsLoggerTest, RecordZeroNetworksEligibleForSync) {
  base::HistogramTester histogram_tester;

  base::flat_set<NetworkEligibilityStatus>
      network_eligible_for_sync_status_codes;
  network_eligible_for_sync_status_codes.insert(
      NetworkEligibilityStatus::kNoWifiNetworksAvailable);
  synced_network_metrics_logger()->RecordZeroNetworksEligibleForSync(
      network_eligible_for_sync_status_codes);
  histogram_tester.ExpectBucketCount(
      kZeroNetworksSyncedReasonHistogram,
      NetworkEligibilityStatus::kNoWifiNetworksAvailable, 1);
  histogram_tester.ExpectTotalCount(kZeroNetworksSyncedReasonHistogram, 1);

  network_eligible_for_sync_status_codes.insert(
      NetworkEligibilityStatus::kNotConnectable);
  synced_network_metrics_logger()->RecordZeroNetworksEligibleForSync(
      network_eligible_for_sync_status_codes);
  histogram_tester.ExpectBucketCount(kZeroNetworksSyncedReasonHistogram,
                                     NetworkEligibilityStatus::kNotConnectable,
                                     1);
  histogram_tester.ExpectTotalCount(kZeroNetworksSyncedReasonHistogram, 3);

  network_eligible_for_sync_status_codes.insert(
      NetworkEligibilityStatus::kNetworkIsEligible);
  synced_network_metrics_logger()->RecordZeroNetworksEligibleForSync(
      network_eligible_for_sync_status_codes);
  histogram_tester.ExpectBucketCount(
      kZeroNetworksSyncedReasonHistogram,
      NetworkEligibilityStatus::kNetworkIsEligible, 1);
  histogram_tester.ExpectBucketCount(kZeroNetworksSyncedReasonHistogram,
                                     NetworkEligibilityStatus::kNotConnectable,
                                     1);
  histogram_tester.ExpectBucketCount(
      kZeroNetworksSyncedReasonHistogram,
      NetworkEligibilityStatus::kNoWifiNetworksAvailable, 2);
  histogram_tester.ExpectTotalCount(kZeroNetworksSyncedReasonHistogram, 4);
}

}  // namespace ash::sync_wifi
