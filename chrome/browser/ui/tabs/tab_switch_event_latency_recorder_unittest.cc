// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_switch_event_latency_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabSwitchEventLatencyRecorderTest : public testing::Test {
 public:
  ~TabSwitchEventLatencyRecorderTest() override {}

  void SetUp() override {
    EXPECT_EQ(histogram_tester
                  .GetAllSamples("Browser.Tabs.InputEventToSelectionTime.Mouse")
                  .size(),
              0ULL);
    EXPECT_EQ(
        histogram_tester
            .GetAllSamples("Browser.Tabs.InputEventToSelectionTime.Keyboard")
            .size(),
        0ULL);
    EXPECT_EQ(histogram_tester
                  .GetAllSamples("Browser.Tabs.InputEventToSelectionTime.Touch")
                  .size(),
              0ULL);
    EXPECT_EQ(histogram_tester
                  .GetAllSamples("Browser.Tabs.InputEventToSelectionTime.Wheel")
                  .size(),
              0ULL);
  }

 protected:
  size_t GetHistogramSampleSize(
      TabStripUserGestureDetails::GestureType gesture_type) {
    switch (gesture_type) {
      case TabStripUserGestureDetails::GestureType::kMouse:
        return histogram_tester
            .GetAllSamples("Browser.Tabs.InputEventToSelectionTime.Mouse")
            .size();
      case TabStripUserGestureDetails::GestureType::kKeyboard:
        return histogram_tester
            .GetAllSamples("Browser.Tabs.InputEventToSelectionTime.Keyboard")
            .size();
      case TabStripUserGestureDetails::GestureType::kTouch:
        return histogram_tester
            .GetAllSamples("Browser.Tabs.InputEventToSelectionTime.Touch")
            .size();
      case TabStripUserGestureDetails::GestureType::kWheel:
        return histogram_tester
            .GetAllSamples("Browser.Tabs.InputEventToSelectionTime.Wheel")
            .size();
      default:
        return 0;
    }
  }

  TabSwitchEventLatencyRecorder tab_switch_event_latency_recorder_;
  base::HistogramTester histogram_tester;
};

// Mouse input event latency is recorded to histogram
TEST_F(TabSwitchEventLatencyRecorderTest, MouseInputLatency) {
  const auto now = base::TimeTicks::Now();

  tab_switch_event_latency_recorder_.BeginLatencyTiming(
      TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kMouse, now));
  tab_switch_event_latency_recorder_.OnWillChangeActiveTab(
      base::TimeTicks::Now());
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kMouse),
      1ULL);
  EXPECT_EQ(GetHistogramSampleSize(
                TabStripUserGestureDetails::GestureType::kKeyboard),
            0ULL);
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kTouch),
      0ULL);
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kWheel),
      0ULL);
}

// Keyboard input event latency is recorded to histogram
TEST_F(TabSwitchEventLatencyRecorderTest, KeyboardInputLatency) {
  const auto now = base::TimeTicks::Now();

  tab_switch_event_latency_recorder_.BeginLatencyTiming(
      TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kKeyboard, now));
  tab_switch_event_latency_recorder_.OnWillChangeActiveTab(
      base::TimeTicks::Now());
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kMouse),
      0ULL);
  EXPECT_EQ(GetHistogramSampleSize(
                TabStripUserGestureDetails::GestureType::kKeyboard),
            1ULL);
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kTouch),
      0ULL);
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kWheel),
      0ULL);
}

// Touch input event latency is recorded to histogram
TEST_F(TabSwitchEventLatencyRecorderTest, TouchInputLatency) {
  const auto now = base::TimeTicks::Now();

  tab_switch_event_latency_recorder_.BeginLatencyTiming(
      TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kTouch, now));
  tab_switch_event_latency_recorder_.OnWillChangeActiveTab(
      base::TimeTicks::Now());
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kMouse),
      0ULL);
  EXPECT_EQ(GetHistogramSampleSize(
                TabStripUserGestureDetails::GestureType::kKeyboard),
            0ULL);
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kTouch),
      1ULL);
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kWheel),
      0ULL);
}

// Scroll wheel input event latency is recorded to histogram
TEST_F(TabSwitchEventLatencyRecorderTest, WheelInputLatency) {
  const auto now = base::TimeTicks::Now();

  tab_switch_event_latency_recorder_.BeginLatencyTiming(
      TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kWheel, now));
  tab_switch_event_latency_recorder_.OnWillChangeActiveTab(
      base::TimeTicks::Now());
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kMouse),
      0ULL);
  EXPECT_EQ(GetHistogramSampleSize(
                TabStripUserGestureDetails::GestureType::kKeyboard),
            0ULL);
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kTouch),
      0ULL);
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kWheel),
      1ULL);
}

// Other input event type is not recorded to histogram
TEST_F(TabSwitchEventLatencyRecorderTest, OtherInputLatency) {
  const auto now = base::TimeTicks::Now();

  tab_switch_event_latency_recorder_.BeginLatencyTiming(
      TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kOther, now));
  tab_switch_event_latency_recorder_.OnWillChangeActiveTab(
      base::TimeTicks::Now());
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kMouse),
      0ULL);
  EXPECT_EQ(GetHistogramSampleSize(
                TabStripUserGestureDetails::GestureType::kKeyboard),
            0ULL);
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kTouch),
      0ULL);
  EXPECT_EQ(
      GetHistogramSampleSize(TabStripUserGestureDetails::GestureType::kWheel),
      0ULL);
}
