// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/technology_state_controller.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/enterprise_managed_metadata_store.h"
#include "chromeos/ash/components/network/hotspot_allowed_flag_handler.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_controller.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/components/network/metrics/connection_results.h"
#include "chromeos/ash/components/network/metrics/hotspot_feature_usage_metrics.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kEnableWifiResultHistogram[] =
    "Network.Ash.WiFi.EnabledState.Enable.Result";
const char kEnableWifiResultCodeHistogram[] =
    "Network.Ash.WiFi.EnabledState.Enable.ResultCode";

}  // namespace

class TechnologyStateControllerTest : public ::testing::Test {
 public:
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
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    hotspot_controller_.reset();
    hotspot_feature_usage_metrics_.reset();
    hotspot_capabilities_provider_.reset();
    hotspot_allowed_flag_handler_.reset();
    hotspot_state_handler_.reset();
    enterprise_managed_metadata_store_.reset();
    technology_state_controller_.reset();
  }

  // Returns the pair of prepare_success and wifi_turned_off result of the
  // TechnologStateHandler::PrepareEnableHotspot method.
  std::pair<bool, bool> PrepareEnableHotspot() {
    base::RunLoop run_loop;
    bool prepare_success, wifi_turned_off;
    technology_state_controller_->PrepareEnableHotspot(
        base::BindLambdaForTesting([&](bool success, bool wifi_off) {
          prepare_success = success;
          wifi_turned_off = wifi_off;
          run_loop.Quit();
        }));
    run_loop.Run();
    return std::make_pair(prepare_success, wifi_turned_off);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<HotspotController> hotspot_controller_;
  std::unique_ptr<EnterpriseManagedMetadataStore>
      enterprise_managed_metadata_store_;
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
  std::unique_ptr<HotspotAllowedFlagHandler> hotspot_allowed_flag_handler_;
  std::unique_ptr<HotspotFeatureUsageMetrics> hotspot_feature_usage_metrics_;
  std::unique_ptr<HotspotStateHandler> hotspot_state_handler_;
  std::unique_ptr<TechnologyStateController> technology_state_controller_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
};

TEST_F(TechnologyStateControllerTest, ChangeWifiTechnology) {
  // Disable Wifi technology. Will immediately set the state to DISABLING.
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/false,
      network_handler::ErrorCallback());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_DISABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  // Run the message loop. When Shill updates the enabled technologies since
  // the state should transition to AVAILABLE.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  // Enable Wifi technology. Will immediately set the state to ENABLING.
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/true,
      network_handler::ErrorCallback());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  // Run the message loop. State should change to ENABLED.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
}

TEST_F(TechnologyStateControllerTest, ChangePhysicalTechnologies) {
  // Disable Physical technologies which includes WiFi(), Cellular() and
  // Ethernet()
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::Physical(), /*enabled=*/false,
      network_handler::ErrorCallback());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_DISABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_DISABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Cellular()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_DISABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Ethernet()));

  // Run the message loop. When Shill updates the enabled technologies since
  // the state should transition to AVAILABLE.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Cellular()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Ethernet()));

  // Enable Physical technologies. Will immediately set the state to ENABLING.
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::Physical(), /*enabled=*/true,
      network_handler::ErrorCallback());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Cellular()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLING,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Ethernet()));

  // Run the message loop. State should change to ENABLED.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Cellular()));
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::Ethernet()));
}

TEST_F(TechnologyStateControllerTest, EnableWifiWhenHotspotOn) {
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/false,
      network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  // Simulate that there's an active hotspot
  auto status_dict = base::Value::Dict().Set(
      shill::kTetheringStatusStateProperty, shill::kTetheringStateActive);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(std::move(status_dict)));
  base::RunLoop().RunUntilIdle();

  // Simulate disable hotspot will fail.
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, "network_failure");

  std::string error;
  base::RunLoop run_loop;
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/true,
      base::BindLambdaForTesting([&](const std::string& error_name) {
        error = error_name;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(TechnologyStateController::kErrorDisableHotspot, error);
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
  histogram_tester_.ExpectTotalCount(kEnableWifiResultCodeHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      kEnableWifiResultCodeHistogram,
      ShillConnectResult::kErrorDisableHotspotFailed, 1);
  histogram_tester_.ExpectTotalCount(kEnableWifiResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(kEnableWifiResultHistogram, false, 1);

  // Simulate disable hotspot will succeed.
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/true,
      network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
  histogram_tester_.ExpectTotalCount(kEnableWifiResultCodeHistogram, 2);
  histogram_tester_.ExpectBucketCount(kEnableWifiResultCodeHistogram,
                                      ShillConnectResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kEnableWifiResultHistogram, 2);
  histogram_tester_.ExpectBucketCount(kEnableWifiResultHistogram, true, 1);
}

TEST_F(TechnologyStateControllerTest, PrepareEnableHotspot) {
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  std::pair<bool, bool> result = PrepareEnableHotspot();
  // Verifies that |prepare_success| will return true.
  EXPECT_TRUE(result.first);
  // Verifies that |wifi_turned_off| will return true since Wifi was on.
  EXPECT_TRUE(result.second);
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));

  result = PrepareEnableHotspot();
  // Verifies that |prepare_success| will return true.
  EXPECT_TRUE(result.first);
  // Verifies that |wifi_turned_off| will return false since Wifi was off.
  EXPECT_FALSE(result.second);
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
}

}  // namespace ash
