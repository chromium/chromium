// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_state_handler.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kHotspotConfigSSID[] = "hotspot_SSID";
const char kHotspotConfigPassphrase[] = "hotspot_passphrase";
const char kCellularServicePath[] = "/service/cellular0";
const char kCellularServiceGuid[] = "cellular_guid0";
const char kCellularServiceName[] = "cellular_name0";

hotspot_config::mojom::HotspotConfigPtr GenerateTestConfig() {
  auto mojom_config = hotspot_config::mojom::HotspotConfig::New();
  mojom_config->auto_disable = false;
  mojom_config->band = hotspot_config::mojom::WiFiBand::k5GHz;
  mojom_config->security = hotspot_config::mojom::WiFiSecurityMode::kWpa2;
  mojom_config->ssid = kHotspotConfigSSID;
  mojom_config->passphrase = kHotspotConfigPassphrase;
  mojom_config->bssid_randomization = false;
  return mojom_config;
}

}  // namespace

class TestObserver : public HotspotStateHandler::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // HotspotStateHandler::Observer:
  void OnHotspotStatusChanged() override { hotspot_status_changed_count_++; }

  void OnHotspotCapabilitiesChanged() override {
    hotspot_capabilities_changed_count_++;
  }

  size_t hotspot_status_changed_count() {
    return hotspot_status_changed_count_;
  }

  size_t hotspot_capabilities_changed_count() {
    return hotspot_capabilities_changed_count_;
  }

 private:
  size_t hotspot_status_changed_count_ = 0u;
  size_t hotspot_capabilities_changed_count_ = 0u;
};

class HotspotStateHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kHotspot);
    LoginState::Initialize();
    LoginState::Get()->set_always_logged_in(false);

    if (hotspot_state_handler_ &&
        hotspot_state_handler_->HasObserver(&observer_)) {
      hotspot_state_handler_->RemoveObserver(&observer_);
    }
    hotspot_state_handler_ = std::make_unique<HotspotStateHandler>();
    hotspot_state_handler_->AddObserver(&observer_);
    hotspot_state_handler_->Init(
        network_state_test_helper_.network_state_handler());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    hotspot_state_handler_->RemoveObserver(&observer_);
    hotspot_state_handler_.reset();
    LoginState::Shutdown();
  }

  hotspot_config::mojom::SetHotspotConfigResult SetHotspotConfig(
      hotspot_config::mojom::HotspotConfigPtr mojom_config) {
    base::RunLoop run_loop;
    hotspot_config::mojom::SetHotspotConfigResult result;
    hotspot_state_handler_->SetHotspotConfig(
        std::move(mojom_config),
        base::BindLambdaForTesting(
            [&](hotspot_config::mojom::SetHotspotConfigResult success) {
              result = success;
              run_loop.QuitClosure();
            }));
    run_loop.RunUntilIdle();
    return result;
  }

  HotspotStateHandler::CheckTetheringReadinessResult CheckTetheringReadiness() {
    base::RunLoop run_loop;
    HotspotStateHandler::CheckTetheringReadinessResult return_result;
    hotspot_state_handler_->CheckTetheringReadiness(base::BindLambdaForTesting(
        [&](HotspotStateHandler::CheckTetheringReadinessResult result) {
          return_result = result;
          run_loop.QuitClosure();
        }));
    run_loop.RunUntilIdle();
    return return_result;
  }

  void LoginToRegularUser() {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_REGULAR);
    task_environment_.RunUntilIdle();
  }

  void Logout() {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                        LoginState::LOGGED_IN_USER_NONE);
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<HotspotStateHandler> hotspot_state_handler_;
  TestObserver observer_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
};

TEST_F(HotspotStateHandlerTest, GetHotspotState) {
  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            hotspot_config::mojom::HotspotState::kDisabled);

  // Update tethering status to active in Shill.
  base::Value status_dict(base::Value::Type::DICTIONARY);
  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateActive));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            hotspot_config::mojom::HotspotState::kEnabled);
  EXPECT_EQ(1u, observer_.hotspot_status_changed_count());

  // Update tethering status to idle in Shill.
  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateIdle));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            hotspot_config::mojom::HotspotState::kDisabled);
  EXPECT_EQ(2u, observer_.hotspot_status_changed_count());

  // Simulate user starting tethering.
  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateStarting));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            hotspot_config::mojom::HotspotState::kEnabling);
  EXPECT_EQ(3u, observer_.hotspot_status_changed_count());
}

