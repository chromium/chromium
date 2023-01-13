// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/metrics/connection_results.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kCellularCustomApnsCountHistogram[] =
    "Network.Ash.Cellular.Apn.CustomApns.Count";

const char kCellularConnectResultHasEnabledCustomApnsAllHistogram[] =
    "Network.Ash.Cellular.ConnectionResult.HasEnabledCustomApns.All";
const char kCellularConnectResultNoEnabledCustomApnsAllHistogram[] =
    "Network.Ash.Cellular.ConnectionResult.NoEnabledCustomApns.All";

const char kCellularGuid[] = "test_guid";
const char kCellularServicePath[] = "/service/network";
const char kCellularName[] = "network_name";

const char kWifiGuid[] = "test_guid2";
const char kWifiServicePath[] = "/service/network2";
const char kWifiName[] = "network_name2";

}  // namespace

class CellularNetworkMetricsLoggerTest : public testing::Test {
 protected:
  CellularNetworkMetricsLoggerTest() = default;
  CellularNetworkMetricsLoggerTest(const CellularNetworkMetricsLoggerTest&) =
      delete;
  CellularNetworkMetricsLoggerTest& operator=(
      const CellularNetworkMetricsLoggerTest&) = delete;
  ~CellularNetworkMetricsLoggerTest() override = default;

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

  void AssertHistogramsTotalCount(size_t custom_apns_count,
                                  size_t no_enabled_custom_apns,
                                  size_t has_enabled_custom_apns) {
    histogram_tester_->ExpectTotalCount(kCellularCustomApnsCountHistogram,
                                        custom_apns_count);
    histogram_tester_->ExpectTotalCount(
        kCellularConnectResultNoEnabledCustomApnsAllHistogram,
        no_enabled_custom_apns);
    histogram_tester_->ExpectTotalCount(
        kCellularConnectResultHasEnabledCustomApnsAllHistogram,
        has_enabled_custom_apns);
  }

