// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_state_handler.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/hotspot_enabled_state_test_observer.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

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
  }

  void SetupObserver() {
    hotspot_enabled_state_observer_ =
        std::make_unique<hotspot_config::HotspotEnabledStateTestObserver>();
    hotspot_state_handler_->ObserveEnabledStateChanges(
        hotspot_enabled_state_observer_->GenerateRemote());
  }

  hotspot_config::HotspotEnabledStateTestObserver* hotspotStateObserver() {
    return hotspot_enabled_state_observer_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<HotspotStateHandler> hotspot_state_handler_;
  TestObserver observer_;
  std::unique_ptr<hotspot_config::HotspotEnabledStateTestObserver>
      hotspot_enabled_state_observer_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
};

TEST_F(HotspotStateHandlerTest, DisableReason) {
  SetupObserver();
  base::Value::Dict status_dict;
  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateIdle);

  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonInitialState);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, hotspotStateObserver()->hotspot_turned_off_count());

  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonInactive);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kAutoDisabled,
            hotspotStateObserver()->last_disable_reason());

  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonUpstreamDisconnect);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kUpstreamNetworkNotAvailable,
            hotspotStateObserver()->last_disable_reason());

  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonError);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kInternalError,
            hotspotStateObserver()->last_disable_reason());

  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonSuspend);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kSuspended,
            hotspotStateObserver()->last_disable_reason());

  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonUserExit);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kUserInitiated,
            hotspotStateObserver()->last_disable_reason());

  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonClientStop);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::DisableReason::kUserInitiated,
            hotspotStateObserver()->last_disable_reason());
}

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

}  // namespace ash
