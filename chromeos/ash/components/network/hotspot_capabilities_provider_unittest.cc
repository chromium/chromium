// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_allowed_flag_handler.h"
#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kCellularServicePath[] = "/service/cellular0";
const char kCellularServiceGuid[] = "cellular_guid0";
const char kCellularServiceName[] = "cellular_name0";

}  // namespace

class TestObserver : public HotspotCapabilitiesProvider::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void OnHotspotCapabilitiesChanged() override {
    hotspot_capabilities_changed_count_++;
  }

  size_t hotspot_capabilities_changed_count() {
    return hotspot_capabilities_changed_count_;
  }

 private:
  size_t hotspot_capabilities_changed_count_ = 0u;
};

class HotspotCapabilitiesProviderTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (hotspot_capabilities_provider_ &&
        hotspot_capabilities_provider_->HasObserver(&observer_)) {
      hotspot_capabilities_provider_->RemoveObserver(&observer_);
    }
    hotspot_capabilities_provider_ =
        std::make_unique<HotspotCapabilitiesProvider>();
    hotspot_capabilities_provider_->AddObserver(&observer_);
    hotspot_allowed_flag_handler_ =
        std::make_unique<HotspotAllowedFlagHandler>();
    hotspot_capabilities_provider_->Init(
        network_state_test_helper_.network_state_handler(),
        hotspot_allowed_flag_handler_.get());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    hotspot_capabilities_provider_->RemoveObserver(&observer_);
    hotspot_capabilities_provider_.reset();
    hotspot_allowed_flag_handler_.reset();
  }

  HotspotCapabilitiesProvider::CheckTetheringReadinessResult
  CheckTetheringReadiness() {
    base::RunLoop run_loop;
    HotspotCapabilitiesProvider::CheckTetheringReadinessResult return_result;
    hotspot_capabilities_provider_->CheckTetheringReadiness(
        base::BindLambdaForTesting(
            [&](HotspotCapabilitiesProvider::CheckTetheringReadinessResult
                    result) {
              return_result = result;
              run_loop.Quit();
            }));
    run_loop.Run();
    return return_result;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<HotspotAllowedFlagHandler> hotspot_allowed_flag_handler_;
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
  TestObserver observer_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
};

