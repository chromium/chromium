// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/metrics/network_metrics_helper.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Note: All histogram names should be listed here.

// LogAllConnectionResult() Cellular histograms.
const char kCellularConnectResultAllHistogram[] =
    "Network.Cellular.ConnectionResult.All";
const char kCellularESimConnectResultAllHistogram[] =
    "Network.Cellular.ESim.ConnectionResult.All";
const char kCellularPSimConnectResultAllHistogram[] =
    "Network.Cellular.PSim.ConnectionResult.All";

const char kTestGuid[] = "test_guid";
const char kTestServicePath[] = "/service/network";
const char kTestName[] = "network_name";

}  // namespace

class NetworkMetricsHelperTest : public testing::Test {
 public:
  NetworkMetricsHelperTest() {}

  NetworkMetricsHelperTest(const NetworkMetricsHelperTest&) = delete;
  NetworkMetricsHelperTest& operator=(const NetworkMetricsHelperTest&) = delete;

  ~NetworkMetricsHelperTest() override = default;

  void SetUp() override {
    network_metrics_helper_.reset(new NetworkMetricsHelper());
    network_metrics_helper_->Init(
        network_state_test_helper_.network_state_handler());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    shill_service_client_ = network_state_test_helper_.service_test();
  }

  void TearDown() override {
    network_state_test_helper_.ClearServices();
    network_metrics_helper_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  NetworkStateTestHelper network_state_test_helper_{
      false /* use_default_devices_and_services */};
  ShillServiceClient::TestInterface* shill_service_client_;
  std::unique_ptr<NetworkMetricsHelper> network_metrics_helper_;
};

TEST_F(NetworkMetricsHelperTest, LogAllConnectionResultCellularESim) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeCellular, shill::kStateIdle,
                                    true);
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kEidProperty, base::Value(kTestGuid));
  base::RunLoop().RunUntilIdle();

  network_metrics_helper_->LogAllConnectionResult(kTestGuid,
                                                  shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      0);
}

TEST_F(NetworkMetricsHelperTest, LogAllConnectionResultCellularPSim) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeCellular, shill::kStateIdle,
                                    true);
  base::RunLoop().RunUntilIdle();

  network_metrics_helper_->LogAllConnectionResult(kTestGuid,
                                                  shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      0);
}

}  // namespace chromeos
