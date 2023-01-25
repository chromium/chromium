// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_bubble_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/performance_manager/public/features.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPerformanceSettingsTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsInteractionTestUtilTestId);

constexpr base::TimeDelta kShortDelay = base::Seconds(1);

}  // namespace

class HighEfficiencyChipInteractiveTest : public InteractiveBrowserTest {
 public:
  HighEfficiencyChipInteractiveTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    // Start with a non-null TimeTicks, as there is no discard protection for
    // a tab with a null focused timestamp.
    test_clock_.Advance(kShortDelay);
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{performance_manager::features::kHighEfficiencyModeAvailable,
          {{"default_state", "true"}, {"time_before_discard", "30s"}}}},
        {});

    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    tab_strip_model_ = browser()->tab_strip_model();
    test_url_ = embedded_test_server()->GetURL("a.com", "/title1.html");
    util_ = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
        browser(), kWebContentsInteractionTestUtilTestId);

    // To avoid flakes when focus changes, set the active tab strip model
    // explicitly.
    resource_coordinator::GetTabLifecycleUnitSource()
        ->SetFocusedTabStripModelForTesting(tab_strip_model_);
  }

  void TearDownOnMainThread() override {
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* GetWebContentsAt(int index) {
    return tab_strip_model_->GetWebContentsAt(index);
  }

  PageActionIconView* GetPageActionIconView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->GetIconView(PageActionIconType::kHighEfficiency);
  }

  bool IsTabDiscarded(int tab_index) {
    return resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
               GetWebContentsAt(tab_index))
        ->IsDiscarded();
  }

  auto DiscardTab(int discard_tab_index) {
    return Do(base::BindLambdaForTesting([=]() {
      EXPECT_NE(discard_tab_index, tab_strip_model_->active_index());
      EXPECT_FALSE(IsTabDiscarded(discard_tab_index));
      resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
          GetWebContentsAt(discard_tab_index))
          ->DiscardTab(LifecycleUnitDiscardReason::EXTERNAL);
      EXPECT_TRUE(IsTabDiscarded(discard_tab_index));
    }));
  }

  // Discard the tab at discard_tab_index and navigates to that tab and waits
  // for it to reload
  auto DiscardAndSelectTab(int discard_tab_index,
                           const ui::ElementIdentifier& contents_id) {
    return Steps(FlushEvents(),
                 // This has to be done on a fresh message loop to prevent
                 // a tab being discarded while it is notifying its observers
                 DiscardTab(discard_tab_index), WaitForHide(contents_id),
                 SelectTab(kTabStripElementId, discard_tab_index),
                 WaitForShow(contents_id));
  }

  auto CheckChipIsExpandedState() {
    return CheckViewProperty(kHighEfficiencyChipElementId,
                             &PageActionIconView::ShouldShowLabel, true);
  }

  // Discard and reload the tab at discard_tab_index the number of times the
  // high efficiency page action chip can expand so subsequent discards
  // will result in the chip staying in its collapsed state
  auto DiscardTabUntilChipStopsExpanding(
      size_t discard_tab_index,
      size_t non_discard_tab_index,
      const ui::ElementIdentifier& contents_id) {
    MultiStep result;
    for (int i = 0; i < HighEfficiencyChipView::kChipAnimationCount; i++) {
      MultiStep temp = std::move(result);
      result = Steps(std::move(temp),
                     SelectTab(kTabStripElementId, non_discard_tab_index),
                     DiscardAndSelectTab(discard_tab_index, contents_id),
                     CheckChipIsExpandedState());
    }

    return result;
  }

  // Navigates the current active tab to the given URL and waits for it to load
  auto NavigateTab(GURL url, const ui::ElementIdentifier& contents_id) {
    return Steps(
        Do(base::BindLambdaForTesting([=]() { util_->LoadPage(url); })),
        WaitForWebContentsNavigation(contents_id, url));
  }

  auto CheckChipIsCollapsedState() {
    return CheckViewProperty(kHighEfficiencyChipElementId,
                             &PageActionIconView::ShouldShowLabel, false);
  }

  auto NameTab(size_t index, std::string name) {
    return NameViewRelative(
        kTabStripElementId, name,
        base::BindOnce([](size_t index, TabStrip* tab_strip)
                           -> views::View* { return tab_strip->tab_at(index); },
                       index));
  }

  base::SimpleTestTickClock test_clock_;
  resource_coordinator::ScopedSetTickClockForTesting
      scoped_set_tick_clock_for_testing_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TabStripModel, DanglingUntriaged> tab_strip_model_;
  GURL test_url_;
  std::unique_ptr<WebContentsInteractionTestUtil> util_;
};

