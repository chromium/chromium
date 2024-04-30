// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_enabled_state_notifier.h"
#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/enterprise_managed_metadata_store.h"
#include "chromeos/ash/components/network/hotspot_allowed_flag_handler.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/components/network/metrics/hotspot_feature_usage_metrics.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/hotspot_enabled_state_test_observer.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kCellularServicePath[] = "/service/cellular0";
const char kCellularServiceGuid[] = "cellular_guid0";
const char kCellularServiceName[] = "cellular_name0";

}  // namespace

class HotspotEnabledStateNotifierTest : public ::testing::Test {
 public:
  void SetUp() override {
    enterprise_managed_metadata_store_ =
        std::make_unique<EnterpriseManagedMetadataStore>();
    hotspot_state_handler_ = std::make_unique<HotspotStateHandler>();
    hotspot_state_handler_->Init();
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
    technology_state_controller_ =
        std::make_unique<TechnologyStateController>();
    technology_state_controller_->Init(
        network_state_test_helper_.network_state_handler());
    hotspot_controller_ = std::make_unique<HotspotController>();
    hotspot_controller_->Init(hotspot_capabilities_provider_.get(),
                              hotspot_feature_usage_metrics_.get(),
                              hotspot_state_handler_.get(),
                              technology_state_controller_.get());
    hotspot_enabled_state_notifier_ =
        std::make_unique<HotspotEnabledStateNotifier>();
    hotspot_enabled_state_notifier_->Init(hotspot_state_handler_.get(),
                                          hotspot_controller_.get());
    SetReadinessCheckResultReady();
  }

  void SetValidTetheringCapabilities() {
    auto capabilities_dict =
        base::Value::Dict()
            .Set(shill::kTetheringCapUpstreamProperty,
                 base::Value::List().Append(shill::kTypeCellular))
            // Add WiFi to the downstream technology list in Shill
            .Set(shill::kTetheringCapDownstreamProperty,
                 base::Value::List().Append(shill::kTypeWifi))
            // Add allowed WiFi security mode in Shill
            .Set(shill::kTetheringCapSecurityProperty,
                 base::Value::List()
                     .Append(shill::kSecurityWpa2)
                     .Append(shill::kSecurityWpa3));
    network_state_test_helper_.manager_test()->SetManagerProperty(
        shill::kTetheringCapabilitiesProperty,
        base::Value(std::move(capabilities_dict)));
    base::RunLoop().RunUntilIdle();
  }

  void AddActiveCellularServivce() {
    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->AddService(kCellularServicePath, kCellularServiceGuid,
                             kCellularServiceName, shill::kTypeCellular,
                             shill::kStateOnline, /*visible=*/true);
  }

  void SetReadinessCheckResultReady() {
    network_state_test_helper_.manager_test()
        ->SetSimulateCheckTetheringReadinessResult(
            FakeShillSimulatedResult::kSuccess,
            shill::kTetheringReadinessReady);
    base::RunLoop().RunUntilIdle();
  }

  void SetHotspotStateInShill(const std::string& state) {
    auto status_dict =
        base::Value::Dict().Set(shill::kTetheringStatusStateProperty, state);
    network_state_test_helper_.manager_test()->SetManagerProperty(
        shill::kTetheringStatusProperty, base::Value(std::move(status_dict)));
    base::RunLoop().RunUntilIdle();
  }

