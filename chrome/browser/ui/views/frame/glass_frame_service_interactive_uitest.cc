// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_frame_service.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/views/view_utils.h"

class GlassFrameServiceInteractiveTest : public InProcessBrowserTest {
 public:
  GlassFrameServiceInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlassFrame, tabs::kVerticalTabs}, {});
  }

  bool GlassFrameEligibilityMatchesTabStrip(BrowserWindowInterface* browser) {
    bool is_eligible =
        GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser);
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    TabStripRegionView* tab_strip_region_view = browser_view->tab_strip_view();

    if (auto* base_region =
            views::AsViewClass<BaseTabStripRegionView>(tab_strip_region_view)) {
      if (TabStripCollectionController* controller =
              base_region->GetTabStripCollectionController()) {
        return controller->IsGlassFrame() == is_eligible;
      }
    }

    if (TabStrip* tab_strip = views::AsViewClass<TabStrip>(
            tab_strip_region_view->GetTabStripView())) {
      return tab_strip->IsGlassFrame() == is_eligible;
    }

    return false;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::UserDataFactory::ScopedOverride glass_frame_service_override_;
};

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, SingleWindowEligible) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  BrowserWindowInterface* const browser1 = browser();
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       ThreeWindowsActivationSwap) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  BrowserWindowInterface* const browser1 = browser();
  BrowserWindowInterface* const browser2 =
      CreateBrowser(browser()->GetProfile());
  BrowserWindowInterface* const browser3 =
      CreateBrowser(browser()->GetProfile());

  // Initially browser3 is active, so it should be eligible.
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser3));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser2));

  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser2));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser3));

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

  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser2));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser3));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       ThreeWindowsCloseMiddle) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  BrowserWindowInterface* const browser1 = browser();
  BrowserWindowInterface* const browser2 =
      CreateBrowser(browser()->GetProfile());
  BrowserWindowInterface* const browser3 =
      CreateBrowser(browser()->GetProfile());

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
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser3));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, CallbackNotified) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

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
  BrowserWindowInterface* const browser2 =
      CreateBrowser(browser()->GetProfile());
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
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser2));

  // Close window 1 (the currently active/eligible window).
  CloseBrowserSynchronously(browser1);

  // The remaining window (browser2) should become eligible.
  ASSERT_TRUE(base::test::RunUntil([&] { return browser2_eligible; }));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser2));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, LocalStatePref) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  PrefService* const local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);
  EXPECT_TRUE(local_state->GetBoolean(prefs::kGlassFrameEnabled));

  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();
  BrowserWindowInterface* const browser1 = browser();

  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser1));

  local_state->SetBoolean(prefs::kGlassFrameEnabled, false);
  EXPECT_FALSE(local_state->GetBoolean(prefs::kGlassFrameEnabled));
  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));

  local_state->SetBoolean(prefs::kGlassFrameEnabled, true);
  EXPECT_TRUE(local_state->GetBoolean(prefs::kGlassFrameEnabled));
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       SwitchTabOrientationPreservesGlassState) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  BrowserWindowInterface* const browser_window = browser();
  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser_window);
  ASSERT_TRUE(controller);

  // Initially in horizontal tabs mode and eligible for glass frame.
  GlassFrameService* glass_frame_service = GlassFrameService::GetInstance();
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));

  // Switch to vertical tabs mode.
  controller->SetVerticalTabsEnabled(true);
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_TRUE(controller->ShouldDisplayVerticalTabs());
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));

  // Switch back to horizontal tabs mode.
  controller->SetVerticalTabsEnabled(false);
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_FALSE(controller->ShouldDisplayVerticalTabs());
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       SwitchTabOrientationPreservesDisabledGlassState) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  PrefService* const local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);
  local_state->SetBoolean(prefs::kGlassFrameEnabled, false);

  BrowserWindowInterface* const browser_window = browser();
  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser_window);
  ASSERT_TRUE(controller);

  GlassFrameService* glass_frame_service = GlassFrameService::GetInstance();
  // Initially in horizontal tabs mode and not eligible for glass frame.
  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));

  // Switch to vertical tabs mode while glass frame is disabled.
  controller->SetVerticalTabsEnabled(true);
  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_TRUE(controller->ShouldDisplayVerticalTabs());
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));

  // Switch back to horizontal tabs mode while glass frame is disabled.
  controller->SetVerticalTabsEnabled(false);
  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_FALSE(controller->ShouldDisplayVerticalTabs());
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));
}

#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       GetInstanceDoesNotConstructService) {
  EXPECT_EQ(GlassFrameService::GetInstance(), nullptr);
}
#endif  // !BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, BatterySaverMode) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

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

  // Initially BSM is not active, so browser1 is eligible.
  EXPECT_TRUE(browser1_eligible);

  // Enable Battery Saver Mode.
  g_browser_process->local_state()->SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));

  // Wait until BSM is active. GlassFrameService should report browser1 as
  // ineligible.
  ASSERT_TRUE(base::test::RunUntil([&] { return !browser1_eligible; }));
  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser1));

  // Disable Battery Saver Mode.
  g_browser_process->local_state()->SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled));

  // Wait until BSM is inactive and browser1 is eligible again.
  ASSERT_TRUE(base::test::RunUntil([&] { return browser1_eligible; }));
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser1));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       DanglingBrowserWindowPointerOnActivation) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();
  BrowserWindowInterface* const browser1 = browser();

  // Allocate a browser pointer.
  BrowserWindowInterface* deleted_browser =
      CreateBrowser(browser()->GetProfile());

  // Register a callback referencing the browser pointer before it is destroyed.
  base::CallbackListSubscription sub =
      glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
          deleted_browser, base::BindRepeating([](bool is_eligible) {}));

  // Delete browser to simulate a destroyed window while subscription is active.
  CloseBrowserSynchronously(deleted_browser);

  // Trigger OnBrowserActivated to invoke callbacks_.Notify().
  glass_frame_service->OnBrowserActivated(browser1);
}
