// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/tab_discard_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

class DiscardMockNavigationHandle : public content::MockNavigationHandle {
 public:
  void SetWasDiscarded(bool was_discarded) { was_discarded_ = was_discarded; }
  bool ExistingDocumentWasDiscarded() const override { return was_discarded_; }

 private:
  bool was_discarded_ = false;
};

class HighEfficiencyChipViewBrowserTest : public InProcessBrowserTest {
 public:
  HighEfficiencyChipViewBrowserTest() = default;
  ~HighEfficiencyChipViewBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{feature_engagement::kIPHHighEfficiencyInfoModeFeature, {}},
         {performance_manager::features::kHighEfficiencyModeAvailable,
          {{"default_state", "true"}, {"time_before_discard", "5s"}}}},
        {});

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override { InProcessBrowserTest::TearDown(); }

  BrowserFeaturePromoController* GetFeaturePromoController() {
    auto* promo_controller = static_cast<BrowserFeaturePromoController*>(
        browser()->window()->GetFeaturePromoController());
    return promo_controller;
  }

  PageActionIconView* GetHighEfficiencyChipView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetLocationBarView()
        ->page_action_icon_controller()
        ->GetIconView(PageActionIconType::kHighEfficiency);
  }

  void ClickHighEfficiencyChip() {
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        GetHighEfficiencyChipView(),
        ui::test::InteractionTestUtil::InputType::kMouse);
  }

  void SetTabDiscardState(bool is_discarded) {
    TabDiscardTabHelper* tab_helper = TabDiscardTabHelper::FromWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(0));
    std::unique_ptr<DiscardMockNavigationHandle> navigation_handle =
        std::make_unique<DiscardMockNavigationHandle>();
    navigation_handle.get()->SetWasDiscarded(is_discarded);
    tab_helper->DidStartNavigation(navigation_handle.get());

    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    browser_view->GetLocationBarView()
        ->page_action_icon_controller()
        ->UpdateAll();
  }

  void WaitForIPHToShow() {
    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        user_education::HelpBubbleView::kViewClassName);
    waiter.WaitIfNeededAndGet();
  }

  user_education::HelpBubbleView* GetHelpBubbleView() {
    return GetFeaturePromoController()
        ->promo_bubble_for_testing()
        ->AsA<user_education::HelpBubbleViews>()
        ->bubble_view();
  }

  void ClickIPHCancelButton() {
    views::test::WidgetDestroyedWaiter waiter(GetHelpBubbleView()->GetWidget());
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        GetHelpBubbleView()->GetDefaultButtonForTesting(),
        ui::test::InteractionTestUtil::InputType::kMouse);
    waiter.Wait();
  }

  void ClickIPHSettingsButton() {
    views::test::WidgetDestroyedWaiter waiter(GetHelpBubbleView()->GetWidget());
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        GetHelpBubbleView()->GetNonDefaultButtonForTesting(0),
        ui::test::InteractionTestUtil::InputType::kMouse);
    waiter.Wait();
  }

  views::InkDropState GetInkDropState() {
    return views::InkDrop::Get(GetHighEfficiencyChipView())
        ->GetInkDrop()
        ->GetTargetInkDropState();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(HighEfficiencyChipViewBrowserTest,
                       NavigatesOnIPHSettingsLinkClicked) {
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();

  EXPECT_FALSE(GetFeaturePromoController()->IsPromoActive(
      feature_engagement::kIPHHighEfficiencyInfoModeFeature));

  SetTabDiscardState(true);
  WaitForIPHToShow();

  EXPECT_TRUE(GetFeaturePromoController()->IsPromoActive(
      feature_engagement::kIPHHighEfficiencyInfoModeFeature));

  content::TestNavigationObserver navigation_observer(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  ClickIPHSettingsButton();
  navigation_observer.Wait();

  GURL expected(chrome::kChromeUIPerformanceSettingsURL);
  EXPECT_EQ(expected.host(), navigation_observer.last_navigation_url().host());
}

IN_PROC_BROWSER_TEST_F(HighEfficiencyChipViewBrowserTest,
                       PromoDismissesOnCancelClick) {
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();

  SetTabDiscardState(true);
  WaitForIPHToShow();

  EXPECT_TRUE(GetFeaturePromoController()->IsPromoActive(
      feature_engagement::kIPHHighEfficiencyInfoModeFeature));

  ClickHighEfficiencyChip();

  // Expect the bubble to be open and the promo to be closed.
  EXPECT_FALSE(GetFeaturePromoController()->IsPromoActive(
      feature_engagement::kIPHHighEfficiencyInfoModeFeature));
  EXPECT_NE(GetHighEfficiencyChipView()->GetBubble(), nullptr);
}

IN_PROC_BROWSER_TEST_F(HighEfficiencyChipViewBrowserTest,
                       ShowAndHideInkDropWithPromo) {
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();

  EXPECT_FALSE(GetFeaturePromoController()->IsPromoActive(
      feature_engagement::kIPHHighEfficiencyInfoModeFeature));

  SetTabDiscardState(true);
  WaitForIPHToShow();

  EXPECT_TRUE(GetFeaturePromoController()->IsPromoActive(
      feature_engagement::kIPHHighEfficiencyInfoModeFeature));
  EXPECT_EQ(GetInkDropState(), views::InkDropState::ACTIVATED);

  ClickIPHCancelButton();

  EXPECT_FALSE(GetFeaturePromoController()->IsPromoActive(
      feature_engagement::kIPHHighEfficiencyInfoModeFeature));
  views::InkDropState current_state = GetInkDropState();
  // The deactivated state is HIDDEN on Mac but DEACTIVATED on Linux.
  EXPECT_TRUE(current_state == views::InkDropState::HIDDEN ||
              current_state == views::InkDropState::DEACTIVATED);
}
