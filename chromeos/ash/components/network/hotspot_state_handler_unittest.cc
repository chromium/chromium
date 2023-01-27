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

class TestObserver : public HotspotStateHandler::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // HotspotStateHandler::Observer:
  void OnHotspotStatusChanged() override { hotspot_status_changed_count_++; }

  size_t hotspot_status_changed_count() {
    return hotspot_status_changed_count_;
  }

 private:
  size_t hotspot_status_changed_count_ = 0u;
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
    hotspot_state_handler_->Init();
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
  base::Value::Dict status_dict;
  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateActive);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            hotspot_config::mojom::HotspotState::kEnabled);
  EXPECT_EQ(1u, observer_.hotspot_status_changed_count());

  // Update tethering status to idle in Shill.
  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateIdle);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            hotspot_config::mojom::HotspotState::kDisabled);
  EXPECT_EQ(2u, observer_.hotspot_status_changed_count());

  // Simulate user starting tethering.
  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateStarting);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            hotspot_config::mojom::HotspotState::kEnabling);
  EXPECT_EQ(3u, observer_.hotspot_status_changed_count());
}

TEST_F(HotspotStateHandlerTest, GetHotspotActiveClientCount) {
  EXPECT_EQ(0u, hotspot_state_handler_->GetHotspotActiveClientCount());

  base::Value::Dict status_dict;
  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateActive);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, hotspot_state_handler_->GetHotspotActiveClientCount());
  EXPECT_EQ(1u, observer_.hotspot_status_changed_count());

  // Update tethering status with one active client.
  base::Value::List active_clients_list;
  base::Value::Dict client;
  client.Set(shill::kTetheringStatusClientIPv4Property, "IPV4:001");
  client.Set(shill::kTetheringStatusClientHostnameProperty, "hostname1");
  client.Set(shill::kTetheringStatusClientMACProperty, "persist");
  active_clients_list.Append(std::move(client));
  status_dict.Set(shill::kTetheringStatusClientsProperty,
                  std::move(active_clients_list));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, hotspot_state_handler_->GetHotspotActiveClientCount());
  EXPECT_EQ(2u, observer_.hotspot_status_changed_count());

  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateIdle);
  status_dict.Remove(shill::kTetheringStatusClientsProperty);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, hotspot_state_handler_->GetHotspotActiveClientCount());
  EXPECT_EQ(3u, observer_.hotspot_status_changed_count());
}

TEST_F(HotspotStateHandlerTest, UpdateHotspotConfigWhenProfileLoaded) {
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
  EXPECT_EQ(hotspot_state_handler_->GetHotspotConfig()->ssid, kInitialSSID);
  EXPECT_EQ(hotspot_state_handler_->GetHotspotConfig()->passphrase,
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

  EXPECT_EQ(hotspot_state_handler_->GetHotspotConfig()->ssid, kLoadedSSID);
  EXPECT_EQ(hotspot_state_handler_->GetHotspotConfig()->passphrase,
            kLoadedPassphrase);
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
  EXPECT_EQ(hotspot_config->band, hotspot_config::mojom::WiFiBand::kAutoChoose);
  EXPECT_EQ(hotspot_config->security,
            hotspot_config::mojom::WiFiSecurityMode::kWpa2);
  EXPECT_EQ(hotspot_config->ssid, kHotspotConfigSSID);
  EXPECT_EQ(hotspot_config->passphrase, kHotspotConfigPassphrase);
  EXPECT_FALSE(hotspot_config->bssid_randomization);

  Logout();
  ASSERT_FALSE(hotspot_state_handler_->GetHotspotConfig());
  EXPECT_EQ(2u, observer_.hotspot_status_changed_count());
}

}  // namespace ash
