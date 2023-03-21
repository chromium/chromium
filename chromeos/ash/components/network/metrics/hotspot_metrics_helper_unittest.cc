// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_configuration_handler.h"
#include "chromeos/ash/components/network/hotspot_controller.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kCellularServicePath[] = "/service/cellular0";
const char kCellularServiceGuid[] = "cellular_guid0";
const char kCellularServiceName[] = "cellular_name0";

}  // namespace

class HotspotMetricsHelperTest : public testing::Test {
 public:
  HotspotMetricsHelperTest() = default;
  HotspotMetricsHelperTest(const HotspotMetricsHelperTest&) = delete;
  HotspotMetricsHelperTest& operator=(const HotspotMetricsHelperTest&) = delete;
  ~HotspotMetricsHelperTest() override = default;

  void SetUp() override {
    LoginState::Initialize();

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
    hotspot_configuration_handler_->Init(hotspot_controller_.get());
    hotspot_metrics_helper_ = std::make_unique<HotspotMetricsHelper>();
    hotspot_metrics_helper_->Init(
        hotspot_capabilities_provider_.get(), hotspot_state_handler_.get(),
        hotspot_controller_.get(), hotspot_configuration_handler_.get(),
        network_state_test_helper_.network_state_handler());

    base::RunLoop().RunUntilIdle();
  }

  void PrepareEnableHotspotForTesting() {
    SetHotspotStateInShill(shill::kTetheringStateIdle);
    SetHotspotAllowStatus(hotspot_config::mojom::HotspotAllowStatus::kAllowed);
    network_state_test_helper_.manager_test()
        ->SetSimulateCheckTetheringReadinessResult(
            FakeShillSimulatedResult::kSuccess,
            shill::kTetheringReadinessReady);
    network_state_test_helper_.manager_test()->SetSimulateTetheringEnableResult(
        FakeShillSimulatedResult::kSuccess,
        shill::kTetheringEnableResultSuccess);
  }

  void SetHotspotAllowStatus(
      hotspot_config::mojom::HotspotAllowStatus allow_status) {
    hotspot_capabilities_provider_->SetHotspotAllowStatus(allow_status);
  }

  void SetHotspotStateInShill(const std::string& hotspot_state) {
    base::Value::Dict status_dict;
    status_dict.Set(shill::kTetheringStatusStateProperty, hotspot_state);
    network_state_test_helper_.manager_test()->SetManagerProperty(
        shill::kTetheringStatusProperty, base::Value(std::move(status_dict)));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    hotspot_metrics_helper_.reset();
    hotspot_configuration_handler_.reset();
    hotspot_controller_.reset();
    hotspot_capabilities_provider_.reset();
    hotspot_state_handler_.reset();
    technology_state_controller_.reset();
    LoginState::Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
  std::unique_ptr<HotspotStateHandler> hotspot_state_handler_;
  std::unique_ptr<TechnologyStateController> technology_state_controller_;
  std::unique_ptr<HotspotController> hotspot_controller_;
  std::unique_ptr<HotspotConfigurationHandler> hotspot_configuration_handler_;
  std::unique_ptr<HotspotMetricsHelper> hotspot_metrics_helper_;
};

TEST_F(HotspotMetricsHelperTest, HotspotAllowStatusHistogram) {
  using hotspot_config::mojom::HotspotAllowStatus;

  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  SetHotspotAllowStatus(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoMobileData);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotAllowStatusHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotAllowStatusHistogram,
      HotspotMetricsHelper::HotspotMetricsAllowStatus::kDisallowedNoMobileData,
      1);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotAllowStatusAtLoginHistogram, 0);

  task_environment_.FastForwardBy(
      HotspotMetricsHelper::kLogAllowStatusAtLoginTimeout);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotAllowStatusHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotAllowStatusAtLoginHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotAllowStatusAtLoginHistogram,
      HotspotMetricsHelper::HotspotMetricsAllowStatus::kDisallowedNoMobileData,
      1);

  SetHotspotAllowStatus(hotspot_config::mojom::HotspotAllowStatus::kAllowed);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotAllowStatusHistogram, 2);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotAllowStatusHistogram,
      HotspotMetricsHelper::HotspotMetricsAllowStatus::kAllowed, 1);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotAllowStatusAtLoginHistogram, 1);
}

TEST_F(HotspotMetricsHelperTest, HotspotUsageConfigHistogram) {
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  auto mojom_config = hotspot_config::mojom::HotspotConfig::New();
  mojom_config->auto_disable = true;
  mojom_config->band = hotspot_config::mojom::WiFiBand::kAutoChoose;
  mojom_config->ssid = "test_ssid";
  mojom_config->passphrase = "test_password";
  mojom_config->bssid_randomization = true;
  hotspot_configuration_handler_->SetHotspotConfig(std::move(mojom_config),
                                                   base::DoNothing());
  base::RunLoop().RunUntilIdle();

  PrepareEnableHotspotForTesting();
  hotspot_controller_->EnableHotspot(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotUsageConfigAutoDisable, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotUsageConfigAutoDisable, true, 1);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotUsageConfigMAR, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotUsageConfigMAR, true, 1);
  histogram_tester_.ExpectTotalCount(
      HotspotMetricsHelper::kHotspotUsageConfigCompatibilityMode, 1);
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotUsageConfigCompatibilityMode, false, 1);
}

