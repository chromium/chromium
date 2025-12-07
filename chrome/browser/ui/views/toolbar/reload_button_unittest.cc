// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/reload_button.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

class ReloadButtonTestBase {
 public:
  ReloadButtonTestBase() = default;

  ReloadButtonTestBase(const ReloadButtonTestBase&) = delete;
  ReloadButtonTestBase& operator=(const ReloadButtonTestBase&) = delete;

  void CheckState(bool enabled,
                  ReloadButton::Mode intended_mode,
                  ReloadButton::Mode visible_mode,
                  bool double_click_timer_running,
                  bool mode_switch_timer_running);

  // These accessors eliminate the need to declare each testcase as a friend.
  void set_mouse_hovered(bool hovered) {
    reload_button()->testing_mouse_hovered_ = hovered;
  }
  int reload_count() { return reload_button()->testing_reload_count_; }

 protected:
  virtual ReloadButton* reload_button() = 0;
  virtual Profile* GetProfile() = 0;

  void SetupReloadButtonTimers(ReloadButton* button) {
    // Set the timer delays to 0 so that timers will fire as soon as we tell the
    // message loop to run pending tasks.
    reload_button()->double_click_timer_delay_ = base::TimeDelta();
    reload_button()->mode_switch_timer_delay_ = base::TimeDelta();
  }
};

void ReloadButtonTestBase::CheckState(bool enabled,
                                      ReloadButton::Mode intended_mode,
                                      ReloadButton::Mode visible_mode,
                                      bool double_click_timer_running,
                                      bool mode_switch_timer_running) {
  EXPECT_EQ(enabled, reload_button()->GetEnabled());
  EXPECT_EQ(intended_mode, reload_button()->intended_mode_);
  EXPECT_EQ(visible_mode, reload_button()->visible_mode_);
  EXPECT_EQ(double_click_timer_running,
            reload_button()->double_click_timer_.IsRunning());
  EXPECT_EQ(mode_switch_timer_running,
            reload_button()->mode_switch_timer_.IsRunning());
}

class ReloadButtonTest : public ChromeViewsTestBase,
                         public ReloadButtonTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    reload_ = std::make_unique<ReloadButton>(GetProfile(), nullptr);
    SetupReloadButtonTimers(reload_.get());
  }

  void TearDown() override {
    reload_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  ReloadButton* reload_button() override { return reload_.get(); }
  Profile* GetProfile() override { return profile_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ReloadButton> reload_;
};

TEST_F(ReloadButtonTest, Basic) {
  // The stop/reload button starts in the "enabled reload" state with no timers
  // running.
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             false, false);

  // Press the button.  This should start the double-click timer.
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(reload_button());
  test_api.NotifyClick(e);
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             true, false);

  // Now change the mode (as if the browser had started loading the page).  This
  // should cancel the double-click timer since the button is not hovered.
  reload_button()->ChangeMode(ReloadButton::Mode::kStop, false);
  CheckState(true, ReloadButton::Mode::kStop, ReloadButton::Mode::kStop, false,
             false);

  // Press the button again.  This should change back to reload.
  test_api.NotifyClick(e);
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             false, false);
}

TEST_F(ReloadButtonTest, DoubleClickTimer) {
  // Start by pressing the button.
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(reload_button());
  test_api.NotifyClick(e);

  // Try to press the button again.  This should do nothing because the timer is
  // running.
  int original_reload_count = reload_count();
  test_api.NotifyClick(e);
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             true, false);
  EXPECT_EQ(original_reload_count, reload_count());

  // Hover the button, and change mode.  The visible mode should not change,
  // again because the timer is running.
  set_mouse_hovered(true);
  reload_button()->ChangeMode(ReloadButton::Mode::kStop, false);
  CheckState(true, ReloadButton::Mode::kStop, ReloadButton::Mode::kReload, true,
             false);

  // Now fire the timer.  This should complete the mode change.
  base::RunLoop().RunUntilIdle();
  CheckState(true, ReloadButton::Mode::kStop, ReloadButton::Mode::kStop, false,
             false);
}

TEST_F(ReloadButtonTest, DisableOnHover) {
  // Change to stop and hover.
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(reload_button()).NotifyClick(e);
  reload_button()->ChangeMode(ReloadButton::Mode::kStop, false);
  set_mouse_hovered(true);

  // Now change back to reload.  This should result in a disabled stop button
  // due to the hover.
  reload_button()->ChangeMode(ReloadButton::Mode::kReload, false);
  CheckState(false, ReloadButton::Mode::kReload, ReloadButton::Mode::kStop,
             false, true);

  // Un-hover the button, which should allow it to reset.
  set_mouse_hovered(false);
  ui::MouseEvent e2(ui::EventType::kMouseMoved, gfx::Point(), gfx::Point(),
                    ui::EventTimeForNow(), 0, 0);
  reload_button()->OnMouseExited(e2);
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             false, false);
}

