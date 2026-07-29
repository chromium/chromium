// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/battery_saver_button.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

class BatterySaverButtonTest : public ChromeViewsTestBase {
 public:
  BatterySaverButtonTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Register and initialize local state preferences for battery saver mode.
    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
    environment_.SetUp(&local_state_);

    // Set up mock window and user education interfaces.
    mock_browser_window_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    mock_user_education_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserUserEducationInterface>>(
            mock_browser_window_interface_.get());

    // Create a test widget to host the button and provide a valid element
    // context.
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    battery_saver_button_ =
        widget_->SetContentsView(std::make_unique<BatterySaverButton>(
            mock_browser_window_interface_.get()));
    widget_->Show();
  }

  void TearDown() override {
    // Safely close any open dialog bubbles before destroying the widget.
    if (battery_saver_button_ && battery_saver_button_->IsBubbleShowing()) {
      battery_saver_button_->GetBubble()->GetWidget()->CloseNow();
    }
    battery_saver_button_ = nullptr;
    widget_.reset();
    environment_.TearDown();
    ChromeViewsTestBase::TearDown();
  }

  void SetBatterySaverModeEnabled(bool enabled) {
    performance_manager::user_tuning::
        TestUserPerformanceTuningManagerEnvironment::SetBatterySaverMode(
            &local_state_, enabled);
  }

  BatterySaverButton* battery_saver_button() { return battery_saver_button_; }
  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }

 private:
  TestingPrefServiceSimple local_state_;
  performance_manager::user_tuning::TestUserPerformanceTuningManagerEnvironment
      environment_;
  std::unique_ptr<MockBrowserWindowInterface> mock_browser_window_interface_;
  std::unique_ptr<MockBrowserUserEducationInterface>
      mock_user_education_interface_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<BatterySaverButton> battery_saver_button_ = nullptr;
  base::HistogramTester histogram_tester_;
};

// Battery Saver is controlled by the OS on ChromeOS
#if !BUILDFLAG(IS_CHROMEOS)

// Battery saver button should not be shown when the pref state for battery
// saver mode is ON and shown when the pref state is ON
TEST_F(BatterySaverButtonTest, ShouldButtonShowTest) {
  BatterySaverButton* button = battery_saver_button();
  ASSERT_NE(button, nullptr);

  SetBatterySaverModeEnabled(false);
  EXPECT_FALSE(button->GetVisible());

  SetBatterySaverModeEnabled(true);
  EXPECT_TRUE(button->GetVisible());
}

// Battery saver button has the correct tooltip and accessibility text
TEST_F(BatterySaverButtonTest, TooltipAccessibilityTextTest) {
  BatterySaverButton* button = battery_saver_button();

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUTTON_TOOLTIP),
            button->GetRenderedTooltipText(gfx::Point()));

  ui::AXNodeData ax_node_data;
  button->GetViewAccessibility().GetAccessibleNodeData(&ax_node_data);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUTTON_TOOLTIP),
      ax_node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

// Battery saver bubble should be shown when the toolbar button is clicked
// and dismissed when it is clicked again
TEST_F(BatterySaverButtonTest, ShowAndHideBubbleOnButtonPressTest) {
  BatterySaverButton* button = battery_saver_button();
  ASSERT_NE(button, nullptr);

  SetBatterySaverModeEnabled(true);
  ASSERT_TRUE(button->GetVisible());

  EXPECT_FALSE(button->IsBubbleShowing());
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(button->IsBubbleShowing());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      button->GetBubble()->GetWidget());
  test_api.NotifyClick(e);
  EXPECT_FALSE(button->IsBubbleShowing());
  destroyed_waiter.Wait();
}

// Dismiss bubble if expanded when battery saver mode is deactivated
TEST_F(BatterySaverButtonTest, DismissBubbleWhenModeDeactivatedTest) {
  BatterySaverButton* button = battery_saver_button();
  ASSERT_NE(button, nullptr);

  SetBatterySaverModeEnabled(true);
  ASSERT_TRUE(button->GetVisible());

  EXPECT_FALSE(button->IsBubbleShowing());
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(button->IsBubbleShowing());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      button->GetBubble()->GetWidget());
  SetBatterySaverModeEnabled(false);
  EXPECT_FALSE(button->IsBubbleShowing());
  destroyed_waiter.Wait();
  EXPECT_FALSE(button->GetVisible());
}

// Check if the element identifier is set correctly by the battery saver
// toolbar button
TEST_F(BatterySaverButtonTest, ElementIdentifierTest) {
  views::View* battery_saver_button_view = battery_saver_button();
  ASSERT_NE(battery_saver_button_view, nullptr);

  const views::View* matched_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kToolbarBatterySaverButtonElementId,
          views::ElementTrackerViews::GetContextForView(
              battery_saver_button_view));

  EXPECT_EQ(battery_saver_button_view, matched_view);
}

TEST_F(BatterySaverButtonTest, LogMetricsOnDialogDismissTest) {
  BatterySaverButton* button = battery_saver_button();
  ASSERT_NE(button, nullptr);

  SetBatterySaverModeEnabled(true);
  ASSERT_TRUE(button->GetVisible());

  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(button->IsBubbleShowing());

  test_api.NotifyClick(e);
  EXPECT_FALSE(button->IsBubbleShowing());

  GetHistogramTester()->ExpectUniqueSample(
      "PerformanceControls.BatterySaver.BubbleAction",
      BatterySaverBubbleActionType::kDismiss, 1);
}

TEST_F(BatterySaverButtonTest, LogMetricsOnTurnOffNowTest) {
  BatterySaverButton* button = battery_saver_button();
  ASSERT_NE(button, nullptr);

  SetBatterySaverModeEnabled(true);
  ASSERT_TRUE(button->GetVisible());

  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(button->IsBubbleShowing());

  views::BubbleDialogModelHost* const bubble_dialog_host = button->GetBubble();
  ASSERT_NE(bubble_dialog_host, nullptr);

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      bubble_dialog_host->GetWidget());
  bubble_dialog_host->Cancel();
  destroyed_waiter.Wait();

  GetHistogramTester()->ExpectUniqueSample(
      "PerformanceControls.BatterySaver.BubbleAction",
      BatterySaverBubbleActionType::kTurnOffNow, 1);
}

#endif  // !BUILDFLAG(IS_CHROMEOS)
