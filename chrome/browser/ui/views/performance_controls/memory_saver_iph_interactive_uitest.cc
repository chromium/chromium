// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);

class MemorySaverIphUiTest : public InteractiveFeaturePromoTest {
 public:
  MemorySaverIphUiTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHMemorySaverModeFeature})) {}
  ~MemorySaverIphUiTest() override = default;

  auto TriggerMemorySaverPromo() {
    auto steps = Steps(
        // Ensure that the primary tab has completed loading.
        InstrumentTab(kPrimaryTabId),
        // Load a bunch of tabs in the background.
        Do([this]() {
          constexpr int kTabCountThreshold = 10;
          for (int i = 0; i < kTabCountThreshold; i++) {
            NavigateParams params(browser(),
                                  GURL("about:blank"),
                                  ui::PAGE_TRANSITION_LINK);
            params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
            Navigate(&params);
          }
        }),
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
    AddDescription(steps, "TriggerMemorySaverPromo( %s )");
    return steps;
  }
};

// Check that the memory saver mode in-product help promo is shown when
// a tab threshold is reached and dismisses correctly when the app menu
// button is pushed.
IN_PROC_BROWSER_TEST_F(MemorySaverIphUiTest, ShowPromoOnTabThreshold) {
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
      TriggerMemorySaverPromo(), PressDefaultPromoButton(),
      CheckResult([manager]() { return manager->IsMemorySaverModeDefault(); },
                  false),
      CheckResult([manager]() { return manager->IsMemorySaverModeActive(); },
                  true));
}

// Check that the performance menu item is alerted when the memory saver
// promo is shown and the app menu button is clicked
IN_PROC_BROWSER_TEST_F(MemorySaverIphUiTest, AlertMenuItemWhenPromoShown) {
  RunTestSequence(
      TriggerMemorySaverPromo(),
      // This is required because normally this would happen when the button is
      // pressed, but pages loading in the background can cause the render view
      // to be focused, which can cause focus to pop back to the web view while
      // the app menu is trying to show, which can in turn cause the menu to
      // close.
      ActivateSurface(kBrowserViewElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kMoreToolsMenuItem),
      CheckViewProperty(AppMenuModel::kMoreToolsMenuItem,
                        &views::MenuItemView::is_alerted, true),
      SelectMenuItem(AppMenuModel::kMoreToolsMenuItem),
      WaitForShow(ToolsMenuModel::kPerformanceMenuItem),
      CheckViewProperty(ToolsMenuModel::kPerformanceMenuItem,
                        &views::MenuItemView::is_alerted, true));
}