// Page Action Chip should appear expanded the first three times a tab is
// discarded and collapse all subsequent times
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest, PageActionChipShows) {
  RunTestSequence(InstrumentTab(kFirstTabContents, 0),
                  NavigateTab(test_url_, kFirstTabContents),
                  AddInstrumentedTab(kSecondTabContents, test_url_, 1),
                  SelectTab(kTabStripElementId, 0),
                  EnsureNotPresent(kHighEfficiencyChipElementId),
                  DiscardTabUntilChipStopsExpanding(0, 1, kFirstTabContents),
                  SelectTab(kTabStripElementId, 1),
                  DiscardAndSelectTab(0, kFirstTabContents),
                  CheckChipIsCollapsedState());
}

// Page Action chip should collapses after navigating to a tab without a chip
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       PageActionChipCollapseOnTabSwitch) {
  RunTestSequence(InstrumentTab(kFirstTabContents, 0),
                  NavigateTab(test_url_, kFirstTabContents),
                  AddInstrumentedTab(kSecondTabContents, test_url_, 1),
                  EnsureNotPresent(kHighEfficiencyChipElementId),
                  SelectTab(kTabStripElementId, 1),
                  EnsureNotPresent(kHighEfficiencyChipElementId),
                  SelectTab(kTabStripElementId, 1),
                  DiscardAndSelectTab(0, kFirstTabContents),
                  CheckChipIsExpandedState(), SelectTab(kTabStripElementId, 1),
                  EnsureNotPresent(kHighEfficiencyChipElementId),
                  SelectTab(kTabStripElementId, 0), CheckChipIsCollapsedState(),
                  SelectTab(kTabStripElementId, 1),
                  EnsureNotPresent(kHighEfficiencyChipElementId));
}

// Page Action chip should stay collapsed when navigating between two
// discarded tabs
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       ChipCollapseRemainCollapse) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateTab(test_url_, kFirstTabContents),
      AddInstrumentedTab(kSecondTabContents, test_url_, 1),
      EnsureNotPresent(kHighEfficiencyChipElementId),
      SelectTab(kTabStripElementId, 1),
      DiscardAndSelectTab(0, kFirstTabContents), CheckChipIsExpandedState(),
      DiscardAndSelectTab(1, kSecondTabContents), CheckChipIsExpandedState(),
      SelectTab(kTabStripElementId, 0), CheckChipIsCollapsedState(),
      SelectTab(kTabStripElementId, 1), CheckChipIsCollapsedState());
}

// Page Action chip should only show on discarded non-chrome pages
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       ChipShowsOnNonChromeSites) {
  RunTestSequence(InstrumentTab(kFirstTabContents, 0),
                  NavigateTab(test_url_, kFirstTabContents),
                  AddInstrumentedTab(kSecondTabContents,
                                     GURL(chrome::kChromeUINewTabURL), 1),
                  // Discards tab on non-chrome page
                  SelectTab(kTabStripElementId, 1),
                  DiscardAndSelectTab(0, kFirstTabContents),
                  WaitForShow(kHighEfficiencyChipElementId),

                  // Discards tab on chrome://newtab page
                  DiscardAndSelectTab(1, kSecondTabContents),
                  EnsureNotPresent(kHighEfficiencyChipElementId));
}

// Clicking on the settings link in high efficiency dialog bubble should open
// a new tab and navigate to the performance settings page
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       BubbleSettingsLinkNavigates) {
  constexpr char kPerformanceSettingsLinkViewName[] = "performance_link";

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateTab(test_url_, kFirstTabContents),
      AddInstrumentedTab(kSecondTabContents, test_url_, 1),
      SelectTab(kTabStripElementId, 1),
      DiscardAndSelectTab(0, kFirstTabContents),
      SelectTab(kTabStripElementId, 1), SelectTab(kTabStripElementId, 0),
      CheckChipIsCollapsedState(), PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      InAnyContext(NameViewRelative(
          HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId,
          kPerformanceSettingsLinkViewName,
          base::BindOnce([](views::StyledLabel* label) -> views::View* {
            return label->GetFirstLinkForTesting();
          }))),
      MoveMouseTo(kPerformanceSettingsLinkViewName), ClickMouse(),
      Check(base::BindLambdaForTesting(
          [&]() { return tab_strip_model_->GetTabCount() == 3; })),
      InstrumentTab(kPerformanceSettingsTab, 2));
}

// High Efficiency Dialog bubble should close after clicking the "OK" button
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       CloseBubbleOnOkButtonClick) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateTab(test_url_, kFirstTabContents),
      AddInstrumentedTab(kSecondTabContents, test_url_, 1),
      SelectTab(kTabStripElementId, 1),
      DiscardAndSelectTab(0, kFirstTabContents),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      PressButton(HighEfficiencyBubbleView::kHighEfficiencyDialogOkButton),
      WaitForHide(
          HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId));
}

