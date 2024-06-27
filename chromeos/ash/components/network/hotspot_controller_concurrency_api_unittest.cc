// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/enterprise_managed_metadata_store.h"
#include "chromeos/ash/components/network/hotspot_allowed_flag_handler.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_controller.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/components/network/metrics/hotspot_feature_usage_metrics.h"
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
const char kHotspotFeatureUsage[] = "ChromeOS.FeatureUsage.Hotspot";

class TestObserver : public HotspotController::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // HotspotStateHandler::Observer:
  void OnHotspotTurnedOn() override { hotspot_turned_on_count_++; }
  void OnHotspotTurnedOff(
      hotspot_config::mojom::DisableReason disable_reason) override {
    last_disable_reason_ = disable_reason;
    hotspot_turned_off_count_++;
  }

  size_t hotspot_turned_on_count() { return hotspot_turned_on_count_; }

  size_t hotspot_turned_off_count() { return hotspot_turned_off_count_; }

  std::optional<hotspot_config::mojom::DisableReason> last_disable_reason() {
    return last_disable_reason_;
  }

 private:
  size_t hotspot_turned_on_count_ = 0u;
  size_t hotspot_turned_off_count_ = 0u;
  std::optional<hotspot_config::mojom::DisableReason> last_disable_reason_ =
      std::nullopt;
};

}  // namespace

class HotspotControllerConcurrencyApiTest : public ::testing::Test {
 public:
  // This struct is used to simplify checking the metrics associated with the
  // tests below.
  struct ExpectedHistogramState {
    size_t success_initial_count = 0u;
    size_t success_retry_count = 0u;
    size_t inhibit_failed_initial_count = 0u;
    size_t inhibit_failed_retry_count = 0u;
    size_t hermes_install_failed_initial_count = 0u;
    size_t hermes_install_failed_retry_count = 0u;
    size_t smds_scan_profile_total_count = 0u;
    size_t smds_scan_profile_sum = 0u;
    size_t no_available_profiles_via_smdp_count = 0u;
    size_t no_available_profiles_via_smds_count = 0u;
    size_t install_method_via_smdp_count = 0u;
    size_t install_method_via_smds_count = 0u;
    size_t scan_duration_other_success_count = 0u;
    size_t scan_duration_other_failure_count = 0u;
    size_t scan_duration_android_success_count = 0u;
    size_t scan_duration_android_failure_count = 0u;
    size_t scan_duration_gsma_success_count = 0u;
    size_t scan_duration_gsma_failure_count = 0u;
  };

  HotspotControllerConcurrencyApiTest() {
    feature_list_.InitAndEnableFeature(features::kWifiConcurrency);
  }
  ~HotspotControllerConcurrencyApiTest() override = default;

  void SetUp() override {
    enterprise_managed_metadata_store_ =
        std::make_unique<EnterpriseManagedMetadataStore>();
    hotspot_capabilities_provider_ =
        std::make_unique<HotspotCapabilitiesProvider>();
    hotspot_allowed_flag_handler_ =
        std::make_unique<HotspotAllowedFlagHandler>();
    hotspot_capabilities_provider_->Init(
        network_state_test_helper_.network_state_handler(),
        hotspot_allowed_flag_handler_.get());
    hotspot_feature_usage_metrics_ =
        std::make_unique<HotspotFeatureUsageMetrics>();
    hotspot_feature_usage_metrics_->Init(
        enterprise_managed_metadata_store_.get(),
        hotspot_capabilities_provider_.get());
    hotspot_state_handler_ = std::make_unique<HotspotStateHandler>();
    hotspot_state_handler_->Init();
    hotspot_controller_ = std::make_unique<HotspotController>();
    hotspot_controller_->Init(
        hotspot_capabilities_provider_.get(),
        hotspot_feature_usage_metrics_.get(), hotspot_state_handler_.get(),
        network_state_test_helper_.technology_state_controller());
    hotspot_controller_->AddObserver(&observer_);
    SetReadinessCheckResultReady();
  }

  void TearDown() override {
    hotspot_controller_->RemoveObserver(&observer_);
    hotspot_controller_.reset();
    hotspot_feature_usage_metrics_.reset();
    hotspot_capabilities_provider_.reset();
    hotspot_allowed_flag_handler_.reset();
    hotspot_state_handler_.reset();
    enterprise_managed_metadata_store_.reset();
  }

