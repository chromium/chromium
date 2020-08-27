// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/synced_network_metrics_logger.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sync_wifi/network_test_helper.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace sync_wifi {

class SyncedNetworkMetricsLoggerTest : public testing::Test {
 public:
  SyncedNetworkMetricsLoggerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    network_test_helper_ = std::make_unique<NetworkTestHelper>();
  }
  ~SyncedNetworkMetricsLoggerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    network_test_helper_->SetUp();
    base::RunLoop().RunUntilIdle();

    synced_network_metrics_logger_.reset(new SyncedNetworkMetricsLogger(
        network_test_helper_->network_state_test_helper()
            ->network_state_handler(),
        /* network_connection_handler */ nullptr));
  }

  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();
    testing::Test::TearDown();
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
  }

  const NetworkState* CreateNetwork(bool from_sync) {
    std::string guid = network_test_helper()->ConfigureWiFiNetwork(
        "ssid", /*is_secure=*/true, /*in_profile=*/true, /*has_connected=*/true,
        /*owned_by_user=*/true, /*configured_by_sync=*/from_sync);
    return network_test_helper()
        ->network_state_helper()
        .network_state_handler()
        ->GetNetworkStateFromGuid(guid);
  }

 private:
  std::unique_ptr<NetworkTestHelper> network_test_helper_;
  std::unique_ptr<SyncedNetworkMetricsLogger> synced_network_metrics_logger_;

  DISALLOW_COPY_AND_ASSIGN(SyncedNetworkMetricsLoggerTest);
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

TEST_F(SyncedNetworkMetricsLoggerTest, FailedConnection_SyncedNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/true);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateConfiguration);
  synced_network_metrics_logger()->NetworkConnectionStateChanged(network);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateFailure);
  SetNetworkProperty(network->path(), shill::kErrorProperty,
                     shill::kErrorUnknownFailure);
  base::RunLoop().RunUntilIdle();

  synced_network_metrics_logger()->NetworkConnectionStateChanged(network);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(kConnectionResultAllHistogram, false, 1);
  histogram_tester.ExpectBucketCount(kConnectionFailureReasonAllHistogram,
                                     ConnectionFailureReason::kUnknown, 1);
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

}  // namespace sync_wifi

}  // namespace chromeos
