// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/performance_controls/tab_discard_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_bubble_view.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"

constexpr int kMemorySavingsKilobytes = 100000;
constexpr int kSmallMemorySavingsKilobytes = 10;

class DiscardMockNavigationHandle : public content::MockNavigationHandle {
 public:
  void SetWasDiscarded(bool was_discarded) { was_discarded_ = was_discarded; }
  bool ExistingDocumentWasDiscarded() const override { return was_discarded_; }
  void SetWebContents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }
  content::WebContents* GetWebContents() override { return web_contents_; }

 private:
  bool was_discarded_ = false;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

class HighEfficiencyChipViewTest : public TestWithBrowserView {
 public:
 protected:
  HighEfficiencyChipViewTest() = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        performance_manager::features::kHighEfficiencyModeAvailable);
    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
    environment_.SetUp(&local_state_);
    TestWithBrowserView::SetUp();

    AddNewTab(kMemorySavingsKilobytes,
              ::mojom::LifecycleUnitDiscardReason::PROACTIVE);
  }

  void TearDown() override {
    TestWithBrowserView::TearDown();
    environment_.TearDown();
  }

  // Creates a new tab at index 0 that would report the given memory savings and
  // discard reason if the tab was discarded
  void AddNewTab(int memory_savings,
                 mojom::LifecycleUnitDiscardReason discard_reason) {
    AddTab(browser(), GURL("http://foo"));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TabDiscardTabHelper::CreateForWebContents(contents);
    performance_manager::user_tuning::UserPerformanceTuningManager::
        PreDiscardResourceUsage::CreateForWebContents(contents, memory_savings,
                                                      discard_reason);
  }

  void SetTabDiscardState(int tab_index, bool is_discarded) {
    TabDiscardTabHelper* tab_helper = TabDiscardTabHelper::FromWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(tab_index));
    std::unique_ptr<DiscardMockNavigationHandle> navigation_handle =
        std::make_unique<DiscardMockNavigationHandle>();
    navigation_handle.get()->SetWasDiscarded(is_discarded);
    navigation_handle.get()->SetWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(tab_index));
    tab_helper->DidStartNavigation(navigation_handle.get());

    browser_view()
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->UpdateAll();
  }

  void SetHighEfficiencyModeEnabled(bool enabled) {
    g_browser_process->local_state()->SetBoolean(
        performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
        enabled);
  }

  PageActionIconView* GetPageActionIconView() {
    return browser_view()
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->GetIconView(PageActionIconType::kHighEfficiency);
  }

  views::InkDropState GetInkDropState() {
    return views::InkDrop::Get(GetPageActionIconView())
        ->GetInkDrop()
        ->GetTargetInkDropState();
  }

  template <class T>
  T* GetDialogLabel(ui::ElementIdentifier identifier) {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForWidget(
            GetPageActionIconView()->GetBubble()->anchor_widget());
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingViewAs<T>(
        identifier, context);
  }

  void ClickPageActionChip() {
    PageActionIconView* view = GetPageActionIconView();

    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi test_api(view);
    test_api.NotifyClick(e);
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple local_state_;
  performance_manager::user_tuning::TestUserPerformanceTuningManagerEnvironment
      environment_;
};

// When the previous page has a tab discard state of true, when the icon is
// updated it should be visible.
TEST_F(HighEfficiencyChipViewTest, ShouldShowChipForProactivelyDiscardedPage) {
  SetHighEfficiencyModeEnabled(true);
  SetTabDiscardState(0, true);
  EXPECT_TRUE(GetPageActionIconView()->GetVisible());
}

TEST_F(HighEfficiencyChipViewTest,
       ShouldNotShowChipWhenNonProactivelyDiscardPage) {
  SetHighEfficiencyModeEnabled(true);

  // Add a new tab that was discarded through extensions
  AddNewTab(kMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::EXTERNAL);
  SetTabDiscardState(0, true);
  EXPECT_FALSE(GetPageActionIconView()->GetVisible());

  // Add a new tab that was urgently discarded
  AddNewTab(kMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::URGENT);
  SetTabDiscardState(0, true);
  EXPECT_FALSE(GetPageActionIconView()->GetVisible());
}

// If a discard is triggered when the user doesn't have high efficiency mode
// enabled, we don't show the chip.
TEST_F(HighEfficiencyChipViewTest, ShouldNotShowWhenPrefIsFalse) {
  SetHighEfficiencyModeEnabled(false);
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();

  EXPECT_FALSE(view->GetVisible());
}

// When the previous page was not previously discarded, the icon should not be
// visible.
TEST_F(HighEfficiencyChipViewTest, ShouldNotShowForRegularPage) {
  SetHighEfficiencyModeEnabled(true);
  SetTabDiscardState(0, false);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_FALSE(view->GetVisible());
}

// When the page action chip is clicked, the dialog should open.
TEST_F(HighEfficiencyChipViewTest, ShouldOpenDialogOnClick) {
  SetHighEfficiencyModeEnabled(true);
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_EQ(view->GetBubble(), nullptr);

  ClickPageActionChip();

  EXPECT_NE(view->GetBubble(), nullptr);
}

