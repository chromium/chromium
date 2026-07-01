// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/glass_frame_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

class GlassFrameServiceInteractiveTest : public InProcessBrowserTest {
 public:
  GlassFrameServiceInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlassFrame);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, GetInstance) {
  EXPECT_TRUE(GlassFrameService::GetInstance());
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, SingleWindowEligible) {
  BrowserWindowInterface* const browser1 = browser();
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       ThreeWindowsActivationSwap) {
  BrowserWindowInterface* const browser1 = browser();
  BrowserWindowInterface* const browser2 = CreateBrowser(browser()->profile());
  BrowserWindowInterface* const browser3 = CreateBrowser(browser()->profile());

  // Initially browser3 is active, so it should be eligible.
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser3));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser2));

  // Activate window 1.
  browser1->GetWindow()->Activate();
  ASSERT_TRUE(base::test::RunUntil([&] {
    return GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1);
  }));

  // Now windows 2 and 3 shouldn't be eligible anymore.
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser2));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser3));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       ThreeWindowsCloseMiddle) {
  BrowserWindowInterface* const browser1 = browser();
  BrowserWindowInterface* const browser2 = CreateBrowser(browser()->profile());
  BrowserWindowInterface* const browser3 = CreateBrowser(browser()->profile());

  // Initially browser3 is active and eligible.
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser3));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser2));

  // Close window 2.
  CloseBrowserSynchronously(browser2->GetBrowserForMigrationOnly());

  // Window 3 should still be eligible, and window 1 is ineligible.
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser3));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
}