  void AssertCustomApnsStatusBucketCount(
      ash::ShillConnectResult no_enabled_custom_apns_bucket,
      size_t no_enabled_bucket_count,
      ash::ShillConnectResult has_enabled_custom_apns_bucket,
      size_t has_enabled_bucket_count) {
    histogram_tester_->ExpectBucketCount(
        kCellularConnectResultNoEnabledCustomApnsAllHistogram,
        no_enabled_custom_apns_bucket, no_enabled_bucket_count);
    histogram_tester_->ExpectBucketCount(
        kCellularConnectResultHasEnabledCustomApnsAllHistogram,
        has_enabled_custom_apns_bucket, has_enabled_bucket_count);
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  ShillServiceClient::TestInterface* shill_service_client_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(CellularNetworkMetricsLoggerTest, AutoStatusTransitions) {
  SetUpGenericCellularNetwork();

  // Successful connect from disconnected to connected.
  SetShillState(kCellularServicePath, shill::kStateIdle);
  AssertHistogramsTotalCount(/*custom_apns_count=*/0,
                             /*no_enabled_custom_apns=*/0,
                             /*has_enabled_custom_apns=*/0);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  AssertHistogramsTotalCount(/*custom_apns_count=*/1,
                             /*no_enabled_custom_apns=*/1,
                             /*has_enabled_custom_apns=*/0);
  AssertCustomApnsStatusBucketCount(
      ShillConnectResult::kSuccess, /*no_enabled_bucket_count=*/1,
      ShillConnectResult::kSuccess, /*has_enabled_bucket_count=*/0);
  histogram_tester_->ExpectBucketCount(kCellularCustomApnsCountHistogram, 0, 1);

  // Add an APN to the network.
  base::Value::Dict apn;
  apn.Set(shill::kApnProperty, "apn");
  base::Value::List custom_apn_list;
  custom_apn_list.Append(std::move(apn));
  NetworkHandler::Get()->network_metadata_store()->SetCustomApnList(
      kCellularGuid, std::move(custom_apn_list));

  // Successful connect from connecting to connected.
  SetShillState(kCellularServicePath, shill::kStateAssociation);
  AssertHistogramsTotalCount(/*custom_apns_count=*/1,
                             /*no_enabled_custom_apns=*/1,
                             /*has_enabled_custom_apns=*/0);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  AssertHistogramsTotalCount(/*custom_apns_count=*/2,
                             /*no_enabled_custom_apns=*/1,
                             /*has_enabled_custom_apns=*/1);
  AssertCustomApnsStatusBucketCount(
      ShillConnectResult::kSuccess, /*no_enabled_bucket_count=*/1,
      ShillConnectResult::kSuccess, /*has_enabled_bucket_count=*/1);
  histogram_tester_->ExpectBucketCount(kCellularCustomApnsCountHistogram, 1, 1);

  // Successful connect from connecting to connected again.
  SetShillState(kCellularServicePath, shill::kStateAssociation);
  AssertHistogramsTotalCount(/*custom_apns_count=*/2,
                             /*no_enabled_custom_apns=*/1,
                             /*has_enabled_custom_apns=*/1);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  AssertHistogramsTotalCount(/*custom_apns_count=*/3,
                             /*no_enabled_custom_apns=*/1,
                             /*has_enabled_custom_apns=*/2);
  AssertCustomApnsStatusBucketCount(
      ShillConnectResult::kSuccess, /*no_enabled_bucket_count=*/1,
      ShillConnectResult::kSuccess, /*has_enabled_bucket_count=*/2);
  histogram_tester_->ExpectBucketCount(kCellularCustomApnsCountHistogram, 1, 2);

  // Fail to connect from connecting to disconnecting, no valid shill error.
  SetShillState(kCellularServicePath, shill::kStateAssociation);
  AssertHistogramsTotalCount(/*custom_apns_count=*/3,
                             /*no_enabled_custom_apns=*/1,
                             /*has_enabled_custom_apns=*/2);
  SetShillState(kCellularServicePath, shill::kStateDisconnect);
  AssertHistogramsTotalCount(/*custom_apns_count=*/3,
                             /*no_enabled_custom_apns=*/1,
                             /*has_enabled_custom_apns=*/2);

  // Fail to connect from disconnecting to disconnected.
  SetShillError(kCellularServicePath, shill::kErrorConnectFailed);
  SetShillState(kCellularServicePath, shill::kStateIdle);
  AssertHistogramsTotalCount(/*custom_apns_count=*/3,
                             /*no_enabled_custom_apns=*/1,
                             /*has_enabled_custom_apns=*/3);
  AssertCustomApnsStatusBucketCount(ShillConnectResult::kSuccess,
                                    /*no_enabled_bucket_count=*/1,
                                    ShillConnectResult::kErrorConnectFailed,
                                    /*has_enabled_bucket_count=*/1);
}

TEST_F(CellularNetworkMetricsLoggerTest, OnlyCellularNetworksStatusRecorded) {
  SetUpGenericCellularNetwork();
  SetUpGenericWifiNetwork();

  SetShillState(kCellularServicePath, shill::kStateIdle);
  AssertHistogramsTotalCount(/*custom_apns_count=*/0,
                             /*no_enabled_custom_apns=*/0,
                             /*has_enabled_custom_apns=*/0);

  SetShillState(kCellularServicePath, shill::kStateOnline);
  AssertHistogramsTotalCount(/*custom_apns_count=*/1,
                             /*no_enabled_custom_apns=*/1,
                             /*has_enabled_custom_apns=*/0);
  AssertCustomApnsStatusBucketCount(
      ShillConnectResult::kSuccess, /*no_enabled_bucket_count=*/1,
      ShillConnectResult::kSuccess, /*has_enabled_bucket_count=*/0);
  histogram_tester_->ExpectBucketCount(kCellularCustomApnsCountHistogram, 0, 1);

  SetShillState(kWifiServicePath, shill::kStateIdle);
  AssertHistogramsTotalCount(/*custom_apns_count=*/1,
                             /*no_enabled_custom_apns=*/1,
                             /*has_enabled_custom_apns=*/0);

  SetShillState(kWifiServicePath, shill::kStateOnline);
  AssertHistogramsTotalCount(/*custom_apns_count=*/1,
                             /*no_enabled_custom_apns=*/1,
                             /*has_enabled_custom_apns=*/0);
}

}  // namespace ash
