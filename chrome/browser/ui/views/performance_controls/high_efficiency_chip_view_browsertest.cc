// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_bubble_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

constexpr base::TimeDelta kShortDelay = base::Seconds(1);
}  // namespace

class HighEfficiencyChipViewBrowserTest : public InProcessBrowserTest {
 public:
  HighEfficiencyChipViewBrowserTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    // Start with a non-null TimeTicks, as there is no discard protection for
    // a tab with a null focused timestamp.
    test_clock_.Advance(kShortDelay);
  }

  ~HighEfficiencyChipViewBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{performance_manager::features::kHighEfficiencyModeAvailable,
          {{"default_state", "true"}, {"time_before_discard", "1h"}}}},
        {});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // To avoid flakes when focus changes, set the active tab strip model
    // explicitly.
    resource_coordinator::GetTabLifecycleUnitSource()
        ->SetFocusedTabStripModelForTesting(browser()->tab_strip_model());

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL test_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), test_url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
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

  views::StyledLabel* GetHighEfficiencyBubbleLabel() {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForWidget(
            GetHighEfficiencyChipView()->GetBubble()->anchor_widget());
    return views::ElementTrackerViews::GetInstance()
        ->GetFirstMatchingViewAs<views::StyledLabel>(
            HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId,
            context);
  }

  void ClickHighEfficiencyChip() {
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        GetHighEfficiencyChipView(),
        ui::test::InteractionTestUtil::InputType::kMouse);
  }

  void DiscardTabAt(int tab_index) {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(tab_index);
    performance_manager::user_tuning::UserPerformanceTuningManager* manager =
        performance_manager::user_tuning::UserPerformanceTuningManager::
            GetInstance();
    manager->DiscardPageForTesting(contents);
  }

  views::InkDropState GetInkDropState() {
    return views::InkDrop::Get(GetHighEfficiencyChipView())
        ->GetInkDrop()
        ->GetTargetInkDropState();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestTickClock test_clock_;
  resource_coordinator::ScopedSetTickClockForTesting
      scoped_set_tick_clock_for_testing_;
};

IN_PROC_BROWSER_TEST_F(HighEfficiencyChipViewBrowserTest,
                       ShowAndHideInkDropOnDialog) {
  PageActionIconView* chip = GetHighEfficiencyChipView();
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(chip);

  DiscardTabAt(0);
  chrome::SelectNumberedTab(browser(), 0);

  EXPECT_EQ(GetInkDropState(), views::InkDropState::HIDDEN);

  // Open bubble
  test_api.NotifyClick(press);

  EXPECT_EQ(GetInkDropState(), views::InkDropState::ACTIVATED);

  test_api.NotifyClick(press);

  views::InkDropState current_state = GetInkDropState();
  // The deactivated state is HIDDEN on Mac but DEACTIVATED on Linux.
  EXPECT_TRUE(current_state == views::InkDropState::HIDDEN ||
              current_state == views::InkDropState::DEACTIVATED);
}
