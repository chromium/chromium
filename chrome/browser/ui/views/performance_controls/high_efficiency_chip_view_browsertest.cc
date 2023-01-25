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
#include "chrome/browser/ui/performance_controls/tab_discard_tab_helper.h"
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
#include "components/performance_manager/public/decorators/process_metrics_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

constexpr base::TimeDelta kShortDelay = base::Seconds(1);

class QuitRunLoopOnMemoryMetricsRefreshObserver
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          Observer {
 public:
  explicit QuitRunLoopOnMemoryMetricsRefreshObserver(
      base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  ~QuitRunLoopOnMemoryMetricsRefreshObserver() override = default;

  void OnMemoryMetricsRefreshed() override { std::move(quit_closure_).Run(); }

 private:
  base::OnceClosure quit_closure_;
};
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
    iph_features_.InitForDemo(
        feature_engagement::kIPHHighEfficiencyInfoModeFeature,
        {{performance_manager::features::kHighEfficiencyModeAvailable,
          {{"default_state", "true"}, {"time_before_discard", "1h"}}}});

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

  void ForceRefreshMemoryMetricsForTesting() {
    performance_manager::user_tuning::UserPerformanceTuningManager* manager =
        performance_manager::user_tuning::UserPerformanceTuningManager::
            GetInstance();

    base::RunLoop run_loop;
    QuitRunLoopOnMemoryMetricsRefreshObserver observer(run_loop.QuitClosure());
    base::ScopedObservation<
        performance_manager::user_tuning::UserPerformanceTuningManager,
        QuitRunLoopOnMemoryMetricsRefreshObserver>
        memory_metrics_observer(&observer);
    memory_metrics_observer.Observe(manager);

    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindLambdaForTesting([](performance_manager::Graph* graph) {
          auto* metrics_decorator = graph->GetRegisteredObjectAs<
              performance_manager::ProcessMetricsDecorator>();
          metrics_decorator->RefreshMetricsForTesting();
        }));

    run_loop.Run();
  }

 private:
  feature_engagement::test::ScopedIphFeatureList iph_features_;
  base::SimpleTestTickClock test_clock_;
  resource_coordinator::ScopedSetTickClockForTesting
      scoped_set_tick_clock_for_testing_;
};

IN_PROC_BROWSER_TEST_F(HighEfficiencyChipViewBrowserTest,
                       NavigatesOnIPHSettingsLinkClicked) {
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  EXPECT_FALSE(GetFeaturePromoController()->IsPromoActive(
      feature_engagement::kIPHHighEfficiencyInfoModeFeature));

  DiscardTabAt(0);
  chrome::SelectNumberedTab(browser(), 0);
  WaitForIPHToShow();

  EXPECT_TRUE(GetFeaturePromoController()->IsPromoActive(
      feature_engagement::kIPHHighEfficiencyInfoModeFeature));

  ClickIPHSettingsButton();
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(3, tab_strip_model->count());
  content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(2);
  WaitForLoadStop(web_contents);
  GURL expected(chrome::kChromeUIPerformanceSettingsURL);
  EXPECT_EQ(expected.host(), web_contents->GetLastCommittedURL().host());
}

IN_PROC_BROWSER_TEST_F(HighEfficiencyChipViewBrowserTest,
                       PromoDismissesOnCancelClick) {
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  DiscardTabAt(0);
  chrome::SelectNumberedTab(browser(), 0);
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

  DiscardTabAt(0);
  chrome::SelectNumberedTab(browser(), 0);
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

IN_PROC_BROWSER_TEST_F(HighEfficiencyChipViewBrowserTest,
                       BubbleCorrectlyReportingMemorySaved) {
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  ForceRefreshMemoryMetricsForTesting();
  DiscardTabAt(0);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  auto* pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::FromWebContents(web_contents);
  int memory_footprint_estimate =
      pre_discard_resource_usage->memory_footprint_estimate_kb();
  chrome::SelectNumberedTab(browser(), 0);
  WaitForIPHToShow();
  ClickHighEfficiencyChip();
  views::StyledLabel* label = GetHighEfficiencyBubbleLabel();

  EXPECT_NE(
      label->GetText().find(ui::FormatBytes(memory_footprint_estimate * 1024)),
      std::string::npos);
}