TEST_F(ReloadButtonTest, ResetOnClick) {
  // Change to stop and hover.
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(reload_button());
  test_api.NotifyClick(e);
  reload_button()->ChangeMode(ReloadButton::Mode::kStop, false);
  set_mouse_hovered(true);

  // Press the button.  This should change back to reload despite the hover,
  // because it's a direct user action.
  test_api.NotifyClick(e);
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             false, false);
}

TEST_F(ReloadButtonTest, ResetOnTimer) {
  // Change to stop, hover, and change back to reload.
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(reload_button()).NotifyClick(e);
  reload_button()->ChangeMode(ReloadButton::Mode::kStop, false);
  set_mouse_hovered(true);
  reload_button()->ChangeMode(ReloadButton::Mode::kReload, false);

  // Now fire the stop-to-reload timer.  This should reset the button.
  base::RunLoop().RunUntilIdle();
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             false, false);
}

TEST_F(ReloadButtonTest, AccessibleHasPopup) {
  ui::AXNodeData button_data;

  button_data = ui::AXNodeData();
  reload_button()->GetViewAccessibility().GetAccessibleNodeData(&button_data);
  EXPECT_FALSE(reload_button()->GetMenuEnabled());
  EXPECT_EQ(ax::mojom::HasPopup::kNone, button_data.GetHasPopup());

  button_data = ui::AXNodeData();
  reload_button()->SetMenuEnabled(true);
  reload_button()->GetViewAccessibility().GetAccessibleNodeData(&button_data);
  EXPECT_TRUE(reload_button()->GetMenuEnabled());
  EXPECT_EQ(ax::mojom::HasPopup::kMenu, button_data.GetHasPopup());
}

TEST_F(ReloadButtonTest, TooltipText) {
  reload_button()->SetVisibleMode(ReloadButton::Mode::kReload);
  EXPECT_FALSE(reload_button()->GetMenuEnabled());
  EXPECT_EQ(reload_button()->GetRenderedTooltipText(gfx::Point()),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD));
  reload_button()->SetVisibleMode(ReloadButton::Mode::kStop);
  EXPECT_EQ(reload_button()->GetRenderedTooltipText(gfx::Point()),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_STOP));

  reload_button()->SetMenuEnabled(true);
  reload_button()->SetVisibleMode(ReloadButton::Mode::kReload);
  EXPECT_TRUE(reload_button()->GetMenuEnabled());
  EXPECT_EQ(reload_button()->GetRenderedTooltipText(gfx::Point()),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD_WITH_MENU));
  reload_button()->SetVisibleMode(ReloadButton::Mode::kStop);
  EXPECT_EQ(reload_button()->GetRenderedTooltipText(gfx::Point()),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_STOP));
}

TEST_F(ReloadButtonTest, TooltipTextAccessibility) {
  ui::AXNodeData button_data;
  reload_button()->SetVisibleMode(ReloadButton::Mode::kReload);
  reload_button()->GetViewAccessibility().GetAccessibleNodeData(&button_data);
  EXPECT_FALSE(reload_button()->GetMenuEnabled());
  EXPECT_EQ(reload_button()->GetRenderedTooltipText(gfx::Point()),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD));
  EXPECT_EQ(button_data.GetString16Attribute(
                ax::mojom::StringAttribute::kDescription),
            reload_button()->GetRenderedTooltipText(gfx::Point()));
  button_data = ui::AXNodeData();
  reload_button()->SetVisibleMode(ReloadButton::Mode::kStop);
  reload_button()->GetViewAccessibility().GetAccessibleNodeData(&button_data);
  EXPECT_EQ(reload_button()->GetRenderedTooltipText(gfx::Point()),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_STOP));
  EXPECT_EQ(button_data.GetString16Attribute(
                ax::mojom::StringAttribute::kDescription),
            reload_button()->GetRenderedTooltipText(gfx::Point()));
  button_data = ui::AXNodeData();

  reload_button()->SetMenuEnabled(true);
  reload_button()->SetVisibleMode(ReloadButton::Mode::kReload);
  reload_button()->GetViewAccessibility().GetAccessibleNodeData(&button_data);
  EXPECT_TRUE(reload_button()->GetMenuEnabled());
  EXPECT_EQ(reload_button()->GetRenderedTooltipText(gfx::Point()),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD_WITH_MENU));
  EXPECT_EQ(button_data.GetString16Attribute(
                ax::mojom::StringAttribute::kDescription),
            reload_button()->GetRenderedTooltipText(gfx::Point()));
  button_data = ui::AXNodeData();
  reload_button()->SetVisibleMode(ReloadButton::Mode::kStop);
  reload_button()->GetViewAccessibility().GetAccessibleNodeData(&button_data);
  EXPECT_EQ(reload_button()->GetRenderedTooltipText(gfx::Point()),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_STOP));
  EXPECT_EQ(button_data.GetString16Attribute(
                ax::mojom::StringAttribute::kDescription),
            reload_button()->GetRenderedTooltipText(gfx::Point()));
}

