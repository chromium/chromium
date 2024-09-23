// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/hotspot_config/cros_hotspot_config.h"

#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_test_observer.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/hotspot_enabled_state_test_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::hotspot_config {

namespace {

const char kHotspotConfigSSID[] = "hotspot_SSID";
const char kHotspotConfigPassphrase[] = "hotspot_passphrase";
const char kCellularServicePath[] = "/service/cellular0";
const char kCellularServiceGuid[] = "cellular_guid0";
const char kCellularServiceName[] = "cellular_name0";

mojom::HotspotConfigPtr GenerateTestConfig() {
  auto mojom_config = mojom::HotspotConfig::New();
  mojom_config->auto_disable = false;
  mojom_config->band = mojom::WiFiBand::kAutoChoose;
  mojom_config->security = mojom::WiFiSecurityMode::kWpa2;
  mojom_config->ssid = kHotspotConfigSSID;
  mojom_config->passphrase = kHotspotConfigPassphrase;
  return mojom_config;
}

}  // namespace

class CrosHotspotConfigTest : public testing::Test {
 public:
  CrosHotspotConfigTest() = default;
  CrosHotspotConfigTest(const CrosHotspotConfigTest&) = delete;
  CrosHotspotConfigTest& operator=(const CrosHotspotConfigTest&) = delete;
  ~CrosHotspotConfigTest() override = default;

  // testing::Test:
  void SetUp() override {
    LoginState::Initialize();
    LoginState::Get()->set_always_logged_in(false);

    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();
    network_handler_test_helper_->ResetDevicesAndServices();
    NetworkHandler* network_handler = NetworkHandler::Get();
    // Use base::WrapUnique(new CrosHotspotConfig(...)) instead of
    // std::make_unique<CrosHotspotConfig> to access a private constructor.
    cros_hotspot_config_ = base::WrapUnique(new CrosHotspotConfig(
        network_handler->hotspot_capabilities_provider(),
        network_handler->hotspot_state_handler(),
        network_handler->hotspot_controller(),
        network_handler->hotspot_configuration_handler(),
        network_handler->hotspot_enabled_state_notifier()));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    cros_hotspot_config_.reset();
    network_handler_test_helper_.reset();
    LoginState::Shutdown();
  }

  void SetupObserver() {
    observer_ = std::make_unique<CrosHotspotConfigTestObserver>();
    cros_hotspot_config_->AddObserver(observer_->GenerateRemote());

    hotspot_enabled_state_observer_ =
        std::make_unique<HotspotEnabledStateTestObserver>();
    cros_hotspot_config_->ObserveEnabledStateChanges(
        hotspot_enabled_state_observer_->GenerateRemote());
    base::RunLoop().RunUntilIdle();
  }

  void SetValidHotspotCapabilities() {
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
    network_handler_test_helper_->manager_test()->SetManagerProperty(
        shill::kTetheringCapabilitiesProperty,
        base::Value(std::move(capabilities_dict)));
    base::RunLoop().RunUntilIdle();
  }

