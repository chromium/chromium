// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/test_support/app_menu_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);

class MemorySaverIphUiTest : public InteractiveFeaturePromoTest {
 public:
  MemorySaverIphUiTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHMemorySaverModeFeature})) {}
  ~MemorySaverIphUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    WaitForInitialWebUIToolbar(browser());
  }

  auto TriggerMemorySaverPromo() {
    auto steps = Steps(
        // Ensure that the primary tab has completed loading.
        InstrumentTab(kPrimaryTabId),
        // Load a bunch of tabs in the background.
        Do([this]() {
          constexpr int kTabCountThreshold = 10;
          for (int i = 0; i < kTabCountThreshold; i++) {
            NavigateParams params(browser(), GURL("about:blank"),
                                  ui::PAGE_TRANSITION_LINK);
            params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
            Navigate(&params);
          }
        }),
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
    AddDescriptionPrefix(steps, "TriggerMemorySaverPromo()");
    return steps;
  }

 private:
  // Pretend to have 64GB of memory by default, so background memory threshold
  // notifications never prematurely trigger the promo during test execution.
  base::test::ScopedAmountOfPhysicalMemoryOverride
      scoped_amount_of_physical_memory_override_{base::GiB(64)};
};

// Check that the memory saver mode in-product help promo is shown when
// a tab threshold is reached and dismisses correctly when the app menu
// button is pushed.
IN_PROC_BROWSER_TEST_F(MemorySaverIphUiTest, ShowPromoOnTabThreshold) {
  // Override to 8GB so the device is below the 16GB cap for promo eligibility.
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiB(8));
  RunTestSequence(
      TriggerMemorySaverPromo(), PressButton(kToolbarAppMenuButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

// Confirm that Memory Saver mode is enabled when the custom action
// button for memory saver mode is clicked
IN_PROC_BROWSER_TEST_F(MemorySaverIphUiTest, PromoCustomActionClicked) {
  auto* const manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  RunTestSequence(
      CheckResult([manager]() { return manager->IsMemorySaverModeDefault(); },
                  true),
      CheckResult([manager]() { return manager->IsMemorySaverModeActive(); },
                  false),
      MaybeShowPromo(feature_engagement::kIPHMemorySaverModeFeature),
      PressDefaultPromoButton(),
      CheckResult([manager]() { return manager->IsMemorySaverModeDefault(); },
                  false),
      CheckResult([manager]() { return manager->IsMemorySaverModeActive(); },
                  true));
}

// Check that the performance menu item is alerted when the memory saver
// promo is shown and the app menu button is clicked
IN_PROC_BROWSER_TEST_F(MemorySaverIphUiTest, AlertMenuItemWhenPromoShown) {
  RunTestSequence(
      MaybeShowPromo(feature_engagement::kIPHMemorySaverModeFeature),
      PressButton(kToolbarAppMenuButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckResult(
          [this]() {
            return AppMenuTestAccessor(browser()).IsElementIdAlerted(
                ToolsMenuModel::kPerformanceMenuItem);
          },
          true));
}
