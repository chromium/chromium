// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/battery_saver_button.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/event_utils.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

class BatterySaverButtonTest : public TestWithBrowserView {
 public:
  BatterySaverButtonTest() = default;

  void SetUp() override { TestWithBrowserView::SetUp(); }

  void SetBatterySaverModeEnabled(bool enabled) {
    auto mode = enabled ? performance_manager::user_tuning::prefs::
                              BatterySaverModeState::kEnabled
                        : performance_manager::user_tuning::prefs::
                              BatterySaverModeState::kDisabled;
    g_browser_process->local_state()->SetInteger(
        performance_manager::user_tuning::prefs::kBatterySaverModeState,
        static_cast<int>(mode));
  }

  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

// Battery Saver is controlled by the OS on ChromeOS
#if !BUILDFLAG(IS_CHROMEOS_ASH)

// Battery saver button should not be shown when the pref state for battery
// saver mode is ON and shown when the pref state is ON
TEST_F(BatterySaverButtonTest, ShouldButtonShowTest) {
  const BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();
  ASSERT_NE(battery_saver_button, nullptr);

  SetBatterySaverModeEnabled(false);
  EXPECT_FALSE(battery_saver_button->GetVisible());

  SetBatterySaverModeEnabled(true);
  EXPECT_TRUE(battery_saver_button->GetVisible());
}

// Battery saver button has the correct tooltip and accessibility text
TEST_F(BatterySaverButtonTest, TooltipAccessibilityTextTest) {
  BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUTTON_TOOLTIP),
            battery_saver_button->GetTooltipText(gfx::Point()));

  ui::AXNodeData ax_node_data;
  battery_saver_button->GetViewAccessibility().GetAccessibleNodeData(
      &ax_node_data);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUTTON_TOOLTIP),
      ax_node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

// Battery saver bubble should be shown when the toolbar button is clicked
// and dismissed when it is clicked again
TEST_F(BatterySaverButtonTest, ShowAndHideBubbleOnButtonPressTest) {
  BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();
  ASSERT_NE(battery_saver_button, nullptr);

  SetBatterySaverModeEnabled(true);
  ASSERT_TRUE(battery_saver_button->GetVisible());

  EXPECT_FALSE(battery_saver_button->IsBubbleShowing());
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(battery_saver_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(battery_saver_button->IsBubbleShowing());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      battery_saver_button->GetBubble()->GetWidget());
  test_api.NotifyClick(e);
  EXPECT_FALSE(battery_saver_button->IsBubbleShowing());
  destroyed_waiter.Wait();
}

// Dismiss bubble if expanded when battery saver mode is deactivated
TEST_F(BatterySaverButtonTest, DismissBubbleWhenModeDeactivatedTest) {
  BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();
  ASSERT_NE(battery_saver_button, nullptr);

  SetBatterySaverModeEnabled(true);
  ASSERT_TRUE(battery_saver_button->GetVisible());

  EXPECT_FALSE(battery_saver_button->IsBubbleShowing());
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(battery_saver_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(battery_saver_button->IsBubbleShowing());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      battery_saver_button->GetBubble()->GetWidget());
  SetBatterySaverModeEnabled(false);
  EXPECT_FALSE(battery_saver_button->IsBubbleShowing());
  destroyed_waiter.Wait();
  EXPECT_FALSE(battery_saver_button->GetVisible());
}

// Check if the element identifier is set correctly by the battery saver
// toolbar button
TEST_F(BatterySaverButtonTest, ElementIdentifierTest) {
  const views::View* battery_saver_button_view =
      browser_view()->toolbar()->battery_saver_button();
  ASSERT_NE(battery_saver_button_view, nullptr);

  const views::View* matched_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kToolbarBatterySaverButtonElementId,
          browser_view()->GetElementContext());

  EXPECT_EQ(battery_saver_button_view, matched_view);
}

TEST_F(BatterySaverButtonTest, LogMetricsOnDialogDismissTest) {
  BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();
  ASSERT_NE(battery_saver_button, nullptr);

  SetBatterySaverModeEnabled(true);
  ASSERT_TRUE(battery_saver_button->GetVisible());

  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(battery_saver_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(battery_saver_button->IsBubbleShowing());

  test_api.NotifyClick(e);
  EXPECT_FALSE(battery_saver_button->IsBubbleShowing());

  GetHistogramTester()->ExpectUniqueSample(
      "PerformanceControls.BatterySaver.BubbleAction",
      BatterySaverBubbleActionType::kDismiss, 1);
}

TEST_F(BatterySaverButtonTest, LogMetricsOnTurnOffNowTest) {
  BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();
  ASSERT_NE(battery_saver_button, nullptr);

  SetBatterySaverModeEnabled(true);
  ASSERT_TRUE(battery_saver_button->GetVisible());

  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(battery_saver_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(battery_saver_button->IsBubbleShowing());

  views::BubbleDialogModelHost* const bubble_dialog_host =
      battery_saver_button->GetBubble();
  ASSERT_NE(bubble_dialog_host, nullptr);

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      bubble_dialog_host->GetWidget());
  bubble_dialog_host->Cancel();
  destroyed_waiter.Wait();

  GetHistogramTester()->ExpectUniqueSample(
      "PerformanceControls.BatterySaver.BubbleAction",
      BatterySaverBubbleActionType::kTurnOffNow, 1);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