  void SetHotspotAllowed() {
    hotspot_capabilities_provider_->SetHotspotAllowStatus(
        hotspot_config::mojom::HotspotAllowStatus::kAllowed);
  }

  void SetHotspotStateInShill(const std::string& state) {
    auto status_dict =
        base::Value::Dict().Set(shill::kTetheringStatusStateProperty, state);
    network_state_test_helper_.manager_test()->SetManagerProperty(
        shill::kTetheringStatusProperty, base::Value(std::move(status_dict)));
    base::RunLoop().RunUntilIdle();
  }

  void SetReadinessCheckResultReady() {
    network_state_test_helper_.manager_test()
        ->SetSimulateCheckTetheringReadinessResult(
            FakeShillSimulatedResult::kSuccess,
            shill::kTetheringReadinessReady);
    base::RunLoop().RunUntilIdle();
  }

  void AddActiveCellularService() {
    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->AddService(kCellularServicePath, kCellularServiceGuid,
                             kCellularServiceName, shill::kTypeCellular,
                             shill::kStateOnline, /*visible=*/true);
  }

  hotspot_config::mojom::HotspotControlResult EnableHotspot() {
    base::RunLoop run_loop;
    hotspot_config::mojom::HotspotControlResult return_result;
    hotspot_controller_->EnableHotspot(base::BindLambdaForTesting(
        [&](hotspot_config::mojom::HotspotControlResult result) {
          return_result = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    FlushMojoCalls();
    return return_result;
  }

  hotspot_config::mojom::HotspotControlResult DisableHotspot() {
    base::RunLoop run_loop;
    hotspot_config::mojom::HotspotControlResult return_result;
    hotspot_controller_->DisableHotspot(
        base::BindLambdaForTesting(
            [&](hotspot_config::mojom::HotspotControlResult result) {
              return_result = result;
              run_loop.Quit();
            }),
        hotspot_config::mojom::DisableReason::kUserInitiated);
    run_loop.Run();
    FlushMojoCalls();
    return return_result;
  }

  bool PrepareEnableWifi() {
    base::RunLoop run_loop;
    bool prepare_success;
    hotspot_controller_->PrepareEnableWifi(
        base::BindLambdaForTesting([&](bool result) {
          prepare_success = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    FlushMojoCalls();
    return prepare_success;
  }

  void SetPolicyAllowHotspot(bool allow_hotspot) {
    base::RunLoop run_loop;
    hotspot_controller_->SetPolicyAllowHotspot(allow_hotspot);
    run_loop.RunUntilIdle();
  }

  void EnableAndAbortHotspot() {
    base::RunLoop run_loop;
    hotspot_config::mojom::HotspotControlResult enable_result =
        hotspot_config::mojom::HotspotControlResult::kUnknownFailure;
    hotspot_config::mojom::HotspotControlResult disable_result =
        hotspot_config::mojom::HotspotControlResult::kUnknownFailure;
    hotspot_controller_->EnableHotspot(base::BindLambdaForTesting(
        [&](hotspot_config::mojom::HotspotControlResult result) {
          enable_result = result;
          run_loop.Quit();
        }));
    hotspot_controller_->DisableHotspot(
        base::BindLambdaForTesting(
            [&](hotspot_config::mojom::HotspotControlResult result) {
              disable_result = result;
              run_loop.Quit();
            }),
        hotspot_config::mojom::DisableReason::kUserInitiated);
    run_loop.Run();
    FlushMojoCalls();

    EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kAborted,
              enable_result);
    EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kAlreadyFulfilled,
              disable_result);
  }

  void FlushMojoCalls() { base::RunLoop().RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<EnterpriseManagedMetadataStore>
      enterprise_managed_metadata_store_;
  std::unique_ptr<HotspotController> hotspot_controller_;
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
  std::unique_ptr<HotspotAllowedFlagHandler> hotspot_allowed_flag_handler_;
  std::unique_ptr<HotspotFeatureUsageMetrics> hotspot_feature_usage_metrics_;
  std::unique_ptr<HotspotStateHandler> hotspot_state_handler_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
  TestObserver observer_;
};

TEST_F(HotspotControllerConcurrencyApiTest, EnableTetheringSuccess) {
  SetHotspotAllowed();
  AddActiveCellularService();
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::kReady, 1);

  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kSuccess,
            EnableHotspot());
  // Verifies that Wifi technology will be turned off.
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotEnableLatency, 1);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 2);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::kReady, 2);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotEnableResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotEnableResultHistogram,
      HotspotMetricsHelper::HotspotMetricsSetEnabledResult::kSuccess, 1);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      1);
}