  void SetPolicyAllowHotspot(bool allow_hotspot) {
    base::RunLoop run_loop;
    hotspot_controller_->SetPolicyAllowHotspot(allow_hotspot);
    run_loop.RunUntilIdle();
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

  void SetupObserver() {
    hotspot_enabled_state_observer_ =
        std::make_unique<hotspot_config::HotspotEnabledStateTestObserver>();
    hotspot_enabled_state_notifier_->ObserveEnabledStateChanges(
        hotspot_enabled_state_observer_->GenerateRemote());
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

  void FlushMojoCalls() { base::RunLoop().RunUntilIdle(); }

  hotspot_config::HotspotEnabledStateTestObserver* hotspotStateObserver() {
    return hotspot_enabled_state_observer_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
  std::unique_ptr<EnterpriseManagedMetadataStore>
      enterprise_managed_metadata_store_;
  std::unique_ptr<HotspotStateHandler> hotspot_state_handler_;
  std::unique_ptr<HotspotAllowedFlagHandler> hotspot_allowed_flag_handler_;
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
  std::unique_ptr<HotspotFeatureUsageMetrics> hotspot_feature_usage_metrics_;
  std::unique_ptr<TechnologyStateController> technology_state_controller_;
  std::unique_ptr<HotspotController> hotspot_controller_;
  std::unique_ptr<HotspotEnabledStateNotifier> hotspot_enabled_state_notifier_;
  std::unique_ptr<hotspot_config::HotspotEnabledStateTestObserver>
      hotspot_enabled_state_observer_;
};

TEST_F(HotspotEnabledStateNotifierTest, HotspotTurnedOn) {
  SetupObserver();
  SetValidTetheringCapabilities();
  AddActiveCellularServivce();

  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  EnableHotspot();
  EXPECT_EQ(1u, hotspotStateObserver()->hotspot_turned_on_count());
}

TEST_F(HotspotEnabledStateNotifierTest, HotspotTurnedOff) {
  SetupObserver();
  SetValidTetheringCapabilities();
  AddActiveCellularServivce();

  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  SetHotspotStateInShill(shill::kTetheringStateActive);

  SetPolicyAllowHotspot(/*allow_hotspot=*/false);
  EXPECT_EQ(1u, hotspotStateObserver()->hotspot_turned_off_count());
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kProhibitedByPolicy,
            hotspotStateObserver()->last_disable_reason());
}

TEST_F(HotspotEnabledStateNotifierTest, WifiTurnedOn) {
  SetupObserver();
  SetValidTetheringCapabilities();
  AddActiveCellularServivce();

  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  SetHotspotStateInShill(shill::kTetheringStateActive);

  PrepareEnableWifi();
  EXPECT_EQ(1u, hotspotStateObserver()->hotspot_turned_off_count());
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kWifiEnabled,
            hotspotStateObserver()->last_disable_reason());
}

TEST_F(HotspotEnabledStateNotifierTest, DisabledBySystem) {
  SetupObserver();
  SetValidTetheringCapabilities();
  AddActiveCellularServivce();

  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  base::Value::Dict status_dict;
  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateIdle);

  SetHotspotStateInShill(shill::kTetheringStateActive);

  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonInactive);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, hotspotStateObserver()->hotspot_turned_off_count());
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kAutoDisabled,
            hotspotStateObserver()->last_disable_reason());

  SetHotspotStateInShill(shill::kTetheringStateActive);

  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonError);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kInternalError,
            hotspotStateObserver()->last_disable_reason());

  SetHotspotStateInShill(shill::kTetheringStateActive);

  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonUpstreamDisconnect);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kUpstreamNetworkNotAvailable,
            hotspotStateObserver()->last_disable_reason());

  SetHotspotStateInShill(shill::kTetheringStateActive);

  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonSuspend);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kSuspended,
            hotspotStateObserver()->last_disable_reason());

  SetHotspotStateInShill(shill::kTetheringStateActive);
  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonUpstreamNoInternet);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kUpstreamNoInternet,
            hotspotStateObserver()->last_disable_reason());

  SetHotspotStateInShill(shill::kTetheringStateActive);
  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonDownstreamLinkDisconnect);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kDownstreamLinkDisconnect,
            hotspotStateObserver()->last_disable_reason());

  SetHotspotStateInShill(shill::kTetheringStateActive);
  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonDownstreamNetworkDisconnect);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kDownstreamNetworkDisconnect,
            hotspotStateObserver()->last_disable_reason());

  SetHotspotStateInShill(shill::kTetheringStateActive);
  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonUpstreamNotAvailable);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kUpstreamNotAvailable,
            hotspotStateObserver()->last_disable_reason());

  SetHotspotStateInShill(shill::kTetheringStateActive);
  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonStartTimeout);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kStartTimeout,
            hotspotStateObserver()->last_disable_reason());
}

}  // namespace ash
