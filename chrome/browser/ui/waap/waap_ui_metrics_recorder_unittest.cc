// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_ui_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/button.h"

class WaapUIMetricsRecorderTest : public testing::Test {
 public:
  WaapUIMetricsRecorderTest() {
    // WaapUIMetricsService is only available when the feature is enabled.
    feature_list_.InitAndEnableFeature(features::kInitialWebUIMetrics);
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    WaapUIMetricsServiceFactory::GetInstance();
    WaapUIMetricsService::Get(profile_.get());
    recorder_ = std::make_unique<WaapUIMetricsRecorder>(profile_.get());
  }

  void TearDown() override {
    recorder_.reset();
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<WaapUIMetricsRecorder> recorder_;
};

// Tests that the time from mouse hover to the next paint is recorded.
TEST_F(WaapUIMetricsRecorderTest, MouseHoverToNextPaint) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.MouseHoverToNextPaint", 0);

  auto start_time = base::TimeTicks::Now();
  recorder_->OnMouseEntered(start_time);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto end_time = base::TimeTicks::Now();

  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_HOVERED, end_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.MouseHoverToNextPaint", end_time - start_time,
      1);

  // The hover time should be reset. Another OnPaint should not trigger metric.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_HOVERED, base::TimeTicks::Now());
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.MouseHoverToNextPaint", 1);
}

// Tests that if the mouse exits before the next paint, no metric is recorded.
TEST_F(WaapUIMetricsRecorderTest, MouseHoverExitedToNextPaint) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.MouseHoverToNextPaint", 0);

  auto start_time = base::TimeTicks::Now();
  recorder_->OnMouseEntered(start_time);

  task_environment_.FastForwardBy(base::Seconds(1));
  recorder_->OnMouseExited(base::TimeTicks::Now());

  task_environment_.FastForwardBy(base::Seconds(1));
  auto end_time = base::TimeTicks::Now();

  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_HOVERED, end_time);
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.MouseHoverToNextPaint", 0);
}

// Tests that the time from mouse press to the next paint is recorded.
TEST_F(WaapUIMetricsRecorderTest, MousePressToNextPaint) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.MousePressToNextPaint", 0);

  auto start_time = base::TimeTicks::Now();
  recorder_->OnMousePressed(start_time);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto end_time = base::TimeTicks::Now();

  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_PRESSED, end_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.MousePressToNextPaint", end_time - start_time,
      1);

  // The press time should be reset. Another OnPaint should not trigger metric.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_PRESSED, base::TimeTicks::Now());
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.MousePressToNextPaint", 1);
}

// Tests that if the mouse is released before the next paint, no metric is
// recorded.
TEST_F(WaapUIMetricsRecorderTest, MousePressReleasedToNextPaint) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.MousePressToNextPaint", 0);

  auto start_time = base::TimeTicks::Now();
  recorder_->OnMousePressed(start_time);

  task_environment_.FastForwardBy(base::Seconds(1));
  recorder_->OnMouseReleased(base::TimeTicks::Now());

  task_environment_.FastForwardBy(base::Seconds(1));
  auto end_time = base::TimeTicks::Now();

  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_PRESSED, end_time);
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.MousePressToNextPaint", 0);
}

// Tests that the input count and the time from input to the next paint are
// recorded for key press.
TEST_F(WaapUIMetricsRecorderTest, InputToNextPaint_KeyPress) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 0);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress, 0);

  auto start_time = ui::EventTimeForNow();
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                         ui::DomCode::ENTER, 0, start_time);

  recorder_->OnButtonPressedStart(
      key_event, WaapUIMetricsRecorder::ReloadButtonMode::kReload);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress, 1);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto end_time = base::TimeTicks::Now();

  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_NORMAL, end_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress",
      end_time - start_time, 1);

  // last_input_info_ should be reset.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_NORMAL, base::TimeTicks::Now());
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 1);
}

// Tests that the input count and the time from input to the next paint are
// recorded for mouse release.
TEST_F(WaapUIMetricsRecorderTest, InputToNextPaint_MouseRelease) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease", 0);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease, 0);

  auto start_time = ui::EventTimeForNow();
  ui::MouseEvent mouse_event(ui::EventType::kMouseReleased, gfx::Point(),
                             gfx::Point(), start_time, 0, 0);

  recorder_->OnButtonPressedStart(
      mouse_event, WaapUIMetricsRecorder::ReloadButtonMode::kReload);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease, 1);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto end_time = base::TimeTicks::Now();

  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_NORMAL, end_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease",
      end_time - start_time, 1);

  // last_input_info_ should be reset.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_NORMAL, base::TimeTicks::Now());
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease", 1);
}

