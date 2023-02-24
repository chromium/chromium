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
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

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
    hotspot_metrics_helper_ = std::make_unique<HotspotMetricsHelper>();
    hotspot_metrics_helper_->Init(hotspot_capabilities_provider_.get());

    base::RunLoop().RunUntilIdle();
  }

  void SetHotspotAllowStatus(
      hotspot_config::mojom::HotspotAllowStatus allow_status) {
    hotspot_capabilities_provider_->SetHotspotAllowStatus(allow_status);
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    hotspot_capabilities_provider_.reset();
    hotspot_metrics_helper_.reset();
    LoginState::Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
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

}  // namespace ash
