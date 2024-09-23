// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/memory_saver_bubble_view.h"

#include <tuple>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_chip_view.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_resource_view.h"
#include "chrome/browser/ui/views/performance_controls/test_support/memory_saver_unit_test_mixin.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kMemorySavingsKilobytes = 100 * 1024;
}  // namespace

class MemorySaverBubbleViewTest
    : public MemorySaverUnitTestMixin<TestWithBrowserView>,
      public testing::WithParamInterface<std::tuple<int, int>> {
 public:
  void SetUp() override {

    MemorySaverUnitTestMixin::SetUp();

    AddNewTab(kMemorySavingsKilobytes,
              ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

    SetMemorySaverModeEnabled(true);
  }

  template <class T>
  T* GetMatchingView(ui::ElementIdentifier identifier) {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForWidget(
            GetPageActionIconView()->GetBubble()->anchor_widget());
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingViewAs<T>(
        identifier, context);
  }

  void ClickPageActionChip() {
    PageActionIconView* view = GetPageActionIconView();

    ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi test_api(view);
    test_api.NotifyClick(e);
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// When the page action chip is clicked, the dialog should open.
TEST_F(MemorySaverBubbleViewTest, ShouldOpenDialogOnClick) {
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_EQ(view->GetBubble(), nullptr);

  ClickPageActionChip();

  EXPECT_NE(view->GetBubble(), nullptr);
}

// When the dialog is closed, UMA metrics should be logged.
TEST_F(MemorySaverBubbleViewTest, ShouldLogMetricsOnDialogDismiss) {
  SetTabDiscardState(0, true);

  // Open bubble
  ClickPageActionChip();
  // Close bubble
  ClickPageActionChip();

  histogram_tester_.ExpectUniqueSample(
      "PerformanceControls.MemorySaver.BubbleAction",
      MemorySaverBubbleActionType::kDismiss, 1);
}

// A the domain of the current site should be rendered as a subtitle.
TEST_F(MemorySaverBubbleViewTest, ShouldRenderDomainInDialogSubtitle) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::Widget* widget = GetPageActionIconView()->GetBubble()->GetWidget();
  views::BubbleDialogDelegate* const bubble_delegate =
      widget->widget_delegate()->AsBubbleDialogDelegate();
  EXPECT_EQ(bubble_delegate->GetSubtitle(), u"foo.com");
}

TEST_F(MemorySaverBubbleViewTest,
       ShowDialogWithoutExcludeSiteButtonInGuestMode) {
  AddNewTab(kMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  TestingProfile* const testprofile = browser()->profile()->AsTestingProfile();
  EXPECT_TRUE(testprofile);
  testprofile->SetGuestSession(true);

  SetTabDiscardState(0, true);
  ClickPageActionChip();

  // Exclude site button shouldn't be shown since guest users can't exclude
  // sites from being discarded
  views::Button* const cancel_button = GetMatchingView<views::Button>(
      MemorySaverBubbleView::kMemorySaverDialogCancelButton);
  EXPECT_EQ(cancel_button, nullptr);
}

TEST_F(MemorySaverBubbleViewTest,
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

// The memory savings should be rendered within the resource view.
TEST_F(MemorySaverBubbleViewTest, ShouldRenderMemorySavingsInResourceView) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::Label* label = GetMatchingView<views::Label>(
      MemorySaverResourceView::kMemorySaverResourceViewMemorySavingsElementId);
  EXPECT_TRUE(label->GetText().find(ui::FormatBytes(
                  kMemorySavingsKilobytes * 1024)) != std::string::npos);
}

// The memory savings should not be rendered within the text above the resource
// view.
TEST_F(MemorySaverBubbleViewTest,
       ShouldNotRenderMemorySavingsInDialogBodyText) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::Label* label = GetMatchingView<views::Label>(
      MemorySaverBubbleView::kMemorySaverDialogBodyElementId);
  EXPECT_EQ(
      label->GetText().find(ui::FormatBytes(kMemorySavingsKilobytes * 1024)),
      std::string::npos);

  EXPECT_NE(label->GetText().find(
                l10n_util::GetStringUTF16(IDS_MEMORY_SAVER_DIALOG_BODY)),
            std::string::npos);
}

// The correct label should be rendered for different memory savings amounts.
TEST_P(MemorySaverBubbleViewTest, ShowsCorrectLabelsForDifferentSavings) {
  AddNewTab(std::get<0>(GetParam()),
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::Label* label = GetMatchingView<views::Label>(
      MemorySaverResourceView::kMemorySaverResourceViewMemoryLabelElementId);
  EXPECT_EQ(label->GetText(),
            l10n_util::GetStringUTF16(std::get<1>(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MemorySaverBubbleViewTest,
    ::testing::Values(
        std::tuple{50 * 1024, IDS_MEMORY_SAVER_DIALOG_SMALL_SAVINGS_LABEL},
        std::tuple{100 * 1024, IDS_MEMORY_SAVER_DIALOG_MEDIUM_SAVINGS_LABEL},
        std::tuple{150 * 1024, IDS_MEMORY_SAVER_DIALOG_MEDIUM_SAVINGS_LABEL},
        std::tuple{600 * 1024, IDS_MEMORY_SAVER_DIALOG_LARGE_SAVINGS_LABEL},
        std::tuple{900 * 1024,
                   IDS_MEMORY_SAVER_DIALOG_VERY_LARGE_SAVINGS_LABEL}));
