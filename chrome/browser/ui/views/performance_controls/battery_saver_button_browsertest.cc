// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/battery_saver_button.h"

#include "base/power_monitor/battery_state_sampler.h"
#include "base/test/bind.h"
#include "base/test/power_monitor_test_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/performance_controls/test_support/battery_saver_browser_test_mixin.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/performance_controls/battery_saver_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

class BatterySaverHelpPromoTest
    : public BatterySaverBrowserTestMixin<InteractiveFeaturePromoTest> {
 public:
  BatterySaverHelpPromoTest()
      : BatterySaverBrowserTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHBatterySaverModeFeature})) {}
  ~BatterySaverHelpPromoTest() override = default;

  void PressButton(views::Button* button) {
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        button, ui::test::InteractionTestUtil::InputType::kMouse);
  }

  BrowserFeaturePromoController* GetFeaturePromoController() {
    return static_cast<BrowserFeaturePromoController*>(
        browser()->window()->GetFeaturePromoControllerForTesting());
  }
};

// Check if the battery saver in-product help promo is shown when the mode is
// first activated and confirm it is dismissed when the button is clicked.
IN_PROC_BROWSER_TEST_F(BatterySaverHelpPromoTest, ShowPromoOnModeActivation) {
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      user_education::HelpBubbleView::kViewClassName);
  SetBatterySaverModeEnabled(true);
  views::Widget* widget = waiter.WaitIfNeededAndGet();

  bool promo_active = GetFeaturePromoController()->IsPromoActive(
      feature_engagement::kIPHBatterySaverModeFeature);
  EXPECT_TRUE(promo_active);

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  views::View* const battery_saver_button_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kToolbarBatterySaverButtonElementId,
          browser()->window()->GetElementContext());
  PressButton(static_cast<views::Button*>(battery_saver_button_view));
  destroyed_waiter.Wait();
}

// Check if the battery saver in-product help promo is closed if the promo is
// active when the mode is deactivated.
IN_PROC_BROWSER_TEST_F(BatterySaverHelpPromoTest, HidePromoOnModeDeactivation) {
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      user_education::HelpBubbleView::kViewClassName);
  SetBatterySaverModeEnabled(true);
  views::Widget* widget = waiter.WaitIfNeededAndGet();

  bool promo_active = GetFeaturePromoController()->IsPromoActive(
      feature_engagement::kIPHBatterySaverModeFeature);
  EXPECT_TRUE(promo_active);

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  SetBatterySaverModeEnabled(false);
  destroyed_waiter.Wait();
}

// Confirm that the navigation to the performance settings page happens when
// custom action button for battery saver promo bubble is clicked.
IN_PROC_BROWSER_TEST_F(BatterySaverHelpPromoTest, PromoCustomActionClicked) {
  auto* const promo_controller = GetFeaturePromoController();
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      user_education::HelpBubbleView::kViewClassName);
  SetBatterySaverModeEnabled(true);
  waiter.WaitIfNeededAndGet();

  bool promo_active = promo_controller->IsPromoActive(
      feature_engagement::kIPHBatterySaverModeFeature);
  EXPECT_TRUE(promo_active);

  content::TestNavigationObserver navigation_observer(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  auto* promo_bubble = promo_controller->promo_bubble_for_testing()
                           ->AsA<user_education::HelpBubbleViews>()
                           ->bubble_view();
  auto* custom_action_button = promo_bubble->GetNonDefaultButtonForTesting(0);
  PressButton(custom_action_button);
  navigation_observer.Wait();

  GURL expected_url(chrome::GetSettingsUrl(chrome::kPerformanceSubPage));
  EXPECT_EQ(expected_url, navigation_observer.last_navigation_url());
}

class BatterySaverBubbleViewTest
    : public BatterySaverBrowserTestMixin<InProcessBrowserTest> {
 public:
  BatterySaverBubbleViewTest() = default;
  ~BatterySaverBubbleViewTest() override = default;

  BatterySaverButton* GetBatterySaverButton() {
    BatterySaverButton* battery_saver_button =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->battery_saver_button();
    return battery_saver_button;
  }

  void PressButton(views::Button* button) {
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        button, ui::test::InteractionTestUtil::InputType::kMouse);
  }
};

// Disable the battery saver mode for the session using the battery saver
// bubble dialog that is launched from the battery saver toolbar button.
IN_PROC_BROWSER_TEST_F(BatterySaverBubbleViewTest, DisableModeForSession) {
  SetBatterySaverModeEnabled(true);
  base::RunLoop().RunUntilIdle();

  auto* manager =
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance();

  bool is_disabled = manager->IsBatterySaverModeDisabledForSession();
  EXPECT_FALSE(is_disabled);

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       BatterySaverBubbleView::kViewClassName);

  BatterySaverButton* battery_saver_button = GetBatterySaverButton();
  PressButton(battery_saver_button);
  views::Widget* widget = waiter.WaitIfNeededAndGet();

  views::BubbleDialogModelHost* const bubble_dialog_host =
      battery_saver_button->GetBubble();
  ASSERT_NE(bubble_dialog_host, nullptr);

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  bubble_dialog_host->Cancel();
  destroyed_waiter.Wait();

  is_disabled = manager->IsBatterySaverModeDisabledForSession();
  EXPECT_TRUE(is_disabled);
}
