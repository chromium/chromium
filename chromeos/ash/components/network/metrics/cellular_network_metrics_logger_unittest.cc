// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/metrics/connection_results.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kCellularGuid[] = "test_guid";
const char kCellularServicePath[] = "/service/network";
const char kCellularName[] = "network_name";

const char kWifiGuid[] = "test_guid2";
const char kWifiServicePath[] = "/service/network2";
const char kWifiName[] = "network_name2";

struct ApnHistogramCounts {
  size_t custom_apns_total_hist_count = 0u;
  size_t enabled_custom_apns_total_hist_count = 0u;
  size_t disabled_custom_apns_total_hist_count = 0u;
  size_t no_enabled_custom_apns = 0u;
  size_t has_enabled_custom_apns = 0u;
};

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

  void AssertHistogramsTotalCount(const ApnHistogramCounts& counts) {
    histogram_tester_->ExpectTotalCount(
        CellularNetworkMetricsLogger::kCustomApnsCountHistogram,
        counts.custom_apns_total_hist_count);
    histogram_tester_->ExpectTotalCount(
        CellularNetworkMetricsLogger::kCustomApnsEnabledCountHistogram,
        counts.enabled_custom_apns_total_hist_count);
    histogram_tester_->ExpectTotalCount(
        CellularNetworkMetricsLogger::kCustomApnsDisabledCountHistogram,
        counts.disabled_custom_apns_total_hist_count);

    histogram_tester_->ExpectTotalCount(
        CellularNetworkMetricsLogger::
            kConnectResultNoEnabledCustomApnsAllHistogram,
        counts.no_enabled_custom_apns);
    histogram_tester_->ExpectTotalCount(
        CellularNetworkMetricsLogger::
            kConnectResultHasEnabledCustomApnsAllHistogram,
        counts.has_enabled_custom_apns);
  }

  void AssertCustomApnsStatusBucketCount(
      ash::ShillConnectResult no_enabled_custom_apns_bucket,
      size_t no_enabled_bucket_count,
      ash::ShillConnectResult has_enabled_custom_apns_bucket,
      size_t has_enabled_bucket_count) {
    histogram_tester_->ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kConnectResultNoEnabledCustomApnsAllHistogram,
        no_enabled_custom_apns_bucket, no_enabled_bucket_count);
    histogram_tester_->ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kConnectResultHasEnabledCustomApnsAllHistogram,
        has_enabled_custom_apns_bucket, has_enabled_bucket_count);
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  raw_ptr<ShillServiceClient::TestInterface, ExperimentalAsh>
      shill_service_client_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(CellularNetworkMetricsLoggerTest, AutoStatusTransitionsRevampEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(ash::features::kApnRevamp);
  SetUpGenericCellularNetwork();
  ApnHistogramCounts counts;

  SetShillState(kCellularServicePath, shill::kStateIdle);
  AssertHistogramsTotalCount(counts);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  counts.custom_apns_total_hist_count++;
  counts.no_enabled_custom_apns++;
  AssertHistogramsTotalCount(counts);
  AssertCustomApnsStatusBucketCount(
      ShillConnectResult::kSuccess, /*no_enabled_bucket_count=*/1,
      ShillConnectResult::kSuccess, /*has_enabled_bucket_count=*/0);
  histogram_tester_->ExpectBucketCount(
      CellularNetworkMetricsLogger::kCustomApnsCountHistogram,
      /*sample=*/0, /*expected_count=*/1);

  // Add an APN to the network.
  auto custom_apn_list = base::Value::List().Append(
      base::Value::Dict()
          .Set(::onc::cellular_apn::kAccessPointName, "apn1")
          .Set(::onc::cellular_apn::kState,
               ::onc::cellular_apn::kStateEnabled));
  NetworkHandler::Get()->network_metadata_store()->SetCustomApnList(
      kCellularGuid, custom_apn_list.Clone());

  SetShillState(kCellularServicePath, shill::kStateAssociation);
  AssertHistogramsTotalCount(counts);
  SetShillState(kCellularServicePath, shill::kStateOnline);

  counts.custom_apns_total_hist_count++;
  counts.has_enabled_custom_apns++;
  counts.enabled_custom_apns_total_hist_count++;
  counts.disabled_custom_apns_total_hist_count++;
  AssertHistogramsTotalCount(counts);
  AssertCustomApnsStatusBucketCount(
      ShillConnectResult::kSuccess, /*no_enabled_bucket_count=*/1,
      ShillConnectResult::kSuccess, /*has_enabled_bucket_count=*/1);
  histogram_tester_->ExpectBucketCount(
      CellularNetworkMetricsLogger::kCustomApnsEnabledCountHistogram,
      /*sample=*/1, /*expected_count=*/1);
  histogram_tester_->ExpectBucketCount(
      CellularNetworkMetricsLogger::kCustomApnsDisabledCountHistogram,
      /*sample=*/0,
      /*expected_count=*/1);

  custom_apn_list.Append(base::Value::Dict()
                             .Set(::onc::cellular_apn::kAccessPointName, "apn2")
                             .Set(::onc::cellular_apn::kState,
                                  ::onc::cellular_apn::kStateDisabled));

  NetworkHandler::Get()->network_metadata_store()->SetCustomApnList(
      kCellularGuid, std::move(custom_apn_list));

  SetShillState(kCellularServicePath, shill::kStateAssociation);
  AssertHistogramsTotalCount(counts);
  SetShillState(kCellularServicePath, shill::kStateOnline);

  counts.custom_apns_total_hist_count++;
  counts.has_enabled_custom_apns++;
  counts.enabled_custom_apns_total_hist_count++;
  counts.disabled_custom_apns_total_hist_count++;
  AssertHistogramsTotalCount(counts);
  AssertCustomApnsStatusBucketCount(
      ShillConnectResult::kSuccess, /*no_enabled_bucket_count=*/1,
      ShillConnectResult::kSuccess, /*has_enabled_bucket_count=*/2);
  histogram_tester_->ExpectBucketCount(
      CellularNetworkMetricsLogger::kCustomApnsEnabledCountHistogram,
      /*sample=*/1, /*expected_count=*/2);
  histogram_tester_->ExpectBucketCount(
      CellularNetworkMetricsLogger::kCustomApnsDisabledCountHistogram,
      /*sample=*/1,
      /*expected_count=*/1);

  // Fail to connect from disconnecting to disconnected.
  SetShillState(kCellularServicePath, shill::kStateAssociation);
  AssertHistogramsTotalCount(counts);
  SetShillState(kCellularServicePath, shill::kStateDisconnect);
  AssertHistogramsTotalCount(counts);
  // Fail to connect from disconnecting to disconnected.
  SetShillError(kCellularServicePath, shill::kErrorConnectFailed);
  AssertHistogramsTotalCount(counts);
  SetShillState(kCellularServicePath, shill::kStateIdle);
  counts.has_enabled_custom_apns++;
  AssertHistogramsTotalCount(counts);
  AssertCustomApnsStatusBucketCount(ShillConnectResult::kSuccess,
                                    /*no_enabled_bucket_count=*/1,
                                    ShillConnectResult::kErrorConnectFailed,
                                    /*has_enabled_bucket_count=*/1);
}