TEST_F(HotspotMetricsHelperTest, HotspotUsageDurationHistogram) {
  const base::TimeDelta kHotspotUsageTime = base::Seconds(123);
  PrepareEnableHotspotForTesting();
  hotspot_controller_->EnableHotspot(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  task_environment_.FastForwardBy(kHotspotUsageTime);

  SetHotspotStateInShill(shill::kTetheringStateActive);
  hotspot_controller_->DisableHotspot(
      base::DoNothing(), hotspot_config::mojom::DisableReason::kUserInitiated);
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectTimeBucketCount(
      HotspotMetricsHelper::kHotspotUsageDuration, kHotspotUsageTime, 1);

  SetHotspotStateInShill(shill::kTetheringStateIdle);
  // Verifies that the usage duration is logged if hotspot is torn down by
  // internal error.
  hotspot_controller_->EnableHotspot(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  task_environment_.FastForwardBy(kHotspotUsageTime);

  base::Value::Dict status_dict;
  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateIdle);
  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonError);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectTimeBucketCount(
      HotspotMetricsHelper::kHotspotUsageDuration, kHotspotUsageTime, 2);
}

TEST_F(HotspotMetricsHelperTest, HotspotMaxClientCountHistogram) {
  PrepareEnableHotspotForTesting();
  hotspot_controller_->EnableHotspot(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  base::Value::Dict status_dict;
  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateActive);
  // Update tethering status with one active client.
  base::Value::List active_clients_list;
  base::Value::Dict client;
  client.Set(shill::kTetheringStatusClientIPv4Property, "IPV4:001");
  client.Set(shill::kTetheringStatusClientHostnameProperty, "hostname1");
  client.Set(shill::kTetheringStatusClientMACProperty, "persist");
  active_clients_list.Append(std::move(client));
  status_dict.Set(shill::kTetheringStatusClientsProperty,
                  active_clients_list.Clone());
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  hotspot_controller_->DisableHotspot(
      base::DoNothing(), hotspot_config::mojom::DisableReason::kUserInitiated);
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotMaxClientCount,
      /*sample=*/1, /*expected_count=*/1);

  SetHotspotStateInShill(shill::kTetheringStateIdle);
  // Verifies that the max client count is logged if hotspot is torn down by
  // internal error.
  hotspot_controller_->EnableHotspot(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  // Add one more connected client.
  base::Value::Dict client2;
  client2.Set(shill::kTetheringStatusClientIPv4Property, "IPV4:002");
  client2.Set(shill::kTetheringStatusClientHostnameProperty, "hostname2");
  active_clients_list.Append(std::move(client2));
  status_dict.Set(shill::kTetheringStatusClientsProperty,
                  std::move(active_clients_list));
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateIdle);
  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonError);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotMaxClientCount,
      /*sample=*/2, /*expected_count=*/1);
}

TEST_F(HotspotMetricsHelperTest, HotspotIsDeviceManagedHistogram) {
  PrepareEnableHotspotForTesting();
  hotspot_controller_->EnableHotspot(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotIsDeviceManaged, false,
      /*expected_count=*/1);
  hotspot_controller_->DisableHotspot(
      base::DoNothing(), hotspot_config::mojom::DisableReason::kUserInitiated);
  base::RunLoop().RunUntilIdle();

  hotspot_metrics_helper_->set_is_enterprise_managed(
      /*is_enterprise_managed=*/true);
  hotspot_controller_->EnableHotspot(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotIsDeviceManaged, true,
      /*expected_count=*/1);
}

TEST_F(HotspotMetricsHelperTest, HotspotEnabledUpstreamStatusHistogram) {
  ShillServiceClient::TestInterface* service_test =
      network_state_test_helper_.service_test();
  service_test->AddService(kCellularServicePath, kCellularServiceGuid,
                           kCellularServiceName, shill::kTypeCellular,
                           shill::kStateOnline, /*visible=*/true);
  PrepareEnableHotspotForTesting();
  hotspot_controller_->EnableHotspot(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotUpstreamStatusWhenEnabled,
      HotspotMetricsHelper::HotspotMetricsUpstreamStatus::
          kWifiWithCellularConnected,
      /*expected_count=*/1);
  hotspot_controller_->DisableHotspot(
      base::DoNothing(), hotspot_config::mojom::DisableReason::kUserInitiated);
  base::RunLoop().RunUntilIdle();

  // Bring the cellular network down.
  service_test->RemoveService(kCellularServicePath);
  base::RunLoop().RunUntilIdle();
  hotspot_controller_->EnableHotspot(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotUpstreamStatusWhenEnabled,
      HotspotMetricsHelper::HotspotMetricsUpstreamStatus::
          kWifiWithCellularNotConnected,
      /*expected_count=*/1);
}

TEST_F(HotspotMetricsHelperTest, HotspotDisableReasonHistogram) {
  PrepareEnableHotspotForTesting();
  hotspot_controller_->EnableHotspot(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  SetHotspotStateInShill(shill::kTetheringStateActive);
  hotspot_controller_->DisableHotspot(
      base::DoNothing(), hotspot_config::mojom::DisableReason::kUserInitiated);
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotDisableReasonHistogram,
      HotspotMetricsHelper::HotspotMetricsDisableReason::kUserInitiated, 1);

  SetHotspotStateInShill(shill::kTetheringStateIdle);
  // Verifies that the disabel reason is logged if hotspot is torn down by
  // internal error.
  base::Value::Dict status_dict;
  status_dict.Set(shill::kTetheringStatusStateProperty,
                  shill::kTetheringStateIdle);
  status_dict.Set(shill::kTetheringStatusIdleReasonProperty,
                  shill::kTetheringIdleReasonError);
  network_state_test_helper_.manager_test()->SetManagerProperty(
      shill::kTetheringStatusProperty, base::Value(status_dict.Clone()));
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      HotspotMetricsHelper::kHotspotDisableReasonHistogram,
      HotspotMetricsHelper::HotspotMetricsDisableReason::kInternalError, 1);
}

}  // namespace ash