class ReloadButtonMetricsTest : public ChromeViewsTestBase,
                                public ReloadButtonTestBase {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kInitialWebUIMetrics);
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    WaapUIMetricsServiceFactory::GetForProfile(profile_.get());

    command_updater_ = std::make_unique<CommandUpdaterImpl>(nullptr);

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget_->Show();

    auto button =
        std::make_unique<ReloadButton>(profile_.get(), command_updater_.get());
    reload_ = widget_->SetContentsView(std::move(button));
    SetupReloadButtonTimers(reload_);
  }

  void TearDown() override {
    // `reload_` is a raw_ptr to a View owned by `widget_`.
    // The widget must be destroyed before tearing down the test view hierarchy.
    // The pointer must be cleared before the widget is destroyed to avoid a
    // dangling pointer detection.
    reload_ = nullptr;
    widget_.reset();
    command_updater_.reset();
    ChromeViewsTestBase::TearDown();
  }

  ui::MouseEvent CreateMouseEvent(ui::EventType type,
                                  const gfx::Point& point,
                                  int event_flags = ui::EF_LEFT_MOUSE_BUTTON) {
    return ui::MouseEvent(type, point, point, ui::EventTimeForNow(),
                          event_flags, event_flags);
  }

  // Simulates painting the button and notifying the button of the next
  // presentation with the given timestamp.
  void SimulatePaint(base::TimeTicks presentation_timestamp) {
    gfx::Canvas canvas(gfx::Size(20, 20), 1.0f, false);
    reload_button()->PaintButtonContents(&canvas);
    viz::FrameTimingDetails frame_timing_details;
    frame_timing_details.presentation_feedback.timestamp =
        presentation_timestamp;
    reload_button()->OnNextPresentation(reload_button()->visible_mode(),
                                        reload_button()->GetState(),
                                        frame_timing_details);
  }

 protected:
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  // ReloadButtonTestBase:
  ReloadButton* reload_button() override { return reload_; }
  Profile* GetProfile() override { return profile_.get(); }

 private:
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CommandUpdaterImpl> command_updater_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<ReloadButton> reload_ = nullptr;
};

// Tests that the FirstPaint and FirstContentfulPaint histograms are logged
// exactly once when the ReloadButton is painted for the first time. Subsequent
// paints should not trigger these metrics again.
// TODO(crbug.com/448794588): Re-enable once FP cleanup works across all tests.
TEST_F(ReloadButtonMetricsTest, DISABLED_LogFirstPaintMetrics) {
  // Initial State.
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstPaint", 0);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 0);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 0);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease", 0);

  // Simulate First Paint
  SimulatePaint(base::TimeTicks() + base::Microseconds(1));

  // Check Histograms.
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstPaint", 1);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 1);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 0);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease", 0);

  // Simulate Second Paint.
  SimulatePaint(base::TimeTicks() + base::Microseconds(2));

  // Counts should not increase.
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstPaint", 1);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 1);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 0);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease", 0);
}

// Tests that the MousePressToNextPaint histogram is correctly logged. It
// simulates mouse press and release events followed by painting the button,
// verifying that a metric is recorded only after a press and a corresponding
// paint.
TEST_F(ReloadButtonMetricsTest, LogMousePressToNextPaintMetric) {
  // Initial State.
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.MousePressToNextPaint", 0);
  gfx::Canvas canvas(gfx::Size(20, 20), 1.0f, false);

  // Press 1.
  reload_button()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed, {0, 0}));
  // Release 1.
  reload_button()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased, {0, 0}));

  // Press 2.
  reload_button()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed, {0, 0}));
  // Paint 1: Should record only once for Press 2 to this paint.
  SimulatePaint(base::TimeTicks() + base::Microseconds(1));
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.MousePressToNextPaint", 1);

  // Release 2.
  reload_button()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased, {0, 0}));
  // Paint 2: Should not record as there is no mouse press.
  SimulatePaint(base::TimeTicks() + base::Microseconds(2));
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.MousePressToNextPaint", 1);

  // Press 3.
  reload_button()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed, {0, 0}));
  // Paint 3 - Should record again.
  SimulatePaint(base::TimeTicks() + base::Microseconds(3));
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.MousePressToNextPaint", 2);
}

