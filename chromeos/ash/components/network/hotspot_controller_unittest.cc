// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_controller.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/enterprise_managed_metadata_store.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
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
const char kShillNetworkingFailure[] = "network_failure";
const char kHotspotFeatureUsage[] = "ChromeOS.FeatureUsage.Hotspot";

class TestObserver : public HotspotController::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // HotspotStateHandler::Observer:
  void OnHotspotTurnedOn(bool wifi_turned_off) override {
    hotspot_turned_on_count_++;
  }
  void OnHotspotTurnedOff(
      hotspot_config::mojom::DisableReason disable_reason) override {
    last_disable_reason_ = disable_reason;
    hotspot_turned_off_count_++;
  }

  size_t hotspot_turned_on_count() { return hotspot_turned_on_count_; }

  size_t hotspot_turned_off_count() { return hotspot_turned_off_count_; }

  hotspot_config::mojom::DisableReason last_disable_reason() {
    return last_disable_reason_;
  }

 private:
  size_t hotspot_turned_on_count_ = 0u;
  size_t hotspot_turned_off_count_ = 0u;
  hotspot_config::mojom::DisableReason last_disable_reason_;
};

}  // namespace

class HotspotControllerTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (hotspot_controller_ && hotspot_controller_->HasObserver(&observer_)) {
      hotspot_controller_->RemoveObserver(&observer_);
    }
    enterprise_managed_metadata_store_ =
        std::make_unique<EnterpriseManagedMetadataStore>();
    hotspot_capabilities_provider_ =
        std::make_unique<HotspotCapabilitiesProvider>();
    hotspot_capabilities_provider_->Init(
        network_state_test_helper_.network_state_handler());
    hotspot_feature_usage_metrics_ =
        std::make_unique<HotspotFeatureUsageMetrics>();
    hotspot_feature_usage_metrics_->Init(
        enterprise_managed_metadata_store_.get(),
        hotspot_capabilities_provider_.get());
    technology_state_controller_ =
        std::make_unique<TechnologyStateController>();
    technology_state_controller_->Init(
        network_state_test_helper_.network_state_handler());
    hotspot_state_handler_ = std::make_unique<HotspotStateHandler>();
    hotspot_state_handler_->Init();
    hotspot_controller_ = std::make_unique<HotspotController>();
    hotspot_controller_->Init(hotspot_capabilities_provider_.get(),
                              hotspot_feature_usage_metrics_.get(),
                              hotspot_state_handler_.get(),
                              technology_state_controller_.get());
    hotspot_controller_->AddObserver(&observer_);
    SetReadinessCheckResultReady();
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    hotspot_controller_->RemoveObserver(&observer_);
    hotspot_controller_.reset();
    hotspot_feature_usage_metrics_.reset();
    hotspot_capabilities_provider_.reset();
    hotspot_state_handler_.reset();
    enterprise_managed_metadata_store_.reset();
    technology_state_controller_.reset();
  }

  void SetValidTetheringCapabilities() {
    base::Value::Dict capabilities_dict;
    base::Value::List upstream_list;
    upstream_list.Append(shill::kTypeCellular);
    capabilities_dict.Set(shill::kTetheringCapUpstreamProperty,
                          std::move(upstream_list));
    // Add WiFi to the downstream technology list in Shill
    base::Value::List downstream_list;
    downstream_list.Append(shill::kTypeWifi);
    capabilities_dict.Set(shill::kTetheringCapDownstreamProperty,
                          std::move(downstream_list));
    // Add allowed WiFi security mode in Shill
    base::Value::List security_list;
    security_list.Append(shill::kSecurityWpa2);
    security_list.Append(shill::kSecurityWpa3);
    capabilities_dict.Set(shill::kTetheringCapSecurityProperty,
                          std::move(security_list));
    network_state_test_helper_.manager_test()->SetManagerProperty(
        shill::kTetheringCapabilitiesProperty,
        base::Value(std::move(capabilities_dict)));
    base::RunLoop().RunUntilIdle();
  }

  void SetHotspotStateInShill(const std::string& state) {
    base::Value::Dict status_dict;
    status_dict.Set(shill::kTetheringStatusStateProperty, state);
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

  void AddActiveCellularServivce() {
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

  void EnableAndDisableHotspot(
      hotspot_config::mojom::HotspotControlResult& enable_result,
      hotspot_config::mojom::HotspotControlResult& disable_result) {
    {
      base::RunLoop run_loop;
      hotspot_controller_->EnableHotspot(base::BindLambdaForTesting(
          [&](hotspot_config::mojom::HotspotControlResult result) {
            enable_result = result;
            run_loop.Quit();
          }));
      run_loop.Run();
    }
    SetHotspotStateInShill(shill::kTetheringStateActive);
    {
      base::RunLoop run_loop;
      hotspot_controller_->DisableHotspot(
          base::BindLambdaForTesting(
              [&](hotspot_config::mojom::HotspotControlResult result) {
                disable_result = result;
                run_loop.Quit();
              }),
          hotspot_config::mojom::DisableReason::kUserInitiated);
      run_loop.Run();
    }
  }

  void FlushMojoCalls() { base::RunLoop().RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<EnterpriseManagedMetadataStore>
      enterprise_managed_metadata_store_;
  std::unique_ptr<HotspotController> hotspot_controller_;
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
  std::unique_ptr<HotspotFeatureUsageMetrics> hotspot_feature_usage_metrics_;
  std::unique_ptr<HotspotStateHandler> hotspot_state_handler_;
  std::unique_ptr<TechnologyStateController> technology_state_controller_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
  TestObserver observer_;
};

TEST_F(HotspotControllerTest, EnableTetheringCapabilitiesNotAllowed) {
  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kNotAllowed,
            EnableHotspot());
}

TEST_F(HotspotControllerTest, EnableTetheringSuccess) {
  SetValidTetheringCapabilities();
  AddActiveCellularServivce();
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

TEST_F(HotspotControllerTest, EnableTetheringReadinessCheckFailure) {
  // Setup the hotspot capabilities so that the initial hotspot allowance
  // status is allowed.
  SetValidTetheringCapabilities();
  AddActiveCellularServivce();
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

TEST_F(HotspotControllerTest, EnableTetheringNetworkSetupFailure) {
  // Setup the hotspot capabilities so that the initial hotspot allowance
  // status is allowed.
  SetValidTetheringCapabilities();
  AddActiveCellularServivce();
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram,
      HotspotMetricsHelper::HotspotMetricsCheckReadinessResult::kReady, 1);

  // Simulate enable tethering operation fail with kShillNetworkingFailure
  // error.
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, kShillNetworkingFailure);
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

TEST_F(HotspotControllerTest, DisableTetheringSuccess) {
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

TEST_F(HotspotControllerTest, QueuedRequests) {
  SetValidTetheringCapabilities();
  AddActiveCellularServivce();
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  hotspot_config::mojom::HotspotControlResult enable_result, disable_result;
  EnableAndDisableHotspot(enable_result, disable_result);
  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kSuccess,
            enable_result);
  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kSuccess,
            disable_result);
}

TEST_F(HotspotControllerTest, PrepareEnableWifi) {
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  SetHotspotStateInShill(shill::kTetheringStateActive);
  EXPECT_TRUE(PrepareEnableWifi());
  EXPECT_EQ(1u, observer_.hotspot_turned_off_count());
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kWifiEnabled,
            observer_.last_disable_reason());

  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, kShillNetworkingFailure);
  EXPECT_FALSE(PrepareEnableWifi());
}

TEST_F(HotspotControllerTest, SetPolicyAllowHotspot) {
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  SetHotspotStateInShill(shill::kTetheringStateActive);

  SetPolicyAllowHotspot(/*allow_hotspot=*/false);
  EXPECT_EQ(1u, observer_.hotspot_turned_off_count());
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kProhibitedByPolicy,
            observer_.last_disable_reason());
}

TEST_F(HotspotControllerTest, RestartHotspotIfActive) {
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  hotspot_controller_->RestartHotspotIfActive();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, observer_.hotspot_turned_off_count());
  EXPECT_EQ(0u, observer_.hotspot_turned_on_count());

  SetHotspotStateInShill(shill::kTetheringStateActive);
  SetValidTetheringCapabilities();
  AddActiveCellularServivce();
  base::RunLoop().RunUntilIdle();

  hotspot_controller_->RestartHotspotIfActive();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, observer_.hotspot_turned_off_count());
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kRestart,
            observer_.last_disable_reason());
}

}  // namespace ash
