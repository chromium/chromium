// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_bubble_view.h"
#include <tuple>
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_resource_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"

constexpr int kMemorySavingsKilobytes = 100 * 1024;
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

class HighEfficiencyBubbleViewTest : public TestWithBrowserView {
 public:
 protected:
  HighEfficiencyBubbleViewTest()
      : TestWithBrowserView(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    feature_list_.InitAndDisableFeature(
        performance_manager::features::kMemorySavingsReportingImprovements);

    TestWithBrowserView::SetUp();

    AddNewTab(kMemorySavingsKilobytes,
              ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->SetHighEfficiencyModeEnabled(true);
  }

  // Creates a new tab at index 0 that would report the given memory savings and
  // discard reason if the tab was discarded
  void AddNewTab(int memory_savings,
                 mojom::LifecycleUnitDiscardReason discard_reason) {
    AddTab(browser(), GURL("http://foo"));
    content::WebContents* const contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    HighEfficiencyChipTabHelper::CreateForWebContents(contents);
    performance_manager::user_tuning::UserPerformanceTuningManager::
        PreDiscardResourceUsage::CreateForWebContents(contents, memory_savings,
                                                      discard_reason);
  }

  void SetTabDiscardState(int tab_index, bool is_discarded) {
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(tab_index);
    std::unique_ptr<DiscardMockNavigationHandle> navigation_handle =
        std::make_unique<DiscardMockNavigationHandle>();
    navigation_handle.get()->SetWasDiscarded(is_discarded);
    navigation_handle.get()->SetWebContents(web_contents);
    HighEfficiencyChipTabHelper::FromWebContents(web_contents)
        ->DidStartNavigation(navigation_handle.get());

    browser_view()
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->UpdateAll();
  }

  PageActionIconView* GetPageActionIconView() {
    return browser_view()
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->GetIconView(PageActionIconType::kHighEfficiency);
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
};

// When the page action chip is clicked, the dialog should open.
TEST_F(HighEfficiencyBubbleViewTest, ShouldOpenDialogOnClick) {
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_EQ(view->GetBubble(), nullptr);

  ClickPageActionChip();

  EXPECT_NE(view->GetBubble(), nullptr);
}

// When the dialog is closed, UMA metrics should be logged.
TEST_F(HighEfficiencyBubbleViewTest, ShouldLogMetricsOnDialogDismiss) {
  SetTabDiscardState(0, true);

  // Open bubble
  ClickPageActionChip();
  // Close bubble
  ClickPageActionChip();

  histogram_tester_.ExpectUniqueSample(
      "PerformanceControls.HighEfficiency.BubbleAction",
      HighEfficiencyBubbleActionType::kDismiss, 1);
}

// A link should be rendered within the dialog.
TEST_F(HighEfficiencyBubbleViewTest, ShouldRenderLinkInDialog) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::StyledLabel* label = GetDialogLabel<views::StyledLabel>(
      HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId);
  EXPECT_TRUE(
      label->GetText().find(u"You can change this anytime in Settings") !=
      std::string::npos);
}

// The memory savings should be rendered within the dialog.
TEST_F(HighEfficiencyBubbleViewTest, ShouldRenderMemorySavingsInDialog) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::StyledLabel* label = GetDialogLabel<views::StyledLabel>(
      HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId);
  EXPECT_TRUE(label->GetText().find(ui::FormatBytes(
                  kMemorySavingsKilobytes * 1024)) != std::string::npos);
}

//  When the memory savings are lower than 10Mb then they shouldn't be rendered
//  in the dialog.
TEST_F(HighEfficiencyBubbleViewTest,
       ShouldNotRenderSmallMemorySavingsInDialog) {
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

TEST_F(HighEfficiencyBubbleViewTest, ShowDialogWithSavingsInGuestMode) {
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

TEST_F(HighEfficiencyBubbleViewTest, ShowDialogWithoutSavingsInGuestMode) {
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

TEST_F(HighEfficiencyBubbleViewTest,
       ShouldCollapseChipAfterNavigatingTabsWithDialogOpen) {
  AddNewTab(kMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(2, tab_strip_model->GetTabCount());

  SetTabDiscardState(0, true);
  SetTabDiscardState(1, true);

  EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());
  tab_strip_model->SelectNextTab();
  web_contents->WasHidden();

  EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());
  ClickPageActionChip();

  tab_strip_model->SelectPreviousTab();
  web_contents->WasShown();
  EXPECT_FALSE(GetPageActionIconView()->ShouldShowLabel());
}

class HighEfficiencyBubbleViewMemorySavingsImprovementsTest
    : public HighEfficiencyBubbleViewTest,
      public testing::WithParamInterface<std::tuple<int, int>> {
 public:
  HighEfficiencyBubbleViewMemorySavingsImprovementsTest() = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        performance_manager::features::kMemorySavingsReportingImprovements);
    TestWithBrowserView::SetUp();

    AddNewTab(kMemorySavingsKilobytes,
              ::mojom::LifecycleUnitDiscardReason::PROACTIVE);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// The memory savings should be rendered within the resource view.
TEST_F(HighEfficiencyBubbleViewMemorySavingsImprovementsTest,
       ShouldRenderMemorySavingsInResourceView) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::Label* label = GetDialogLabel<views::Label>(
      HighEfficiencyResourceView::
          kHighEfficiencyResourceViewMemorySavingsElementId);
  EXPECT_TRUE(label->GetText().find(ui::FormatBytes(
                  kMemorySavingsKilobytes * 1024)) != std::string::npos);
}

// The memory savings should not be rendered within the text above the resource
// view.
TEST_F(HighEfficiencyBubbleViewMemorySavingsImprovementsTest,
       ShouldNotRenderMemorySavingsInDialogBodyText) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::Label* label = GetDialogLabel<views::Label>(
      HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId);
  EXPECT_EQ(
      label->GetText().find(ui::FormatBytes(kMemorySavingsKilobytes * 1024)),
      std::string::npos);

  EXPECT_NE(label->GetText().find(
                l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_DIALOG_BODY_V2)),
            std::string::npos);
}

// The correct label should be rendered for different memory savings amounts.
TEST_P(HighEfficiencyBubbleViewMemorySavingsImprovementsTest,
       ShowsCorrectLabelsForDifferentSavings) {
  AddNewTab(std::get<0>(GetParam()),
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::Label* label = GetDialogLabel<views::Label>(
      HighEfficiencyResourceView::
          kHighEfficiencyResourceViewMemoryLabelElementId);
  EXPECT_EQ(label->GetText(),
            l10n_util::GetStringUTF16(std::get<1>(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    HighEfficiencyBubbleViewMemorySavingsImprovementsTest,
    ::testing::Values(
        std::tuple{50 * 1024, IDS_HIGH_EFFICIENCY_DIALOG_SMALL_SAVINGS_LABEL},
        std::tuple{100 * 1024, IDS_HIGH_EFFICIENCY_DIALOG_MEDIUM_SAVINGS_LABEL},
        std::tuple{150 * 1024, IDS_HIGH_EFFICIENCY_DIALOG_MEDIUM_SAVINGS_LABEL},
        std::tuple{600 * 1024, IDS_HIGH_EFFICIENCY_DIALOG_LARGE_SAVINGS_LABEL},
        std::tuple{900 * 1024,
                   IDS_HIGH_EFFICIENCY_DIALOG_VERY_LARGE_SAVINGS_LABEL}));
