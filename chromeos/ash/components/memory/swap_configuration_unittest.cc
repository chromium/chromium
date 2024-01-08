// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/swap_configuration.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace memory {

namespace {
class SwapConfigurationPressureThreshold : public testing::Test {
 public:
  void SetUp() override {
    resourced_client_ = ResourcedClient::InitializeFake();
  }

  void TearDown() override { ResourcedClient::Shutdown(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<FakeResourcedClient, DanglingUntriaged> resourced_client_ = nullptr;
};

}  // namespace

TEST_F(SwapConfigurationPressureThreshold, NoArcDefault) {
  feature_list_.InitAndEnableFeature(kCrOSMemoryPressureSignalStudyNonArc);
  ConfigureSwap(/*arc_enabled=*/false);

  EXPECT_EQ(resourced_client_->get_critical_margin_bps(), 1500u);
  EXPECT_EQ(resourced_client_->get_moderate_margin_bps(), 4000u);
}

TEST_F(SwapConfigurationPressureThreshold, NoArcCustom) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kCrOSMemoryPressureSignalStudyNonArc,
      {{kCrOSMemoryPressureSignalStudyNonArcCriticalBps.name, "1500"},
       {kCrOSMemoryPressureSignalStudyNonArcModerateBps.name, "6000"}});
  ConfigureSwap(/*arc_enabled=*/false);

  EXPECT_EQ(resourced_client_->get_critical_margin_bps(), 1500u);
  EXPECT_EQ(resourced_client_->get_moderate_margin_bps(), 6000u);
}

TEST_F(SwapConfigurationPressureThreshold, ArcDefault) {
  feature_list_.InitAndEnableFeature(kCrOSMemoryPressureSignalStudyArc);
  ConfigureSwap(/*arc_enabled=*/true);

  EXPECT_EQ(resourced_client_->get_critical_margin_bps(), 800u);
  EXPECT_EQ(resourced_client_->get_moderate_margin_bps(), 4000u);
}

TEST_F(SwapConfigurationPressureThreshold, ArcCustom) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kCrOSMemoryPressureSignalStudyArc,
      {{kCrOSMemoryPressureSignalStudyArcCriticalBps.name, "1000"},
       {kCrOSMemoryPressureSignalStudyArcModerateBps.name, "6000"}});
  ConfigureSwap(/*arc_enabled=*/true);

  EXPECT_EQ(resourced_client_->get_critical_margin_bps(), 1000u);
  EXPECT_EQ(resourced_client_->get_moderate_margin_bps(), 6000u);
}

}  // namespace memory
}  // namespace ash