// Tests that when there are multiple inputs before a paint, only the last input
// is recorded.
TEST_F(WaapUIMetricsRecorderTest, InputToNextPaint_MultipleInputs) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 0);
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease", 0);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress, 0);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease, 0);

  // 1. KeyPress event.
  auto key_press_time = ui::EventTimeForNow();
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                         ui::DomCode::ENTER, 0, key_press_time);
  recorder_->OnButtonPressedStart(
      key_event, WaapUIMetricsRecorder::ReloadButtonMode::kReload);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress, 1);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease, 0);

  task_environment_.FastForwardBy(base::Seconds(1));

  // 2. MouseRelease event.
  auto mouse_release_time = ui::EventTimeForNow();
  ui::MouseEvent mouse_event(ui::EventType::kMouseReleased, gfx::Point(),
                             gfx::Point(), mouse_release_time, 0, 0);

  recorder_->OnButtonPressedStart(
      mouse_event, WaapUIMetricsRecorder::ReloadButtonMode::kReload);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress, 1);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease, 1);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto paint_time = base::TimeTicks::Now();

  // 3. Paint.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_NORMAL, paint_time);

  // Only the last input (MouseRelease) should be recorded for InputToNextPaint.
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 0);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease",
      paint_time - mouse_release_time, 1);

  // last_input_info_ should be reset.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_NORMAL, base::TimeTicks::Now());
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 0);
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease", 1);
}

// Tests that the input count, the time from input to stop, and the time from
// input to the next paint are recorded.
TEST_F(WaapUIMetricsRecorderTest, InputToStopAndPaintWithMouseRelease) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToStop.MouseRelease", 0);
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease", 0);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease, 0);

  auto start_time = ui::EventTimeForNow();
  ui::MouseEvent mouse_event(ui::EventType::kMouseReleased, gfx::Point(),
                             gfx::Point(), start_time, 0, 0);

  recorder_->OnButtonPressedStart(
      mouse_event, WaapUIMetricsRecorder::ReloadButtonMode::kStop);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease, 1);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto stop_time = base::TimeTicks::Now();

  recorder_->DidExecuteStopCommand(stop_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.InputToStop.MouseRelease",
      stop_time - start_time, 1);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto paint_time = base::TimeTicks::Now();

  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kStop,
      views::Button::STATE_NORMAL, paint_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease",
      paint_time - start_time, 1);

  // After OnPaint, last_input_info_ should be reset, so DidExecuteStopCommand
  // should do nothing.
  recorder_->DidExecuteStopCommand(base::TimeTicks::Now());
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToStop.MouseRelease", 1);
}

// Tests that the input count, the time from input to reload, and the time from
// input to the next paint are recorded.
TEST_F(WaapUIMetricsRecorderTest, InputToReloadAndPaintWithMouseRelease) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToReload.MouseRelease", 0);
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease", 0);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease, 0);

  auto start_time = ui::EventTimeForNow();
  ui::MouseEvent mouse_event(ui::EventType::kMouseReleased, gfx::Point(),
                             gfx::Point(), start_time, 0, 0);

  recorder_->OnButtonPressedStart(
      mouse_event, WaapUIMetricsRecorder::ReloadButtonMode::kReload);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease, 1);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto reload_time = base::TimeTicks::Now();

  recorder_->DidExecuteReloadCommand(reload_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.InputToReload.MouseRelease",
      reload_time - start_time, 1);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto paint_time = base::TimeTicks::Now();

  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_NORMAL, paint_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease",
      paint_time - start_time, 1);

  // After OnPaint, last_input_info_ should be reset, so
  // DidExecuteReloadCommand should do nothing.
  recorder_->DidExecuteReloadCommand(base::TimeTicks::Now());
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToReload.MouseRelease", 1);
}

