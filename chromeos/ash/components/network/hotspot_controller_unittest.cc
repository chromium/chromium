// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_controller.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
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

}  // namespace

class HotspotControllerTest : public ::testing::Test {
 public:
  void SetUp() override {
    hotspot_capabilities_provider_ =
        std::make_unique<HotspotCapabilitiesProvider>();
    hotspot_capabilities_provider_->Init(
        network_state_test_helper_.network_state_handler());
    technology_state_controller_ =
        std::make_unique<TechnologyStateController>();
    technology_state_controller_->Init(
        network_state_test_helper_.network_state_handler());
    hotspot_state_handler_ = std::make_unique<HotspotStateHandler>();
    hotspot_state_handler_->Init();
    hotspot_controller_ = std::make_unique<HotspotController>();
    hotspot_controller_->Init(hotspot_capabilities_provider_.get(),
                              hotspot_state_handler_.get(),
                              technology_state_controller_.get());
    SetReadinessCheckResultReady();
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    hotspot_controller_.reset();
    hotspot_capabilities_provider_.reset();
    hotspot_state_handler_.reset();
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
          run_loop.QuitClosure();
        }));
    run_loop.RunUntilIdle();
    return return_result;
  }

  hotspot_config::mojom::HotspotControlResult DisableHotspot() {
    base::RunLoop run_loop;
    hotspot_config::mojom::HotspotControlResult return_result;
    hotspot_controller_->DisableHotspot(base::BindLambdaForTesting(
        [&](hotspot_config::mojom::HotspotControlResult result) {
          return_result = result;
          run_loop.QuitClosure();
        }));
    run_loop.RunUntilIdle();
    return return_result;
  }

  bool PrepareEnableWifi() {
    base::RunLoop run_loop;
    bool prepare_success;
    hotspot_controller_->PrepareEnableWifi(
        base::BindLambdaForTesting([&](bool result) {
          prepare_success = result;
          run_loop.QuitClosure();
        }));
    run_loop.RunUntilIdle();
    return prepare_success;
  }

  void EnableAndDisableHotspot(
      hotspot_config::mojom::HotspotControlResult& enable_result,
      hotspot_config::mojom::HotspotControlResult& disable_result) {
    base::RunLoop run_loop;
    hotspot_controller_->EnableHotspot(base::BindLambdaForTesting(
        [&](hotspot_config::mojom::HotspotControlResult result) {
          enable_result = result;
          run_loop.QuitClosure();
        }));
    hotspot_controller_->DisableHotspot(base::BindLambdaForTesting(
        [&](hotspot_config::mojom::HotspotControlResult result) {
          disable_result = result;
          run_loop.QuitClosure();
        }));
    run_loop.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<HotspotController> hotspot_controller_;
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
  std::unique_ptr<HotspotStateHandler> hotspot_state_handler_;
  std::unique_ptr<TechnologyStateController> technology_state_controller_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
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

  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kSuccess,
            EnableHotspot());
  // Verifies that Wifi technology will be turned off.
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_test_helper_.network_state_handler()->GetTechnologyState(
          NetworkTypePattern::WiFi()));
}

TEST_F(HotspotControllerTest, EnableTetheringReadinessCheckFailure) {
  // Setup the hotspot capabilities so that the initial hotspot allowance
  // status is allowed.
  SetValidTetheringCapabilities();
  AddActiveCellularServivce();
  base::RunLoop().RunUntilIdle();

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
}

TEST_F(HotspotControllerTest, EnableTetheringNetworkSetupFailure) {
  // Setup the hotspot capabilities so that the initial hotspot allowance
  // status is allowed.
  SetValidTetheringCapabilities();
  AddActiveCellularServivce();
  base::RunLoop().RunUntilIdle();

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
}

TEST_F(HotspotControllerTest, DisableTetheringSuccess) {
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kSuccess,
            DisableHotspot());
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
  base::Value::Dict status_dict;
  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateActive);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(PrepareEnableWifi());

  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, kShillNetworkingFailure);
  EXPECT_FALSE(PrepareEnableWifi());
}

}  // namespace ash
