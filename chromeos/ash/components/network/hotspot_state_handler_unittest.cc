// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "chromeos/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kHotspotConfigSSID[] = "hotspot_SSID";
const char kHotspotConfigPassphrase[] = "hotspot_passphrase";

chromeos::hotspot_config::mojom::HotspotConfigPtr GenerateTestConfig() {
  auto mojom_config = chromeos::hotspot_config::mojom::HotspotConfig::New();
  mojom_config->auto_disable = false;
  mojom_config->band = chromeos::hotspot_config::mojom::WiFiBand::k5GHz;
  mojom_config->security =
      chromeos::hotspot_config::mojom::WiFiSecurityMode::kWpa2;
  mojom_config->ssid = kHotspotConfigSSID;
  mojom_config->passphrase = kHotspotConfigPassphrase;
  return mojom_config;
}

}  // namespace

class TestObserver : public HotspotStateHandler::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // HotspotStateHandler::Observer:
  void OnHotspotStatusChanged() override { hotspot_status_changed_count_++; }

  void OnHotspotStateFailed(const std::string& error) override {
    hotspot_state_failed_count_++;
    last_hotspot_failed_error_ = error;
  }

  size_t hotspot_status_changed_count() {
    return hotspot_status_changed_count_;
  }
  size_t hotspot_state_failed_count() { return hotspot_state_failed_count_; }

  const std::string& last_hotspot_failed_error() {
    return last_hotspot_failed_error_;
  }

 private:
  size_t hotspot_status_changed_count_ = 0u;
  size_t hotspot_state_failed_count_ = 0u;
  std::string last_hotspot_failed_error_;
};

class HotspotStateHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kHotspot);
    shill_clients::InitializeFakes();
    LoginState::Initialize();
    LoginState::Get()->set_always_logged_in(false);
    base::RunLoop().RunUntilIdle();

    if (hotspot_state_handler_ &&
        hotspot_state_handler_->HasObserver(&observer_)) {
      hotspot_state_handler_->RemoveObserver(&observer_);
    }
    hotspot_state_handler_ = std::make_unique<HotspotStateHandler>();
    hotspot_state_handler_->AddObserver(&observer_);
    hotspot_state_handler_->Init();
  }

  void TearDown() override {
    hotspot_state_handler_->RemoveObserver(&observer_);
    hotspot_state_handler_.reset();
    shill_clients::Shutdown();
    LoginState::Shutdown();
  }

  chromeos::hotspot_config::mojom::SetHotspotConfigResult SetHotspotConfig(
      chromeos::hotspot_config::mojom::HotspotConfigPtr mojom_config) {
    base::RunLoop run_loop;
    chromeos::hotspot_config::mojom::SetHotspotConfigResult result;
    hotspot_state_handler_->SetHotspotConfig(
        std::move(mojom_config),
        base::BindLambdaForTesting(
            [&](chromeos::hotspot_config::mojom::SetHotspotConfigResult
                    success) {
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
};

TEST_F(HotspotStateHandlerTest, GetHotspotState) {
  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            chromeos::hotspot_config::mojom::HotspotState::kDisabled);

  // Update tethering status to active in Shill.
  base::Value status_dict(base::Value::Type::DICTIONARY);
  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateActive));
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            chromeos::hotspot_config::mojom::HotspotState::kEnabled);
  EXPECT_EQ(1u, observer_.hotspot_status_changed_count());
  EXPECT_EQ(0u, observer_.hotspot_state_failed_count());

  // Update tethering status to idle in Shill.
  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateIdle));
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            chromeos::hotspot_config::mojom::HotspotState::kDisabled);
  EXPECT_EQ(2u, observer_.hotspot_status_changed_count());
  EXPECT_EQ(0u, observer_.hotspot_state_failed_count());

  // Simulate user starting tethering and failed.
  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateStarting));
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();
  // Update tethering status to failure in Shill.
  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateFailure));
  status_dict.GetDict().Set(
      shill::kTetheringStatusErrorProperty,
      base::Value(shill::kTetheringErrorUpstreamNotReady));
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            chromeos::hotspot_config::mojom::HotspotState::kDisabled);
  EXPECT_EQ(4u, observer_.hotspot_status_changed_count());
  EXPECT_EQ(1u, observer_.hotspot_state_failed_count());
  EXPECT_EQ(shill::kTetheringErrorUpstreamNotReady,
            observer_.last_hotspot_failed_error());

  // Verify the edge case where the state is failure but error is not provided.
  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateStarting));
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();
  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateFailure));
  status_dict.GetDict().Remove(shill::kTetheringStatusErrorProperty);
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(hotspot_state_handler_->GetHotspotState(),
            chromeos::hotspot_config::mojom::HotspotState::kDisabled);
  EXPECT_EQ(6u, observer_.hotspot_status_changed_count());
  EXPECT_EQ(2u, observer_.hotspot_state_failed_count());
  EXPECT_EQ(std::string(), observer_.last_hotspot_failed_error());
}

TEST_F(HotspotStateHandlerTest, GetHotspotActiveClientCount) {
  EXPECT_EQ(0u, hotspot_state_handler_->GetHotspotActiveClientCount());

  base::Value status_dict(base::Value::Type::DICTIONARY);
  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateActive));
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
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
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, hotspot_state_handler_->GetHotspotActiveClientCount());
  EXPECT_EQ(2u, observer_.hotspot_status_changed_count());

  status_dict.GetDict().Set(shill::kTetheringStatusStateProperty,
                            base::Value(shill::kTetheringStateIdle));
  status_dict.GetDict().Remove(shill::kTetheringStatusClientsProperty);
  ShillManagerClient::Get()->GetTestInterface()->SetManagerProperty(
      shill::kTetheringStatusProperty, status_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, hotspot_state_handler_->GetHotspotActiveClientCount());
  EXPECT_EQ(3u, observer_.hotspot_status_changed_count());
}

TEST_F(HotspotStateHandlerTest, SetAndGetHotspotConfig) {
  EXPECT_EQ(
      chromeos::hotspot_config::mojom::SetHotspotConfigResult::kFailedNotLogin,
      SetHotspotConfig(GenerateTestConfig()));
  ASSERT_FALSE(hotspot_state_handler_->GetHotspotConfig());
  EXPECT_EQ(0u, observer_.hotspot_status_changed_count());

  LoginToRegularUser();
  EXPECT_EQ(chromeos::hotspot_config::mojom::SetHotspotConfigResult::kSuccess,
            SetHotspotConfig(GenerateTestConfig()));
  EXPECT_EQ(1u, observer_.hotspot_status_changed_count());
  auto hotspot_config = hotspot_state_handler_->GetHotspotConfig();
  ASSERT_TRUE(hotspot_config);
  ASSERT_FALSE(hotspot_config->auto_disable);
  EXPECT_EQ(hotspot_config->band,
            chromeos::hotspot_config::mojom::WiFiBand::k5GHz);
  EXPECT_EQ(hotspot_config->security,
            chromeos::hotspot_config::mojom::WiFiSecurityMode::kWpa2);
  EXPECT_EQ(hotspot_config->ssid, kHotspotConfigSSID);
  EXPECT_EQ(hotspot_config->passphrase, kHotspotConfigPassphrase);

  Logout();
  ASSERT_FALSE(hotspot_state_handler_->GetHotspotConfig());
  EXPECT_EQ(2u, observer_.hotspot_status_changed_count());
}

}  // namespace ash