TEST_F(HotspotStateHandlerTest, GetHotspotActiveClientCount) {
  EXPECT_EQ(0u, hotspot_state_handler_->GetHotspotActiveClientCount());

  base::Value status_dict(base::Value::Type::DICTIONARY);
  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateActive));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, hotspot_state_handler_->GetHotspotActiveClientCount());
  EXPECT_EQ(1u, observer_.hotspot_status_changed_count());

  // Update tethering status with one active client.
  base::Value active_clients_list(base::Value::Type::LIST);
  base::Value client(base::Value::Type::DICTIONARY);
  client.GetDict().Set(shill::kTetheringStatusClientIPv4Property,
                       base::Value("IPV4:001"));
  client.GetDict().Set(shill::kTetheringStatusClientHostnameProperty,
                       base::Value("hostname1"));
  client.GetDict().Set(shill::kTetheringStatusClientMACProperty,
                       base::Value("persist"));
  active_clients_list.Append(std::move(client));
  status_dict.GetDict().Set(shill::kTetheringStatusClientsProperty,
                            std::move(active_clients_list));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, hotspot_state_handler_->GetHotspotActiveClientCount());
  EXPECT_EQ(2u, observer_.hotspot_status_changed_count());

  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateIdle));
  status_dict.GetDict().Remove(shill::kTetheringStatusClientsProperty);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, hotspot_state_handler_->GetHotspotActiveClientCount());
  EXPECT_EQ(3u, observer_.hotspot_status_changed_count());
}

TEST_F(HotspotStateHandlerTest, GetHotspotCapabilities) {
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoCellularUpstream,
      hotspot_state_handler_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(0u, observer_.hotspot_capabilities_changed_count());

  base::Value capabilities_dict(base::Value::Type::DICTIONARY);
  capabilities_dict.GetDict().Set(shill::kTetheringCapUpstreamProperty,
                                  base::Value(base::Value::Type::LIST));
  capabilities_dict.GetDict().Set(shill::kTetheringCapDownstreamProperty,
                                  base::Value(base::Value::Type::LIST));
  capabilities_dict.GetDict().Set(shill::kTetheringCapSecurityProperty,
                                  base::Value(base::Value::Type::LIST));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringCapabilitiesProperty, capabilities_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoCellularUpstream,
      hotspot_state_handler_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(0u, observer_.hotspot_capabilities_changed_count());

  base::Value upstream_list(base::Value::Type::LIST);
  upstream_list.Append(base::Value(shill::kTypeCellular));
  capabilities_dict.GetDict().Set(shill::kTetheringCapUpstreamProperty,
                                  std::move(upstream_list));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringCapabilitiesProperty, capabilities_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoWiFiDownstream,
      hotspot_state_handler_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(1u, observer_.hotspot_capabilities_changed_count());

  // Add WiFi to the downstream technology list in Shill
  base::Value downstream_list(base::Value::Type::LIST);
  downstream_list.Append(base::Value(shill::kTypeWifi));
  capabilities_dict.GetDict().Set(shill::kTetheringCapDownstreamProperty,
                                  std::move(downstream_list));
  // Add allowed WiFi security mode in Shill
  base::Value security_list(base::Value::Type::LIST);
  security_list.Append(base::Value(shill::kSecurityWpa2));
  security_list.Append(base::Value(shill::kSecurityWpa3));
  capabilities_dict.GetDict().Set(shill::kTetheringCapSecurityProperty,
                                  std::move(security_list));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringCapabilitiesProperty, capabilities_dict);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoMobileData,
            hotspot_state_handler_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(2u, hotspot_state_handler_->GetHotspotCapabilities()
                    .allowed_security_modes.size());
  EXPECT_EQ(2u, observer_.hotspot_capabilities_changed_count());

  // Add an active cellular network and simulate check tethering readiness
  // operation fail.
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kFailure,
          /*readiness_status=*/std::string());
  ShillServiceClient::TestInterface* service_test =
      network_state_test_helper_.service_test();
  service_test->AddService(kCellularServicePath, kCellularServiceGuid,
                           kCellularServiceName, shill::kTypeCellular,
                           shill::kStateOnline, /*visible=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedReadinessCheckFail,
      hotspot_state_handler_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(3u, observer_.hotspot_capabilities_changed_count());

  // Disconnect the active cellular network
  service_test->SetServiceProperty(kCellularServicePath, shill::kStateProperty,
                                   base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoMobileData,
            hotspot_state_handler_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(4u, observer_.hotspot_capabilities_changed_count());

  // Simulate check tethering readiness operation success and re-connect the
  // cellular network
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess, shill::kTetheringReadinessReady);
  service_test->SetServiceProperty(kCellularServicePath, shill::kStateProperty,
                                   base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_config::mojom::HotspotAllowStatus::kAllowed,
            hotspot_state_handler_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(5u, observer_.hotspot_capabilities_changed_count());

  hotspot_state_handler_->SetPolicyAllowHotspot(/*allow*/ false);
  base::RunLoop().RunUntilIdle();
  // TODO (jiajunz): Should expect equal after SetPolicyAllowHotspot() is
  // implemented.
  EXPECT_NE(hotspot_config::mojom::HotspotAllowStatus::kDisallowedByPolicy,
            hotspot_state_handler_->GetHotspotCapabilities().allow_status);
}

