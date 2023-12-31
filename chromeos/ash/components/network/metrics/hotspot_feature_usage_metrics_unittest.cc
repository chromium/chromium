// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/hotspot_feature_usage_metrics.h"

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/ash/components/network/enterprise_managed_metadata_store.h"
#include "chromeos/ash/components/network/hotspot_allowed_flag_handler.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_configuration_handler.h"
#include "chromeos/ash/components/network/hotspot_controller.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kHotspotFeatureUsage[] = "ChromeOS.FeatureUsage.Hotspot";

}  // namespace

class HotspotFeatureUsageMetricsTest : public testing::Test {
 public:
  HotspotFeatureUsageMetricsTest() = default;
  HotspotFeatureUsageMetricsTest(const HotspotFeatureUsageMetricsTest&) =
      delete;
  HotspotFeatureUsageMetricsTest& operator=(
      const HotspotFeatureUsageMetricsTest&) = delete;
  ~HotspotFeatureUsageMetricsTest() override = default;

  void SetUp() override {
    enterprise_managed_metadata_store_ =
        std::make_unique<EnterpriseManagedMetadataStore>();
    hotspot_capabilities_provider_ =
        std::make_unique<HotspotCapabilitiesProvider>();
    hotspot_allowed_flag_handler_ =
        std::make_unique<HotspotAllowedFlagHandler>();
    hotspot_capabilities_provider_->Init(
        network_state_test_helper_.network_state_handler(),
        hotspot_allowed_flag_handler_.get());
    hotspot_feature_usage_metrics_ =
        std::make_unique<HotspotFeatureUsageMetrics>();
    hotspot_feature_usage_metrics_->Init(
        enterprise_managed_metadata_store_.get(),
        hotspot_capabilities_provider_.get());

    base::RunLoop().RunUntilIdle();
  }

  void SetHotspotAllowStatus(
      hotspot_config::mojom::HotspotAllowStatus allow_status) {
    hotspot_capabilities_provider_->SetHotspotAllowStatus(allow_status);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
  std::unique_ptr<EnterpriseManagedMetadataStore>
      enterprise_managed_metadata_store_;
  std::unique_ptr<HotspotAllowedFlagHandler> hotspot_allowed_flag_handler_;
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
  std::unique_ptr<HotspotFeatureUsageMetrics> hotspot_feature_usage_metrics_;
};

TEST_F(HotspotFeatureUsageMetricsTest, FeatureUsageTest) {
  using hotspot_config::mojom::HotspotAllowStatus;

  SetHotspotAllowStatus(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoWiFiDownstream);
  task_environment_.FastForwardBy(
      feature_usage::FeatureUsageMetrics::kInitialInterval);
  EXPECT_FALSE(hotspot_feature_usage_metrics_->IsEligible());
  EXPECT_FALSE(hotspot_feature_usage_metrics_->IsAccessible());
  EXPECT_FALSE(hotspot_feature_usage_metrics_->IsEnabled());
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      0);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kAccessible),
      0);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 0);

  SetHotspotAllowStatus(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedByPolicy);
  task_environment_.FastForwardBy(
      feature_usage::FeatureUsageMetrics::kRepeatedInterval);
  EXPECT_TRUE(hotspot_feature_usage_metrics_->IsEligible());
  EXPECT_FALSE(hotspot_feature_usage_metrics_->IsAccessible());
  EXPECT_FALSE(hotspot_feature_usage_metrics_->IsEnabled());
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      1);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kAccessible),
      0);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 0);

  enterprise_managed_metadata_store_->set_is_enterprise_managed(
      /*is_enterprise_managed=*/true);
  task_environment_.FastForwardBy(
      feature_usage::FeatureUsageMetrics::kRepeatedInterval);
  EXPECT_TRUE(hotspot_feature_usage_metrics_->IsEligible());
  EXPECT_TRUE(hotspot_feature_usage_metrics_->IsAccessible());
  EXPECT_FALSE(*hotspot_feature_usage_metrics_->IsAccessible());
  EXPECT_FALSE(hotspot_feature_usage_metrics_->IsEnabled());
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      2);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kAccessible),
      0);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 0);

  SetHotspotAllowStatus(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoMobileData);
  task_environment_.FastForwardBy(
      feature_usage::FeatureUsageMetrics::kRepeatedInterval);
  EXPECT_TRUE(hotspot_feature_usage_metrics_->IsEligible());
  EXPECT_TRUE(*hotspot_feature_usage_metrics_->IsAccessible());
  EXPECT_FALSE(hotspot_feature_usage_metrics_->IsEnabled());
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      3);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kAccessible),
      1);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 0);

  SetHotspotAllowStatus(hotspot_config::mojom::HotspotAllowStatus::kAllowed);
  task_environment_.FastForwardBy(
      feature_usage::FeatureUsageMetrics::kRepeatedInterval);
  EXPECT_TRUE(hotspot_feature_usage_metrics_->IsEligible());
  EXPECT_TRUE(*hotspot_feature_usage_metrics_->IsAccessible());
  EXPECT_TRUE(hotspot_feature_usage_metrics_->IsEnabled());
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      4);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kAccessible),
      2);
  histogram_tester_.ExpectBucketCount(
      kHotspotFeatureUsage,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 1);
}

}  // namespace ash
