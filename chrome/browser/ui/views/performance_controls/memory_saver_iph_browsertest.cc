// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
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
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

class MemorySaverHelpPromoTest : public InteractiveFeaturePromoTest {
 public:
  MemorySaverHelpPromoTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHMemorySaverModeFeature})) {}
  ~MemorySaverHelpPromoTest() override = default;

  auto TriggerMemorySaverPromo() {
    auto steps = Steps(
        Do([this]() {
          constexpr int kTabCountThreshold = 10;
          for (int i = 0; i < kTabCountThreshold; i++) {
            chrome::AddTabAt(browser(), GURL(), i, true);
          }
        }),
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        FlushEvents());
    AddDescription(steps, "TriggerMemorySaverPromo( %s )");
    return steps;
  }
};

// Check that the memory saver mode in-product help promo is shown when
// a tab threshold is reached and dismisses correctly when the app menu
// button is pushed.
IN_PROC_BROWSER_TEST_F(MemorySaverHelpPromoTest, ShowPromoOnTabThreshold) {
  RunTestSequence(
      TriggerMemorySaverPromo(), PressButton(kToolbarAppMenuButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

// Confirm that Memory Saver mode is enabled when the custom action
// button for memory saver mode is clicked
IN_PROC_BROWSER_TEST_F(MemorySaverHelpPromoTest, PromoCustomActionClicked) {
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
IN_PROC_BROWSER_TEST_F(MemorySaverHelpPromoTest, AlertMenuItemWhenPromoShown) {
  RunTestSequence(
      TriggerMemorySaverPromo(), PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kMoreToolsMenuItem),
      CheckView(kBrowserViewElementId, [](BrowserView* browser_view) {
        AppMenuModel* const app_menu_model =
            browser_view->toolbar()->app_menu_button()->app_menu_model();
        const auto index =
            app_menu_model->GetIndexOfCommandId(IDC_MORE_TOOLS_MENU);
        if (!app_menu_model->IsAlertedAt(index.value())) {
          LOG(ERROR) << "More tools not alerted.";
          return false;
        }

        ToolsMenuModel toolModel(app_menu_model, browser_view->browser());
        const size_t performance_index =
            toolModel.GetIndexOfCommandId(IDC_PERFORMANCE).value();
        return toolModel.IsAlertedAt(performance_index);
      }));
}
