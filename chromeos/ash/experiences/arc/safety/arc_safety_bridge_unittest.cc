// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/safety/arc_safety_bridge.h"

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcSafetyBridgeTest : public testing::Test {
 protected:
  ArcSafetyBridgeTest()
      : bridge_(ArcSafetyBridge::GetForBrowserContextForTesting(&context_)) {}
  ArcSafetyBridgeTest(const ArcSafetyBridgeTest&) = delete;
  ArcSafetyBridgeTest& operator=(const ArcSafetyBridgeTest&) = delete;
  ~ArcSafetyBridgeTest() override = default;

  ArcSafetyBridge* bridge() { return bridge_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  user_prefs::TestBrowserContextWithPrefs context_;
  const raw_ptr<ArcSafetyBridge> bridge_;
};

TEST_F(ArcSafetyBridgeTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

TEST_F(ArcSafetyBridgeTest, CrosSafetyServiceDisabledByDefault) {
  base::test::TestFuture<bool> future;
  bridge()->IsCrosSafetyServiceEnabled(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ArcSafetyBridgeTest, CrosSafetyServiceEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kCrosSafetyService);
  base::test::TestFuture<bool> future;
  bridge()->IsCrosSafetyServiceEnabled(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

}  // namespace
}  // namespace arc
