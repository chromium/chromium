// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class BirchBrowserTest : public InProcessBrowserTest {
 public:
  BirchBrowserTest() = default;
  ~BirchBrowserTest() override = default;
  BirchBrowserTest(const BirchBrowserTest&) = delete;
  BirchBrowserTest& operator=(const BirchBrowserTest&) = delete;

  void SetUp() override {
    switches::SetIgnoreForestSecretKeyForTest(true);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

  BirchKeyedService* GetBirchKeyedService() {
    return BirchKeyedServiceFactory::GetInstance()->GetService(
        browser()->profile());
  }

 protected:
  base::test::ScopedFeatureList feature_list_{features::kForestFeature};
};

// Test that no crash occurs on shutdown with an instantiated BirchKeyedService.
IN_PROC_BROWSER_TEST_F(BirchBrowserTest, NoCrashOnShutdown) {
  auto* birch_keyed_service = GetBirchKeyedService();
  EXPECT_TRUE(birch_keyed_service);
}

}  // namespace ash