TEST_F(HotspotStateHandlerTest, SetAndGetHotspotConfig) {
  EXPECT_EQ(hotspot_config::mojom::SetHotspotConfigResult::kFailedNotLogin,
            SetHotspotConfig(GenerateTestConfig()));
  ASSERT_FALSE(hotspot_state_handler_->GetHotspotConfig());
  EXPECT_EQ(0u, observer_.hotspot_status_changed_count());

  LoginToRegularUser();
  EXPECT_EQ(hotspot_config::mojom::SetHotspotConfigResult::kSuccess,
            SetHotspotConfig(GenerateTestConfig()));
  EXPECT_EQ(1u, observer_.hotspot_status_changed_count());
  auto hotspot_config = hotspot_state_handler_->GetHotspotConfig();
  EXPECT_TRUE(hotspot_config);
  EXPECT_FALSE(hotspot_config->auto_disable);
  EXPECT_EQ(hotspot_config->band, hotspot_config::mojom::WiFiBand::k5GHz);
  EXPECT_EQ(hotspot_config->security,
            hotspot_config::mojom::WiFiSecurityMode::kWpa2);
  EXPECT_EQ(hotspot_config->ssid, kHotspotConfigSSID);
  EXPECT_EQ(hotspot_config->passphrase, kHotspotConfigPassphrase);
  EXPECT_FALSE(hotspot_config->bssid_randomization);

  Logout();
  ASSERT_FALSE(hotspot_state_handler_->GetHotspotConfig());
  EXPECT_EQ(2u, observer_.hotspot_status_changed_count());
}

TEST_F(HotspotStateHandlerTest, CheckTetheringReadiness) {
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess, shill::kTetheringReadinessReady);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CheckTetheringReadiness(),
            HotspotStateHandler::CheckTetheringReadinessResult::kReady);

  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kTetheringReadinessNotAllowed);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CheckTetheringReadiness(),
            HotspotStateHandler::CheckTetheringReadinessResult::kNotAllowed);

  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess,
          /*readiness_result=*/std::string());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CheckTetheringReadiness(),
            HotspotStateHandler::CheckTetheringReadinessResult::kNotAllowed);

  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kFailure,
          /*readiness_result=*/std::string());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CheckTetheringReadiness(),
            HotspotStateHandler::CheckTetheringReadinessResult::
                kShillOperationFailed);
}

}  // namespace ash