TEST_F(HotspotControllerConcurrencyApiTest, AbortEnableTethering) {
  SetHotspotAllowed();
  AddActiveCellularService();
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  EnableAndAbortHotspot();

  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotEnableResultHistogram,
      HotspotMetricsHelper::HotspotMetricsSetEnabledResult::kAborted, 1);
}

TEST_F(HotspotControllerConcurrencyApiTest,
       ShillOperationFailureWhileAborting) {
  SetHotspotAllowed();
  AddActiveCellularService();
  base::RunLoop().RunUntilIdle();

  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess,
      shill::kTetheringEnableResultNetworkSetupFailure);
  base::RunLoop().RunUntilIdle();

  EnableAndAbortHotspot();

  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotEnableResultHistogram,
      HotspotMetricsHelper::HotspotMetricsSetEnabledResult::kAborted, 1);
}

TEST_F(HotspotControllerConcurrencyApiTest,
       EnableTetheringReadinessCheckFailure) {
  // Setup the hotspot capabilities so that the initial hotspot allowance
  // status is allowed.
  SetHotspotAllowed();
  AddActiveCellularService();
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::kReady, 1);

  // Simulate check tethering readiness operation fail.
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kFailure,
          /*readiness_status=*/std::string());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kReadinessCheckFailed,
            EnableHotspot());
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedReadinessCheckFail,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);

  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotEnableLatency, 1);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 2);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::
          kShillOperationFailed,
      1);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotEnableResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotEnableResultHistogram,
      HotspotMetricsHelper::HotspotMetricsSetEnabledResult::
          kReadinessCheckFailure,
      1);
}

TEST_F(HotspotControllerConcurrencyApiTest,
       EnableTetheringNetworkSetupFailure) {
  // Setup the hotspot capabilities so that the initial hotspot allowance
  // status is allowed.
  SetHotspotAllowed();
  AddActiveCellularService();
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::kReady, 1);

  // Simulate enable tethering operation fail with
  // kTetheringEnableResultNetworkSetupFailure error.
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess,
      shill::kTetheringEnableResultNetworkSetupFailure);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kNetworkSetupFailure,
            EnableHotspot());
  // Verifies that Wifi technology will still be on if enable hotspot failed.
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotEnableLatency, 1);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 2);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::kReady, 2);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotEnableResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotEnableResultHistogram,
      HotspotMetricsHelper::HotspotMetricsSetEnabledResult::
          kNetworkSetupFailure,
      1);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure),
      1);
}

TEST_F(HotspotControllerConcurrencyApiTest, DisableTetheringSuccess) {
  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kAlreadyFulfilled,
            DisableHotspot());
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotDisableResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotDisableResultHistogram,
      HotspotMetricsHelper::HotspotMetricsSetEnabledResult::kAlreadyFulfilled,
      1);

  SetHotspotStateInShill(shill::kTetheringStateActive);
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kSuccess,
            DisableHotspot());
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotDisableResultHistogram, 2);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotDisableResultHistogram,
      HotspotMetricsHelper::HotspotMetricsSetEnabledResult::kSuccess, 1);
}

TEST_F(HotspotControllerConcurrencyApiTest, SetPolicyAllowHotspot) {
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  SetHotspotStateInShill(shill::kTetheringStateActive);
  SetHotspotAllowed();
  SetPolicyAllowHotspot(/*allow_hotspot=*/false);
  EXPECT_EQ(1u, observer_.hotspot_turned_off_count());
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kProhibitedByPolicy,
            observer_.last_disable_reason());
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedByPolicy,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);

  SetPolicyAllowHotspot(/*allow_hotspot=*/true);
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kAllowed,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
}

TEST_F(HotspotControllerConcurrencyApiTest, RestoreWiFiStatus) {
  SetHotspotAllowed();
  AddActiveCellularService();
  // Verify Wifi is on before turning on hotspot.
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kSuccess,
            EnableHotspot());

  // Verifies that Wifi will be turned off.
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  SetHotspotStateInShill(shill::kTetheringStateIdle);
  base::RunLoop().RunUntilIdle();
  // Verifies that Wifi will be turned back on.
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
}

}  // namespace ash