// When the dialog is closed, UMA metrics should be logged.
TEST_F(HighEfficiencyChipViewTest, ShouldLogMetricsOnDialogDismiss) {
  SetTabDiscardState(0, true);

  // Open bubble
  ClickPageActionChip();
  // Close bubble
  ClickPageActionChip();

  histogram_tester_.ExpectUniqueSample(
      "PerformanceControls.HighEfficiency.BubbleAction",
      HighEfficiencyBubbleActionType::kDismiss, 1);
}

// When the dialog is closed, the ink drop should hide.
TEST_F(HighEfficiencyChipViewTest, ShouldShowAndHideInkDrop) {
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_EQ(GetInkDropState(), views::InkDropState::HIDDEN);

  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(view);
  // Open bubble
  test_api.NotifyClick(press);
  test_api.NotifyClick(release);
  EXPECT_EQ(GetInkDropState(), views::InkDropState::ACTIVATED);

  // Close bubble
  test_api.NotifyClick(press);
  EXPECT_EQ(GetInkDropState(), views::InkDropState::HIDDEN);
}

// A link should be rendered within the dialog.
TEST_F(HighEfficiencyChipViewTest, ShouldRenderLinkInDialog) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::StyledLabel* label = GetDialogLabel<views::StyledLabel>(
      HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId);
  EXPECT_TRUE(
      label->GetText().find(u"You can change this anytime in Settings") !=
      std::string::npos);
}

// The memory savings should be rendered within the dialog.
TEST_F(HighEfficiencyChipViewTest, ShouldRenderMemorySavingsInDialog) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::StyledLabel* label = GetDialogLabel<views::StyledLabel>(
      HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId);
  EXPECT_TRUE(label->GetText().find(ui::FormatBytes(
                  kMemorySavingsKilobytes * 1024)) != std::string::npos);
}

//  When the memory savings are lower than 1Mb then they shouldn't be rendered
//  in the dialog.
TEST_F(HighEfficiencyChipViewTest, ShouldNotRenderSmallMemorySavingsInDialog) {
  // Add a new tab with small memory savings.
  AddNewTab(kSmallMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  // Mark the new tab as discarded.
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::StyledLabel* label = GetDialogLabel<views::StyledLabel>(
      HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId);
  EXPECT_TRUE(
      label->GetText().find(u"Memory Saver freed up memory for other tasks") !=
      std::string::npos);
}

// When the previous page was not previously discarded, the icon should not be
// visible.
TEST_F(HighEfficiencyChipViewTest, ShouldHideLabelAfterMultipleDiscards) {
  SetHighEfficiencyModeEnabled(true);
  // Open the tab the max number of times for the label to be visible
  for (int i = 0; i < HighEfficiencyChipView::kChipAnimationCount; i++) {
    SetTabDiscardState(0, true);
    EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());
    SetTabDiscardState(0, false);
  }

  // The label should be hidden on subsequent discards
  SetTabDiscardState(0, true);
  EXPECT_FALSE(GetPageActionIconView()->ShouldShowLabel());
}

TEST_F(HighEfficiencyChipViewTest, ShouldCollapseChipAfterNavigatingTabs) {
  SetHighEfficiencyModeEnabled(true);
  AddNewTab(kMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(2, tab_strip_model->GetTabCount());

  SetTabDiscardState(0, true);
  EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());

  tab_strip_model->SelectNextTab();
  PageActionIconView* view = GetPageActionIconView();
  EXPECT_FALSE(view->GetVisible());

  SetTabDiscardState(1, true);
  EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());

  tab_strip_model->SelectPreviousTab();
  EXPECT_FALSE(GetPageActionIconView()->ShouldShowLabel());

  tab_strip_model->SelectNextTab();
  EXPECT_FALSE(GetPageActionIconView()->ShouldShowLabel());
}

TEST_F(HighEfficiencyChipViewTest, ShowChipWithSavingsInGuestMode) {
  TestingProfile* testprofile = browser()->profile()->AsTestingProfile();
  EXPECT_TRUE(testprofile);
  testprofile->SetGuestSession(true);

  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::StyledLabel* label = GetDialogLabel<views::StyledLabel>(
      HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId);

  EXPECT_EQ(label->GetText().find(u"You can change this anytime in Settings"),
            std::string::npos);

  EXPECT_NE(
      label->GetText().find(ui::FormatBytes(kMemorySavingsKilobytes * 1024)),
      std::string::npos);
}

TEST_F(HighEfficiencyChipViewTest, ShowChipWithoutSavingsInGuestMode) {
  // Add a new tab with small memory savings.
  AddNewTab(kSmallMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  TestingProfile* testprofile = browser()->profile()->AsTestingProfile();
  EXPECT_TRUE(testprofile);
  testprofile->SetGuestSession(true);

  SetTabDiscardState(0, true);
  ClickPageActionChip();

  // Since there is no placeholders in the bubble text in guest mode and without
  // savings, the text is created with views::Label instead of
  // views::StyledLabel
  views::Label* label = GetDialogLabel<views::Label>(
      HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId);

  EXPECT_EQ(label->GetText().find(u"You can change this anytime in Settings"),
            std::string::npos);

  EXPECT_NE(
      label->GetText().find(u"Memory Saver freed up memory for other tasks"),
      std::string::npos);
}
