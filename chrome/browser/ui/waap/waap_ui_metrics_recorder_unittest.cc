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

// Tests that the input count and the time from input to the next paint are
// recorded for key press.
TEST_F(WaapUIMetricsRecorderTest, InputCount) {
  histogram_tester_.ExpectTotalCount("InitialWebUI.ReloadButton.InputCount", 0);

  auto start_time = ui::EventTimeForNow();
  ui::MouseEvent mouse_event(ui::EventType::kMouseReleased, gfx::Point(),
                             gfx::Point(), start_time, 0, 0);

  recorder_->OnButtonPressedStart(mouse_event);
  histogram_tester_.ExpectBucketCount(
      "InitialWebUI.ReloadButton.InputCount",
      WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease, 1);
}
