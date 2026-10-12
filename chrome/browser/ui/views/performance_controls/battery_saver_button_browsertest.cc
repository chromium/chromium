// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/performance_controls/test_support/battery_saver_browser_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

class BatterySaverHelpPromoTest
    : public BatterySaverBrowserTestMixin<InteractiveFeaturePromoTest> {
 public:
  BatterySaverHelpPromoTest()
      : BatterySaverBrowserTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHBatterySaverModeFeature})) {}
  ~BatterySaverHelpPromoTest() override = default;
};

// Check if the battery saver in-product help promo is shown when the mode is
// first activated and confirm it is dismissed when the button is clicked.
IN_PROC_BROWSER_TEST_F(BatterySaverHelpPromoTest, ShowPromoOnModeActivation) {
  RunTestSequence(
      Do([this]() { SetBatterySaverModeEnabled(true); }),
      WaitForPromo(feature_engagement::kIPHBatterySaverModeFeature),
      PressButton(kToolbarBatterySaverButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

// Check if the battery saver in-product help promo is closed if the promo is
// active when the mode is deactivated.
IN_PROC_BROWSER_TEST_F(BatterySaverHelpPromoTest, HidePromoOnModeDeactivation) {
  RunTestSequence(
      Do([this]() { SetBatterySaverModeEnabled(true); }),
      WaitForPromo(feature_engagement::kIPHBatterySaverModeFeature),
      Do([this]() { SetBatterySaverModeEnabled(false); }),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

// Confirm that the navigation to the performance settings page happens when
// custom action button for battery saver promo bubble is clicked.
IN_PROC_BROWSER_TEST_F(BatterySaverHelpPromoTest, PromoCustomActionClicked) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
  RunTestSequence(
      InstrumentTab(kWebContentsId, 0),
      Do([this]() { SetBatterySaverModeEnabled(true); }),
      WaitForPromo(feature_engagement::kIPHBatterySaverModeFeature),
      PressNonDefaultPromoButton(),
      WaitForWebContentsNavigation(
          kWebContentsId,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))));
}

class BatterySaverBubbleViewTest
    : public BatterySaverBrowserTestMixin<InteractiveBrowserTest> {
 public:
  BatterySaverBubbleViewTest() = default;
  ~BatterySaverBubbleViewTest() override = default;
};

// Disable the battery saver mode for the session using the battery saver
// bubble dialog that is launched from the battery saver toolbar button.
IN_PROC_BROWSER_TEST_F(BatterySaverBubbleViewTest, DisableModeForSession) {
  auto* manager =
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance();

  EXPECT_FALSE(manager->IsBatterySaverModeDisabledForSession());

  RunTestSequence(
      Do([this]() { SetBatterySaverModeEnabled(true); }),
      WaitForShow(kToolbarBatterySaverButtonElementId),
      PressButton(kToolbarBatterySaverButtonElementId),
      InAnyContext(WaitForShow(kToolbarBatterySaverBubbleElementId)),
      InAnyContext(WithView(kToolbarBatterySaverBubbleElementId,
                            [](views::View* view) {
                              view->GetWidget()
                                  ->widget_delegate()
                                  ->AsDialogDelegate()
                                  ->CancelDialog();
                            })),
      InAnyContext(WaitForHide(kToolbarBatterySaverBubbleElementId)),
      CheckResult(
          [manager]() {
            return manager->IsBatterySaverModeDisabledForSession();
          },
          true));
}