// Tests the logging for the MouseHoverToNextPaint metric. It simulates mouse
// enter and exit events and verifies that a metric is recorded after a mouse
// enter event is followed by a paint, but not after a mouse exit.
TEST_F(ReloadButtonMetricsTest, LogMouseHoverToNextPaintMetric) {
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.MouseHoverToNextPaint", 0);
  gfx::Canvas canvas(gfx::Size(20, 20), 1.0f, false);

  reload_button()->SetState(views::Button::STATE_NORMAL);

  // Simulate MouseEnter.
  ui::MouseEvent enter_event(ui::EventType::kMouseEntered, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  reload_button()->OnMouseEntered(enter_event);
  EXPECT_EQ(reload_button()->GetState(), views::Button::STATE_HOVERED);

  // Simulate Paint.
  SimulatePaint(base::TimeTicks() + base::Microseconds(1));
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.MouseHoverToNextPaint", 1);

  // Simulate MouseExit.
  ui::MouseEvent exit_event(ui::EventType::kMouseExited, gfx::Point(),
                            gfx::Point(), ui::EventTimeForNow(), 0, 0);
  reload_button()->OnMouseExited(exit_event);
  EXPECT_EQ(reload_button()->GetState(), views::Button::STATE_NORMAL);

  // Paint again, should not record.
  SimulatePaint(base::TimeTicks() + base::Microseconds(2));
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.MouseHoverToNextPaint", 1);

  // Enter again.
  reload_button()->OnMouseEntered(enter_event);
  // Paint again, should record.
  SimulatePaint(base::TimeTicks() + base::Microseconds(3));
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.MouseHoverToNextPaint", 2);
}

// Verifies that the latency from an input event (both mouse release and key
// press) to the next paint of the button is recorded.
TEST_F(ReloadButtonMetricsTest, LogInputToNextPaintMetric) {
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease", 0);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 0);

  // Test mouse input event.
  reload_button()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed, {0, 0}));
  reload_button()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased, {0, 0}));
  SimulatePaint(base::TimeTicks() + base::Microseconds(1));
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease", 1);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 0);

  // TODO(crbug.com/448794588): Test or remove key input event.
}

// Verifies that the latency from a mouse input event to the execution of the
// reload command is recorded.
// TODO(crbug.com/448794588): Test or remove key input event.
TEST_F(ReloadButtonMetricsTest, LogMouseInputToReloadMetric) {
  // The button defaults to Reload mode.
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToStop.MouseRelease", 0);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToStop.KeyPress", 0);

  // Test mouse click to Reload.
  reload_button()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed, {0, 0}));
  reload_button()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased, {0, 0}));
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToReload.MouseRelease", 1);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToReload.KeyPress", 0);
}

// Verifies that the latency from a mouse input event to the execution of the
// stop command is recorded.
// TODO(crbug.com/448794588): Test or remove key input event.
TEST_F(ReloadButtonMetricsTest, LogMouseInputToStopMetric) {
  // Sets the button to Stop mode to test input on the Stop button.
  reload_button()->ChangeMode(ReloadButton::Mode::kStop, true);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToStop.MouseRelease", 0);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToStop.KeyPress", 0);

  // Test mouse click to Stop.
  reload_button()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed, {0, 0}));
  reload_button()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased, {0, 0}));
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToStop.MouseRelease", 1);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToStop.KeyPress", 0);
}

// Ensures that the latency from changing the button's visible mode to the next
// paint is correctly logged for both Stop and Reload modes.
TEST_F(ReloadButtonMetricsTest, LogChangeVisibleModeToNextPaintMetric) {
  gfx::Canvas canvas(gfx::Size(20, 20), 1.0f, false);

  // 1. Test change to Stop.
  reload_button()->ChangeMode(ReloadButton::Mode::kStop, true);
  SimulatePaint(base::TimeTicks() + base::Microseconds(1));
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInStop", 1);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInReload", 0);

  // 2. Test change to Reload.
  reload_button()->ChangeMode(ReloadButton::Mode::kReload, true);
  SimulatePaint(base::TimeTicks() + base::Microseconds(2));
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInStop", 1);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInReload", 1);
}

// Verifies that the InputCount for different input types (mouse release and
// key press) is correctly recorded.
TEST_F(ReloadButtonMetricsTest, LogInputCountMetric) {
  // Simulate a mouse click.
  reload_button()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed, {0, 0}));
  reload_button()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased, {0, 0}));
  histogram_tester().ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease, 1);
  histogram_tester().ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress, 0);

  // TODO(crbug.com/448794588): Test or remove key input event.
}