// Tests that the input count, the time from input to stop, and the time from
// input to the next paint are recorded, for a key press event.
TEST_F(WaapUIMetricsRecorderTest, InputToStopAndPaintWithKeyPress) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToStop.KeyPress", 0);
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 0);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress, 0);

  auto start_time = ui::EventTimeForNow();
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                         ui::DomCode::ENTER, 0, start_time);

  recorder_->OnButtonPressedStart(
      key_event, WaapUIMetricsRecorder::ReloadButtonMode::kStop);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress, 1);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto stop_time = base::TimeTicks::Now();

  recorder_->DidExecuteStopCommand(stop_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.InputToStop.KeyPress", stop_time - start_time,
      1);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto paint_time = base::TimeTicks::Now();

  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kStop,
      views::Button::STATE_NORMAL, paint_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress",
      paint_time - start_time, 1);

  // After OnPaint, last_input_info_ should be reset, so DidExecuteStopCommand
  // should do nothing.
  recorder_->DidExecuteStopCommand(base::TimeTicks::Now());
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToStop.KeyPress", 1);
}

// Tests that the input count, the time from input to reload, and the time from
// input to the next paint are recorded, for a key press event.
TEST_F(WaapUIMetricsRecorderTest, InputToReloadAndPaintWithKeyPress) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToReload.KeyPress", 0);
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress", 0);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress, 0);

  auto start_time = ui::EventTimeForNow();
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                         ui::DomCode::ENTER, 0, start_time);

  recorder_->OnButtonPressedStart(
      key_event, WaapUIMetricsRecorder::ReloadButtonMode::kReload);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress, 1);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto reload_time = base::TimeTicks::Now();

  recorder_->DidExecuteReloadCommand(reload_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.InputToReload.KeyPress",
      reload_time - start_time, 1);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto paint_time = base::TimeTicks::Now();

  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_NORMAL, paint_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.InputToNextPaint.KeyPress",
      paint_time - start_time, 1);

  // After OnPaint, last_input_info_ should be reset, so
  // DidExecuteReloadCommand should do nothing.
  recorder_->DidExecuteReloadCommand(base::TimeTicks::Now());
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.InputToReload.KeyPress", 1);
}

// Tests that the time from a change in visible mode to the next paint is
// recorded.
// Simulates logging for Reload icon -> Stop icon.
TEST_F(WaapUIMetricsRecorderTest, ChangeVisibleModeToNextPaintInStop) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInStop", 0);

  auto start_time = base::TimeTicks::Now();
  recorder_->OnChangeVisibleMode(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      WaapUIMetricsRecorder::ReloadButtonMode::kStop, start_time);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto end_time = base::TimeTicks::Now();

  // OnPaint with different mode should not trigger metric.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_NORMAL, end_time);
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInStop", 0);

  task_environment_.FastForwardBy(base::Seconds(1));
  end_time = base::TimeTicks::Now();

  // OnPaint with target mode should trigger metric.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kStop,
      views::Button::STATE_NORMAL, end_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInStop",
      end_time - start_time, 1);

  // pending_mode_change_ should be reset.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kStop,
      views::Button::STATE_NORMAL, base::TimeTicks::Now());
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInStop", 1);
}

// Tests that the time from a change in visible mode to the next paint is
// recorded.
// Simulates logging for Stop icon -> Reload icon.
TEST_F(WaapUIMetricsRecorderTest, ChangeVisibleModeToNextPaintInReload) {
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInReload", 0);

  auto start_time = base::TimeTicks::Now();
  recorder_->OnChangeVisibleMode(
      WaapUIMetricsRecorder::ReloadButtonMode::kStop,
      WaapUIMetricsRecorder::ReloadButtonMode::kReload, start_time);

  task_environment_.FastForwardBy(base::Seconds(1));
  auto end_time = base::TimeTicks::Now();

  // OnPaint with different mode should not trigger metric.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kStop,
      views::Button::STATE_NORMAL, end_time);
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInReload", 0);

  task_environment_.FastForwardBy(base::Seconds(1));
  end_time = base::TimeTicks::Now();

  // OnPaint with target mode should trigger metric.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_NORMAL, end_time);
  histogram_tester_.ExpectTimeBucketCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInReload",
      end_time - start_time, 1);

  // pending_mode_change_ should be reset.
  recorder_->OnPaintFramePresented(
      WaapUIMetricsRecorder::ReloadButtonMode::kReload,
      views::Button::STATE_NORMAL, base::TimeTicks::Now());
  histogram_tester_.ExpectTotalCount(
      "InitialWebUI.ReloadButton.ChangeVisibleModeToNextPaintInReload", 1);
}