// High Efficiency dialog bubble should close after clicking on the "X"
// close button
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       CloseBubbleOnCloseButtonClick) {
  constexpr char kDialogCloseButton[] = "dialog_close_button";

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateTab(test_url_, kFirstTabContents),
      AddInstrumentedTab(kSecondTabContents, test_url_, 1),
      DiscardAndSelectTab(0, kFirstTabContents),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      NameView(kDialogCloseButton, base::BindLambdaForTesting([&]() {
                 return static_cast<views::View*>(
                     GetPageActionIconView()
                         ->GetBubble()
                         ->GetBubbleFrameView()
                         ->GetCloseButtonForTesting());
               })),
      PressButton(kDialogCloseButton),
      EnsureNotPresent(
          HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId));
}

// High Efficiency Dialog bubble should close after clicking on
// the page action chip again
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       CloseBubbleOnChipClick) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateTab(test_url_, kFirstTabContents),
      AddInstrumentedTab(kSecondTabContents, test_url_, 1),
      DiscardAndSelectTab(0, kFirstTabContents),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      PressButton(kHighEfficiencyChipElementId),
      EnsureNotPresent(
          HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId));
}

// High Efficiency dialog bubble should close when clicking to navigate to
// another tab
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       CloseBubbleOnTabSwitch) {
  constexpr char kSecondTab[] = "second_tab";

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateTab(test_url_, kFirstTabContents),
      AddInstrumentedTab(kSecondTabContents, test_url_, 1),
      SelectTab(kTabStripElementId, 1),
      DiscardAndSelectTab(0, kFirstTabContents),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      NameTab(1, kSecondTab), MoveMouseTo(kSecondTab), ClickMouse(),
      WaitForHide(
          HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId));
}

class HighEfficiencyInfoIPHInteractiveTest
    : public HighEfficiencyChipInteractiveTest {
 public:
  HighEfficiencyInfoIPHInteractiveTest() = default;
  ~HighEfficiencyInfoIPHInteractiveTest() override = default;

  void SetUp() override {
    iph_features_.InitAndEnableFeaturesWithParameters(
        {{feature_engagement::kIPHHighEfficiencyInfoModeFeature, {}},
         {performance_manager::features::kHighEfficiencyModeAvailable,
          {{"default_state", "true"}, {"time_before_discard", "30s"}}}});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    HighEfficiencyChipInteractiveTest::SetUpOnMainThread();
    EXPECT_TRUE(user_education::test::WaitForFeatureEngagementReady(
        GetFeaturePromoController()));
  }

  BrowserFeaturePromoController* GetFeaturePromoController() {
    auto* promo_controller = static_cast<BrowserFeaturePromoController*>(
        browser()->window()->GetFeaturePromoController());
    return promo_controller;
  }

 private:
  feature_engagement::test::ScopedIphFeatureList iph_features_;
};

// High Efficiency info IPH should close after clicking the "Got It"
// default button
IN_PROC_BROWSER_TEST_F(HighEfficiencyInfoIPHInteractiveTest,
                       ClosesIPHOnButtonClick) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateTab(test_url_, kFirstTabContents),
      AddInstrumentedTab(kSecondTabContents, test_url_, 1),
      SelectTab(kTabStripElementId, 1),
      DiscardAndSelectTab(0, kFirstTabContents),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      PressButton(user_education::HelpBubbleView::kDefaultButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

// High Efficiency info IPH should close and navigates to the Performance
// settings page after clicking on the settings non-default button
IN_PROC_BROWSER_TEST_F(HighEfficiencyInfoIPHInteractiveTest,
                       NavigatesToSettingsPage) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateTab(test_url_, kFirstTabContents),
      AddInstrumentedTab(kSecondTabContents, test_url_, 1),
      SelectTab(kTabStripElementId, 1),
      DiscardAndSelectTab(0, kFirstTabContents),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      FlushEvents(),
      // This needs to be done on a fresh message loop so that the IPH closes
      PressButton(
          user_education::HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      Check(base::BindLambdaForTesting(
          [&]() { return tab_strip_model_->GetTabCount() == 3; })),
      InstrumentTab(kPerformanceSettingsTab));
}

// High Efficiency IPH should close when navigating to another tab
IN_PROC_BROWSER_TEST_F(HighEfficiencyInfoIPHInteractiveTest,
                       ClosesIPHOnTabSwitch) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateTab(test_url_, kFirstTabContents),
      AddInstrumentedTab(kSecondTabContents, test_url_, 1),
      SelectTab(kTabStripElementId, 1),
      DiscardAndSelectTab(0, kFirstTabContents),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      FlushEvents(),
      // This needs to be done on a fresh message loop so that the IPH closes
      SelectTab(kTabStripElementId, 1),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}