  void AddActiveCellularService() {
    network_handler_test_helper_->service_test()->AddService(
        kCellularServicePath, kCellularServiceGuid, kCellularServiceName,
        shill::kTypeCellular, shill::kStateOnline, /*visible=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetHotspotStateInShill(const std::string& state) {
    // Update tethering status to active in Shill.
    base::Value::Dict status_dict;
    status_dict.Set(shill::kTetheringStatusStateProperty, state);
    network_handler_test_helper_->manager_test()->SetManagerProperty(
        shill::kTetheringStatusProperty, base::Value(std::move(status_dict)));
    base::RunLoop().RunUntilIdle();
  }

  void SetReadinessCheckResultReady() {
    network_handler_test_helper_->manager_test()
        ->SetSimulateCheckTetheringReadinessResult(
            FakeShillSimulatedResult::kSuccess,
            shill::kTetheringReadinessReady);
    base::RunLoop().RunUntilIdle();
  }

  mojom::HotspotInfoPtr GetHotspotInfo() {
    mojom::HotspotInfoPtr out_result;
    base::RunLoop run_loop;
    cros_hotspot_config_->GetHotspotInfo(
        base::BindLambdaForTesting([&](mojom::HotspotInfoPtr result) {
          out_result = std::move(result);
          run_loop.Quit();
        }));
    run_loop.Run();
    return out_result;
  }

  mojom::SetHotspotConfigResult SetHotspotConfig(
      mojom::HotspotConfigPtr mojom_config) {
    base::RunLoop run_loop;
    mojom::SetHotspotConfigResult out_result;
    cros_hotspot_config_->SetHotspotConfig(
        std::move(mojom_config),
        base::BindLambdaForTesting([&](mojom::SetHotspotConfigResult result) {
          out_result = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    FlushMojoCalls();
    return out_result;
  }

  mojom::HotspotControlResult EnableHotspot() {
    base::RunLoop run_loop;
    mojom::HotspotControlResult out_result;
    cros_hotspot_config_->EnableHotspot(
        base::BindLambdaForTesting([&](mojom::HotspotControlResult result) {
          out_result = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    FlushMojoCalls();
    return out_result;
  }

  mojom::HotspotControlResult DisableHotspot() {
    base::RunLoop run_loop;
    mojom::HotspotControlResult out_result;
    cros_hotspot_config_->DisableHotspot(
        base::BindLambdaForTesting([&](mojom::HotspotControlResult result) {
          out_result = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    FlushMojoCalls();
    return out_result;
  }

  void LoginToRegularUser() {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_REGULAR);
    task_environment_.RunUntilIdle();
  }

  void FlushMojoCalls() { base::RunLoop().RunUntilIdle(); }

  NetworkHandlerTestHelper* helper() {
    return network_handler_test_helper_.get();
  }

  CrosHotspotConfigTestObserver* observer() { return observer_.get(); }

  HotspotEnabledStateTestObserver* hotspotStateObserver() {
    return hotspot_enabled_state_observer_.get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<CrosHotspotConfig> cros_hotspot_config_;
  std::unique_ptr<CrosHotspotConfigTestObserver> observer_;
  std::unique_ptr<HotspotEnabledStateTestObserver>
      hotspot_enabled_state_observer_;
};

TEST_F(CrosHotspotConfigTest, GetHotspotInfo) {
  SetupObserver();
  EXPECT_EQ(observer()->hotspot_info_changed_count(), 1u);

  auto hotspot_info = GetHotspotInfo();
  EXPECT_EQ(hotspot_info->state, mojom::HotspotState::kDisabled);
  EXPECT_EQ(hotspot_info->client_count, 0u);
  EXPECT_EQ(hotspot_info->allow_status,
            mojom::HotspotAllowStatus::kDisallowedNoMobileData);
  EXPECT_EQ(hotspot_info->allowed_wifi_security_modes.size(), 1u);
  EXPECT_TRUE(hotspot_info->config);

  SetReadinessCheckResultReady();
  SetValidHotspotCapabilities();
  base::RunLoop().RunUntilIdle();
  hotspot_info = GetHotspotInfo();
  EXPECT_EQ(hotspot_info->state, mojom::HotspotState::kDisabled);
  EXPECT_EQ(hotspot_info->client_count, 0u);
  EXPECT_EQ(hotspot_info->allow_status,
            mojom::HotspotAllowStatus::kDisallowedNoMobileData);
  EXPECT_EQ(hotspot_info->allowed_wifi_security_modes.size(), 2u);
  EXPECT_TRUE(hotspot_info->config);
  EXPECT_EQ(observer()->hotspot_info_changed_count(), 2u);

  AddActiveCellularService();
  base::RunLoop().RunUntilIdle();
  hotspot_info = GetHotspotInfo();
  EXPECT_EQ(hotspot_info->allow_status, mojom::HotspotAllowStatus::kAllowed);
  EXPECT_EQ(observer()->hotspot_info_changed_count(), 3u);

  SetHotspotStateInShill(shill::kTetheringStateActive);
  EXPECT_EQ(GetHotspotInfo()->state, mojom::HotspotState::kEnabled);
  EXPECT_EQ(observer()->hotspot_info_changed_count(), 4u);

  SetHotspotStateInShill(shill::kTetheringStateIdle);
  EXPECT_EQ(GetHotspotInfo()->state, mojom::HotspotState::kDisabled);
  EXPECT_EQ(observer()->hotspot_info_changed_count(), 5u);

  // Simulate user starting tethering
  SetHotspotStateInShill(shill::kTetheringStateStarting);
  EXPECT_EQ(GetHotspotInfo()->state, mojom::HotspotState::kEnabling);
  EXPECT_EQ(observer()->hotspot_info_changed_count(), 6u);
}

TEST_F(CrosHotspotConfigTest, SetHotspotConfig) {
  SetupObserver();
  EXPECT_EQ(observer()->hotspot_info_changed_count(), 1u);
  // Verifies that set hotspot config return failed when the user is not login.
  EXPECT_EQ(mojom::SetHotspotConfigResult::kFailedNotLogin,
            SetHotspotConfig(GenerateTestConfig()));
  // FakeShillManager return valid hotspot config regardless login or not.
  EXPECT_TRUE(GetHotspotInfo()->config);

  LoginToRegularUser();
  EXPECT_EQ(observer()->hotspot_info_changed_count(), 2u);
  EXPECT_EQ(mojom::SetHotspotConfigResult::kSuccess,
            SetHotspotConfig(GenerateTestConfig()));
  auto hotspot_info = GetHotspotInfo();
  EXPECT_TRUE(hotspot_info->config);
  EXPECT_FALSE(hotspot_info->config->auto_disable);
  EXPECT_EQ(hotspot_info->config->band, mojom::WiFiBand::kAutoChoose);
  EXPECT_EQ(hotspot_info->config->security, mojom::WiFiSecurityMode::kWpa2);
  EXPECT_EQ(hotspot_info->config->ssid, kHotspotConfigSSID);
  EXPECT_EQ(hotspot_info->config->passphrase, kHotspotConfigPassphrase);
  EXPECT_EQ(observer()->hotspot_info_changed_count(), 3u);
}

TEST_F(CrosHotspotConfigTest, EnableHotspot) {
  SetupObserver();
  EXPECT_EQ(mojom::HotspotControlResult::kReadinessCheckFailed,
            EnableHotspot());

  SetReadinessCheckResultReady();
  SetValidHotspotCapabilities();
  AddActiveCellularService();
  helper()->manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(mojom::HotspotControlResult::kSuccess, EnableHotspot());
  EXPECT_EQ(hotspotStateObserver()->hotspot_turned_on_count(), 1u);

  // Simulate check tethering readiness operation fail.
  helper()->manager_test()->SetSimulateCheckTetheringReadinessResult(
      FakeShillSimulatedResult::kFailure,
      /*readiness_status=*/std::string());
  base::RunLoop().RunUntilIdle();

  SetHotspotStateInShill(shill::kTetheringStateIdle);
  EXPECT_EQ(mojom::HotspotControlResult::kReadinessCheckFailed,
            EnableHotspot());
}

TEST_F(CrosHotspotConfigTest, DisableHotspot) {
  SetupObserver();
  SetHotspotStateInShill(shill::kTetheringStateActive);
  helper()->manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(mojom::HotspotControlResult::kSuccess, DisableHotspot());
  EXPECT_EQ(hotspotStateObserver()->hotspot_turned_off_count(), 1u);
  EXPECT_EQ(hotspotStateObserver()->last_disable_reason(),
            hotspot_config::mojom::DisableReason::kUserInitiated);
}

}  // namespace ash::hotspot_config
