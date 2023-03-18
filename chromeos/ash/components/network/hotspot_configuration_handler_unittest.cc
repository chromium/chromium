// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_configuration_handler.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_controller.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/hotspot_enabled_state_test_observer.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kHotspotConfigSSID[] = "hotspot_SSID";
const char kHotspotConfigPassphrase[] = "hotspot_passphrase";

hotspot_config::mojom::HotspotConfigPtr GenerateTestConfig() {
  auto mojom_config = hotspot_config::mojom::HotspotConfig::New();
  mojom_config->auto_disable = false;
  mojom_config->band = hotspot_config::mojom::WiFiBand::kAutoChoose;
  mojom_config->security = hotspot_config::mojom::WiFiSecurityMode::kWpa2;
  mojom_config->ssid = kHotspotConfigSSID;
  mojom_config->passphrase = kHotspotConfigPassphrase;
  mojom_config->bssid_randomization = false;
  return mojom_config;
}

}  // namespace

class TestObserver : public HotspotConfigurationHandler::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // HotspotConfigurationHandler::Observer:
  void OnHotspotConfigurationChanged() override {
    hotspot_configuration_changed_count_++;
  }

  size_t hotspot_configuration_changed_count() {
    return hotspot_configuration_changed_count_;
  }

 private:
  size_t hotspot_configuration_changed_count_ = 0u;
};

class HotspotConfigurationHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kHotspot);
    LoginState::Initialize();
    LoginState::Get()->set_always_logged_in(false);

    if (hotspot_configuration_handler_ &&
        hotspot_configuration_handler_->HasObserver(&observer_)) {
      hotspot_configuration_handler_->RemoveObserver(&observer_);
    }
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
    hotspot_configuration_handler_ =
        std::make_unique<HotspotConfigurationHandler>();
    hotspot_configuration_handler_->AddObserver(&observer_);
    hotspot_configuration_handler_->Init(hotspot_controller_.get());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    hotspot_configuration_handler_->RemoveObserver(&observer_);
    hotspot_configuration_handler_.reset();
    hotspot_controller_.reset();
    hotspot_capabilities_provider_.reset();
    hotspot_state_handler_.reset();
    technology_state_controller_.reset();
    LoginState::Shutdown();
  }

  void SetupObserver() {
    hotspot_enabled_state_observer_ =
        std::make_unique<hotspot_config::HotspotEnabledStateTestObserver>();
    hotspot_controller_->ObserveEnabledStateChanges(
        hotspot_enabled_state_observer_->GenerateRemote());
  }

  hotspot_config::mojom::SetHotspotConfigResult SetHotspotConfig(
      hotspot_config::mojom::HotspotConfigPtr mojom_config) {
    base::RunLoop run_loop;
    hotspot_config::mojom::SetHotspotConfigResult result;
    hotspot_configuration_handler_->SetHotspotConfig(
        std::move(mojom_config),
        base::BindLambdaForTesting(
            [&](hotspot_config::mojom::SetHotspotConfigResult success) {
              result = success;
              run_loop.Quit();
            }));
    run_loop.Run();
    FlushMojoCalls();
    return result;
  }

  void SetHotspotStateInShill(const std::string& state) {
    base::Value::Dict status_dict;
    status_dict.Set(shill::kTetheringStatusStateProperty, state);
    network_state_test_helper_.manager_test()->SetManagerProperty(
        shill::kTetheringStatusProperty, base::Value(std::move(status_dict)));
    base::RunLoop().RunUntilIdle();
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

  void FlushMojoCalls() { base::RunLoop().RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<HotspotConfigurationHandler> hotspot_configuration_handler_;
  std::unique_ptr<HotspotController> hotspot_controller_;
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
  std::unique_ptr<HotspotStateHandler> hotspot_state_handler_;
  std::unique_ptr<TechnologyStateController> technology_state_controller_;
  std::unique_ptr<hotspot_config::HotspotEnabledStateTestObserver>
      hotspot_enabled_state_observer_;
  TestObserver observer_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
};

TEST_F(HotspotConfigurationHandlerTest, UpdateHotspotConfigWhenProfileLoaded) {
  const char kInitialSSID[] = "randomized_SSID";
  const char kInitialPassphrase[] = "randomized_passphrase";
  const char kLoadedSSID[] = "loaded_SSID";
  const char kLoadedPassphrase[] = "loaded_passphrase";

  base::Value::Dict config;
  config.Set(shill::kTetheringConfSSIDProperty,
             base::HexEncode(kInitialSSID, std::strlen(kInitialSSID)));
  config.Set(shill::kTetheringConfPassphraseProperty, kInitialPassphrase);
  config.Set(shill::kTetheringConfAutoDisableProperty, true);
  config.Set(shill::kTetheringConfBandProperty, shill::kBandAll);
  config.Set(shill::kTetheringConfMARProperty, false);
  config.Set(shill::kTetheringConfSecurityProperty, shill::kSecurityWpa2);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringConfigProperty, base::Value(config.Clone()));
  base::RunLoop().RunUntilIdle();

  LoginToRegularUser();
  EXPECT_EQ(hotspot_configuration_handler_->GetHotspotConfig()->ssid,
            kInitialSSID);
  EXPECT_EQ(hotspot_configuration_handler_->GetHotspotConfig()->passphrase,
            kInitialPassphrase);

  // Simulate shill load tethering config from user profile after login.
  config.Set(shill::kTetheringConfSSIDProperty,
             base::HexEncode(kLoadedSSID, std::strlen(kLoadedSSID)));
  config.Set(shill::kTetheringConfPassphraseProperty, kLoadedPassphrase);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringConfigProperty, base::Value(config.Clone()));
  // Signals "Profiles" property change when user profile is fully loaded.
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kProfilesProperty, base::Value());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_configuration_handler_->GetHotspotConfig()->ssid,
            kLoadedSSID);
  EXPECT_EQ(hotspot_configuration_handler_->GetHotspotConfig()->passphrase,
            kLoadedPassphrase);
}

