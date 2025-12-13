// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/chrome_feature_flags/arc_chrome_feature_flags_bridge.h"

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/experiences/arc/arc_features.h"
#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "chromeos/ash/experiences/arc/test/fake_chrome_feature_flags_instance.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcChromeFeatureFlagsBridgeTest : public testing::Test {
 protected:
  ArcChromeFeatureFlagsBridgeTest()
      : bridge_(ArcChromeFeatureFlagsBridge::GetForBrowserContextForTesting(
            &context_)) {}
  ArcChromeFeatureFlagsBridgeTest(const ArcChromeFeatureFlagsBridgeTest&) =
      delete;
  ArcChromeFeatureFlagsBridgeTest& operator=(
      const ArcChromeFeatureFlagsBridgeTest&) = delete;
  ~ArcChromeFeatureFlagsBridgeTest() override = default;

  void TearDown() override {
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->chrome_feature_flags()
        ->CloseInstance(&instance_);
    bridge_->Shutdown();
  }

  void Connect() {
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->chrome_feature_flags()
        ->SetInstance(&instance_);
  }

  ArcChromeFeatureFlagsBridge* bridge() { return bridge_; }
  FakeChromeFeatureFlagsInstance* instance() { return &instance_; }
  base::test::ScopedFeatureList* scoped_feature_list() {
    return &scoped_feature_list_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  user_prefs::TestBrowserContextWithPrefs context_;
  FakeChromeFeatureFlagsInstance instance_;
  base::test::ScopedFeatureList scoped_feature_list_;
  const raw_ptr<ArcChromeFeatureFlagsBridge> bridge_;
};

TEST_F(ArcChromeFeatureFlagsBridgeTest, ConstructDestruct) {
  Connect();
  EXPECT_NE(nullptr, bridge());
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyJelly) {
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->jelly_colors);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyRoundedWindowCompat) {
  Connect();
  EXPECT_EQ(instance()->flags_called_value()->rounded_window_compat_strategy,
            mojom::RoundedWindowCompatStrategy::kLeftRightBottomGesture);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, RoundedWindowsRadius) {
  Connect();
  EXPECT_EQ(instance()->flags_called_value()->rounded_window_radius,
            chromeos::kRoundedWindowCornerRadius);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyResizeCompat_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(arc::kResizeCompat);
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->resize_compat);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyResizeCompat_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(arc::kResizeCompat);
  Connect();
  EXPECT_FALSE(instance()->flags_called_value()->resize_compat);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyExtendIntentAnrTimeout_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(arc::kExtendIntentAnrTimeout);
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->extend_intent_anr_timeout);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyExtendIntentAnrTimeout_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(arc::kExtendIntentAnrTimeout);
  Connect();
  EXPECT_FALSE(instance()->flags_called_value()->extend_intent_anr_timeout);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyExtendServiceAnrTimeout_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(arc::kExtendServiceAnrTimeout);
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->extend_service_anr_timeout);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest,
       NotifyExtendServiceAnrTimeout_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(arc::kExtendServiceAnrTimeout);
  Connect();
  EXPECT_FALSE(instance()->flags_called_value()->extend_service_anr_timeout);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest,
       NotifyNotificationWidthIncrease_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(
      chromeos::features::kNotificationWidthIncrease);
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->notification_width_increase);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest,
       NotifyNotificationWidthIncrease_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(
      chromeos::features::kNotificationWidthIncrease);
  Connect();
  EXPECT_FALSE(instance()->flags_called_value()->notification_width_increase);
}

}  // namespace
}  // namespace arc