TEST_F(HotspotCapabilitiesProviderTest, GetHotspotCapabilities) {
  // Notify when first retrieve hotspot capabilities upon login.
  EXPECT_EQ(1u, observer_.hotspot_capabilities_changed_count());

  auto capabilities_dict =
      base::Value::Dict()
          .Set(shill::kTetheringCapUpstreamProperty, base::Value::List())
          .Set(shill::kTetheringCapDownstreamProperty, base::Value::List())
          .Set(shill::kTetheringCapSecurityProperty, base::Value::List());
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringCapabilitiesProperty,
      base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoCellularUpstream,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(2u, observer_.hotspot_capabilities_changed_count());

  capabilities_dict.Set(shill::kTetheringCapUpstreamProperty,
                        base::Value::List().Append(shill::kTypeCellular));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringCapabilitiesProperty,
      base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoWiFiDownstream,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(3u, observer_.hotspot_capabilities_changed_count());

  // Add WiFi to the downstream technology list in Shill
  capabilities_dict.Set(shill::kTetheringCapDownstreamProperty,
                        base::Value::List().Append(shill::kTypeWifi));
  // Add allowed WiFi security mode in Shill
  capabilities_dict.Set(shill::kTetheringCapSecurityProperty,
                        base::Value::List()
                            .Append(shill::kSecurityWpa2)
                            .Append(shill::kSecurityWpa3));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringCapabilitiesProperty,
      base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoMobileData,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(2u, hotspot_capabilities_provider_->GetHotspotCapabilities()
                    .allowed_security_modes.size());
  EXPECT_EQ(4u, observer_.hotspot_capabilities_changed_count());

  // Simulate mobile network has no internet connectivity.
  ShillServiceClient::TestInterface* service_test =
      network_state_test_helper_.service_test();
  service_test->AddService(kCellularServicePath, kCellularServiceGuid,
                           kCellularServiceName, shill::kTypeCellular,
                           shill::kStateNoConnectivity, /*visible=*/true);

  // Add the cellular network is connected and online and simulate check
  // tethering readiness operation fail.
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kFailure,
          /*readiness_status=*/std::string());
  service_test->SetServiceProperty(kCellularServicePath, shill::kStateProperty,
                                   base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedReadinessCheckFail,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(5u, observer_.hotspot_capabilities_changed_count());

  // Disconnect the active cellular network
  service_test->SetServiceProperty(kCellularServicePath, shill::kStateProperty,
                                   base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoMobileData,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(6u, observer_.hotspot_capabilities_changed_count());

  // Simulate check tethering readiness operation success and re-connect the
  // cellular network
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess, shill::kTetheringReadinessReady);
  service_test->SetServiceProperty(kCellularServicePath, shill::kStateProperty,
                                   base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kAllowed,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(7u, observer_.hotspot_capabilities_changed_count());

  hotspot_capabilities_provider_->SetPolicyAllowed(/*allowed=*/false);
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedByPolicy,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(8u, observer_.hotspot_capabilities_changed_count());

  hotspot_capabilities_provider_->SetPolicyAllowed(/*allowed=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kAllowed,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(9u, observer_.hotspot_capabilities_changed_count());
}

TEST_F(HotspotCapabilitiesProviderTest, CheckTetheringReadiness_Ready) {
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess, shill::kTetheringReadinessReady);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CheckTetheringReadiness(),
            HotspotCapabilitiesProvider::CheckTetheringReadinessResult::kReady);
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kAllowed,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::kReady, 1);
}

TEST_F(HotspotCapabilitiesProviderTest, CheckTetheringReadiness_NotAllowed) {
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kTetheringReadinessNotAllowed);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      CheckTetheringReadiness(),
      HotspotCapabilitiesProvider::CheckTetheringReadinessResult::kNotAllowed);
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedReadinessCheckFail,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::kNotAllowed, 1);
}

TEST_F(HotspotCapabilitiesProviderTest,
       CheckTetheringReadiness_NotAllowedByCarrier) {
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kTetheringReadinessNotAllowedByCarrier);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CheckTetheringReadiness(),
            HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
                kNotAllowedByCarrier);
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedReadinessCheckFail,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::
          kNotAllowedByCarrier,
      1);
}

TEST_F(HotspotCapabilitiesProviderTest, Tethering_PolicyNotAllowed) {
  auto capabilities_dict =
      base::Value::Dict()
          .Set(shill::kTetheringCapUpstreamProperty, base::Value::List())
          .Set(shill::kTetheringCapDownstreamProperty, base::Value::List())
          .Set(shill::kTetheringCapSecurityProperty, base::Value::List());
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringCapabilitiesProperty,
      base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  capabilities_dict.Set(shill::kTetheringCapUpstreamProperty,
                        base::Value::List().Append(shill::kTypeCellular));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringCapabilitiesProperty,
      base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  capabilities_dict.Set(shill::kTetheringCapDownstreamProperty,
                        base::Value::List().Append(shill::kTypeWifi));
  capabilities_dict.Set(shill::kTetheringCapSecurityProperty,
                        base::Value::List()
                            .Append(shill::kSecurityWpa2)
                            .Append(shill::kSecurityWpa3));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringCapabilitiesProperty,
      base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  hotspot_capabilities_provider_->SetPolicyAllowed(false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedByPolicy,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
}

TEST_F(HotspotCapabilitiesProviderTest,
       CheckTetheringReadiness_UpstreamNotAvailable) {
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess,
          /*readiness_status=*/shill::
              kTetheringReadinessUpstreamNetworkNotAvailable);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CheckTetheringReadiness(),
            HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
                kUpstreamNetworkNotAvailable);
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoMobileData,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::
          kUpstreamNetworkNotAvailable,
      1);
}

TEST_F(HotspotCapabilitiesProviderTest, CheckTetheringReadiness_EmptyResult) {
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess,
          /*readiness_status=*/std::string());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CheckTetheringReadiness(),
            HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
                kUnknownResult);
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedReadinessCheckFail,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::kUnknownResult,
      1);
}

TEST_F(HotspotCapabilitiesProviderTest, CheckTetheringReadiness_Failure) {
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kFailure,
          /*readiness_status=*/std::string());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CheckTetheringReadiness(),
            HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
                kShillOperationFailed);
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedReadinessCheckFail,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::
          kShillOperationFailed,
      1);
}

}  // namespace ash