TEST_F(CellularNetworkMetricsLoggerTest, AutoStatusTransitionsRevampDisabled) {
  SetUpGenericCellularNetwork();
  ApnHistogramCounts counts;
  // Successful connect from disconnected to connected.
  SetShillState(kCellularServicePath, shill::kStateIdle);
  AssertHistogramsTotalCount(counts);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  counts.custom_apns_total_hist_count++;
  counts.no_enabled_custom_apns++;
  AssertHistogramsTotalCount(counts);
  AssertCustomApnsStatusBucketCount(
      ShillConnectResult::kSuccess, /*no_enabled_bucket_count=*/1,
      ShillConnectResult::kSuccess, /*has_enabled_bucket_count=*/0);
  histogram_tester_->ExpectBucketCount(
      CellularNetworkMetricsLogger::kCustomApnsCountHistogram, 0, 1);

  // Add an APN to the network.
  auto custom_apn_list = base::Value::List().Append(
      base::Value::Dict()
          .Set(::onc::cellular_apn::kAccessPointName, "apn1")
          .Set(::onc::cellular_apn::kState,
               ::onc::cellular_apn::kStateEnabled));

  NetworkHandler::Get()->network_metadata_store()->SetCustomApnList(
      kCellularGuid, std::move(custom_apn_list));

  // Successful connect from connecting to connected.
  SetShillState(kCellularServicePath, shill::kStateAssociation);
  AssertHistogramsTotalCount(counts);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  counts.custom_apns_total_hist_count++;
  counts.has_enabled_custom_apns++;
  AssertHistogramsTotalCount(counts);

  AssertCustomApnsStatusBucketCount(
      ShillConnectResult::kSuccess, /*no_enabled_bucket_count=*/1,
      ShillConnectResult::kSuccess, /*has_enabled_bucket_count=*/1);
  histogram_tester_->ExpectBucketCount(
      CellularNetworkMetricsLogger::kCustomApnsCountHistogram, 1, 1);

  // Successful connect from connecting to connected again.
  SetShillState(kCellularServicePath, shill::kStateAssociation);
  AssertHistogramsTotalCount(counts);
  SetShillState(kCellularServicePath, shill::kStateOnline);
  counts.custom_apns_total_hist_count++;
  counts.has_enabled_custom_apns++;
  AssertHistogramsTotalCount(counts);

  AssertCustomApnsStatusBucketCount(
      ShillConnectResult::kSuccess, /*no_enabled_bucket_count=*/1,
      ShillConnectResult::kSuccess, /*has_enabled_bucket_count=*/2);
  histogram_tester_->ExpectBucketCount(
      CellularNetworkMetricsLogger::kCustomApnsCountHistogram, 1, 2);

  // Fail to connect from connecting to disconnecting, no valid shill error.
  SetShillState(kCellularServicePath, shill::kStateAssociation);
  AssertHistogramsTotalCount(counts);
  SetShillState(kCellularServicePath, shill::kStateDisconnect);
  AssertHistogramsTotalCount(counts);

  // Fail to connect from disconnecting to disconnected.
  SetShillError(kCellularServicePath, shill::kErrorConnectFailed);
  SetShillState(kCellularServicePath, shill::kStateIdle);
  counts.has_enabled_custom_apns++;
  AssertHistogramsTotalCount(counts);
  AssertCustomApnsStatusBucketCount(ShillConnectResult::kSuccess,
                                    /*no_enabled_bucket_count=*/1,
                                    ShillConnectResult::kErrorConnectFailed,
                                    /*has_enabled_bucket_count=*/1);
}

TEST_F(CellularNetworkMetricsLoggerTest, OnlyCellularNetworksStatusRecorded) {
  SetUpGenericCellularNetwork();
  SetUpGenericWifiNetwork();
  ApnHistogramCounts counts;

  SetShillState(kCellularServicePath, shill::kStateIdle);
  AssertHistogramsTotalCount(counts);

  SetShillState(kCellularServicePath, shill::kStateOnline);
  counts.custom_apns_total_hist_count++;
  counts.no_enabled_custom_apns++;
  AssertHistogramsTotalCount(counts);
  AssertCustomApnsStatusBucketCount(
      ShillConnectResult::kSuccess, /*no_enabled_bucket_count=*/1,
      ShillConnectResult::kSuccess, /*has_enabled_bucket_count=*/0);
  histogram_tester_->ExpectBucketCount(
      CellularNetworkMetricsLogger::kCustomApnsCountHistogram, 0, 1);

  SetShillState(kWifiServicePath, shill::kStateIdle);
  AssertHistogramsTotalCount(counts);

  SetShillState(kWifiServicePath, shill::kStateOnline);
  AssertHistogramsTotalCount(counts);
}

}  // namespace ash
