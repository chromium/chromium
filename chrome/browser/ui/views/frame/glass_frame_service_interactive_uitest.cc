// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
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
  CloseBrowserSynchronously(browser2);

  // Window 3 should still be eligible, and window 1 is ineligible.
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser3));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, CallbackNotified) {
  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();
  BrowserWindowInterface* const browser1 = browser();

  bool browser1_eligible =
      glass_frame_service->IsBrowserWindowEligible(browser1);
  base::CallbackListSubscription sub1 =
      glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
          browser1, base::BindRepeating(
                        [](bool* out_eligible, bool is_eligible) {
                          *out_eligible = is_eligible;
                        },
                        &browser1_eligible));

  EXPECT_TRUE(browser1_eligible);

  // Create a second browser, which becomes the active and eligible browser.
  BrowserWindowInterface* const browser2 = CreateBrowser(browser()->profile());
  bool browser2_eligible =
      glass_frame_service->IsBrowserWindowEligible(browser2);
  base::CallbackListSubscription sub2 =
      glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
          browser2, base::BindRepeating(
                        [](bool* out_eligible, bool is_eligible) {
                          *out_eligible = is_eligible;
                        },
                        &browser2_eligible));

  // Wait for the new browser to be eligible. The callback should be notified.
  ASSERT_TRUE(base::test::RunUntil([&] { return browser2_eligible; }));
  EXPECT_FALSE(browser1_eligible);

  // Activate window 1.
  browser1->GetWindow()->Activate();
  ASSERT_TRUE(base::test::RunUntil([&] { return browser1_eligible; }));
  EXPECT_FALSE(browser2_eligible);

  // Close window 1 (the currently active/eligible window).
  CloseBrowserSynchronously(browser1);

  // The remaining window (browser2) should become eligible.
  ASSERT_TRUE(base::test::RunUntil([&] { return browser2_eligible; }));
}
