// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"

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

const char kCellularServicePath[] = "/service/cellular0";
const char kCellularServiceGuid[] = "cellular_guid0";
const char kCellularServiceName[] = "cellular_name0";

}  // namespace

class TestObserver : public HotspotCapabilitiesProvider::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void OnHotspotCapabilitiesChanged() override {
    hotspot_capabilities_changed_count_++;
  }

  size_t hotspot_capabilities_changed_count() {
    return hotspot_capabilities_changed_count_;
  }

 private:
  size_t hotspot_capabilities_changed_count_ = 0u;
};

class HotspotCapabilitiesProviderTest : public ::testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kHotspot);

    if (hotspot_capabilities_provider_ &&
        hotspot_capabilities_provider_->HasObserver(&observer_)) {
      hotspot_capabilities_provider_->RemoveObserver(&observer_);
    }
    hotspot_capabilities_provider_ =
        std::make_unique<HotspotCapabilitiesProvider>();
    hotspot_capabilities_provider_->AddObserver(&observer_);
    hotspot_capabilities_provider_->Init(
        network_state_test_helper_.network_state_handler());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    hotspot_capabilities_provider_->RemoveObserver(&observer_);
    hotspot_capabilities_provider_.reset();
  }

  HotspotCapabilitiesProvider::CheckTetheringReadinessResult
  CheckTetheringReadiness() {
    base::RunLoop run_loop;
    HotspotCapabilitiesProvider::CheckTetheringReadinessResult return_result;
    hotspot_capabilities_provider_->CheckTetheringReadiness(
        base::BindLambdaForTesting(
            [&](HotspotCapabilitiesProvider::CheckTetheringReadinessResult
                    result) {
              return_result = result;
              run_loop.QuitClosure();
            }));
    run_loop.RunUntilIdle();
    return return_result;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
  TestObserver observer_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
};

TEST_F(HotspotCapabilitiesProviderTest, GetHotspotCapabilities) {
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoCellularUpstream,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(0u, observer_.hotspot_capabilities_changed_count());

  base::Value::Dict capabilities_dict;
  capabilities_dict.Set(shill::kTetheringCapUpstreamProperty,
                        base::Value::List());
  capabilities_dict.Set(shill::kTetheringCapDownstreamProperty,
                        base::Value::List());
  capabilities_dict.Set(shill::kTetheringCapSecurityProperty,
                        base::Value::List());
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringCapabilitiesProperty,
      base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoCellularUpstream,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(0u, observer_.hotspot_capabilities_changed_count());

  base::Value::List upstream_list;
  upstream_list.Append(shill::kTypeCellular);
  capabilities_dict.Set(shill::kTetheringCapUpstreamProperty,
                        std::move(upstream_list));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringCapabilitiesProperty,
      base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoWiFiDownstream,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(1u, observer_.hotspot_capabilities_changed_count());

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
      base::Value(capabilities_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoMobileData,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(2u, hotspot_capabilities_provider_->GetHotspotCapabilities()
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
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(3u, observer_.hotspot_capabilities_changed_count());

  // Disconnect the active cellular network
  service_test->SetServiceProperty(kCellularServicePath, shill::kStateProperty,
                                   base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoMobileData,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(4u, observer_.hotspot_capabilities_changed_count());

  // Simulate check tethering readiness operation success and re-connect the
  // cellular network
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess, shill::kTetheringReadinessReady);
  service_test->SetServiceProperty(kCellularServicePath, shill::kStateProperty,
                                   base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      hotspot_config::mojom::HotspotAllowStatus::kAllowed,
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status);
  EXPECT_EQ(5u, observer_.hotspot_capabilities_changed_count());
}

TEST_F(HotspotCapabilitiesProviderTest, CheckTetheringReadiness) {
  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess, shill::kTetheringReadinessReady);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CheckTetheringReadiness(),
            HotspotCapabilitiesProvider::CheckTetheringReadinessResult::kReady);

  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess,
          shill::kTetheringReadinessNotAllowed);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      CheckTetheringReadiness(),
      HotspotCapabilitiesProvider::CheckTetheringReadinessResult::kNotAllowed);

  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kSuccess,
          /*readiness_status=*/std::string());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      CheckTetheringReadiness(),
      HotspotCapabilitiesProvider::CheckTetheringReadinessResult::kNotAllowed);

  network_state_test_helper_.manager_test()
      ->SetSimulateCheckTetheringReadinessResult(
          FakeShillSimulatedResult::kFailure,
          /*readiness_status=*/std::string());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CheckTetheringReadiness(),
            HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
                kShillOperationFailed);
}

}  // namespace ash