TEST_F(HotspotConfigurationHandlerTest, SetAndGetHotspotConfig) {
  EXPECT_EQ(hotspot_config::mojom::SetHotspotConfigResult::kFailedNotLogin,
            SetHotspotConfig(GenerateTestConfig()));
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotSetConfigResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotSetConfigResultHistogram,
      HotspotMetricsHelper::HotspotMetricsSetConfigResult::kFailedNotLogin, 1);
  ASSERT_FALSE(hotspot_configuration_handler_->GetHotspotConfig());
  EXPECT_EQ(0u, observer_.hotspot_configuration_changed_count());

  LoginToRegularUser();
  EXPECT_EQ(hotspot_config::mojom::SetHotspotConfigResult::kSuccess,
            SetHotspotConfig(GenerateTestConfig()));
  EXPECT_EQ(1u, observer_.hotspot_configuration_changed_count());
  auto hotspot_config = hotspot_configuration_handler_->GetHotspotConfig();
  EXPECT_TRUE(hotspot_config);
  EXPECT_FALSE(hotspot_config->auto_disable);
  EXPECT_EQ(hotspot_config->band, hotspot_config::mojom::WiFiBand::kAutoChoose);
  EXPECT_EQ(hotspot_config->security,
            hotspot_config::mojom::WiFiSecurityMode::kWpa2);
  EXPECT_EQ(hotspot_config->ssid, kHotspotConfigSSID);
  EXPECT_EQ(hotspot_config->passphrase, kHotspotConfigPassphrase);
  EXPECT_FALSE(hotspot_config->bssid_randomization);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotSetConfigResultHistogram, 2);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotSetConfigResultHistogram,
      HotspotMetricsHelper::HotspotMetricsSetConfigResult::kSuccess, 1);

  Logout();
  ASSERT_FALSE(hotspot_configuration_handler_->GetHotspotConfig());
  EXPECT_EQ(2u, observer_.hotspot_configuration_changed_count());
}

TEST_F(HotspotConfigurationHandlerTest, SetHotspotConfigWhenHotspotIsActive) {
  SetupObserver();
  base::Value::Dict config;
  config.Set(
      shill::kTetheringConfSSIDProperty,
      base::HexEncode(kHotspotConfigSSID, std::strlen(kHotspotConfigSSID)));
  config.Set(shill::kTetheringConfPassphraseProperty, kHotspotConfigPassphrase);
  config.Set(shill::kTetheringConfAutoDisableProperty, true);
  config.Set(shill::kTetheringConfBandProperty, shill::kBandAll);
  config.Set(shill::kTetheringConfMARProperty, false);
  config.Set(shill::kTetheringConfSecurityProperty, shill::kSecurityWpa2);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringConfigProperty, base::Value(config.Clone()));
  base::RunLoop().RunUntilIdle();
  LoginToRegularUser();
  EXPECT_EQ(1u, observer_.hotspot_configuration_changed_count());

  // Verifies that restart hotspot will not be triggered if hotspot is not
  // active.
  auto mojom_config = hotspot_config::mojom::HotspotConfig::New();
  mojom_config->auto_disable = true;
  mojom_config->band = hotspot_config::mojom::WiFiBand::kAutoChoose;
  mojom_config->security = hotspot_config::mojom::WiFiSecurityMode::kWpa2;
  mojom_config->ssid = "new_ssid";
  mojom_config->passphrase = kHotspotConfigPassphrase;
  mojom_config->bssid_randomization = false;
  EXPECT_EQ(hotspot_config::mojom::SetHotspotConfigResult::kSuccess,
            SetHotspotConfig(mojom_config->Clone()));
  EXPECT_EQ(2u, observer_.hotspot_configuration_changed_count());
  EXPECT_EQ(0u, hotspot_enabled_state_observer_->hotspot_turned_off_count());

  auto hotspot_config = hotspot_configuration_handler_->GetHotspotConfig();
  EXPECT_EQ("new_ssid", hotspot_config->ssid);

  // Verifies that only change auto_disable will not trigger restart.
  SetHotspotStateInShill(shill::kTetheringStateActive);
  network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);

  mojom_config->auto_disable = false;
  EXPECT_EQ(hotspot_config::mojom::SetHotspotConfigResult::kSuccess,
            SetHotspotConfig(mojom_config->Clone()));
  EXPECT_EQ(0u, hotspot_enabled_state_observer_->hotspot_turned_off_count());
  hotspot_config = hotspot_configuration_handler_->GetHotspotConfig();
  EXPECT_FALSE(hotspot_config->auto_disable);

  mojom_config->passphrase = "new_passphrase";
  EXPECT_EQ(hotspot_config::mojom::SetHotspotConfigResult::kSuccess,
            SetHotspotConfig(mojom_config->Clone()));
  EXPECT_EQ(1u, hotspot_enabled_state_observer_->hotspot_turned_off_count());
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kRestart,
            hotspot_enabled_state_observer_->last_disable_reason());
  hotspot_config = hotspot_configuration_handler_->GetHotspotConfig();
  EXPECT_EQ("new_passphrase", hotspot_config->passphrase);
}

}  // namespace ash
