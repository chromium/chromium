// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/click_input_tracker.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace page_load_metrics {

class ClickInputTrackerTest : public testing::Test {
 public:
  ClickInputTrackerTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_current_default_handle_(task_runner_) {}

  ClickInputTrackerTest(const ClickInputTrackerTest&) = delete;
  ClickInputTrackerTest& operator=(const ClickInputTrackerTest&) = delete;

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
};

TEST_F(ClickInputTrackerTest, OnUserInputGestureTapClickBurst) {
  ClickInputTracker click_tracker;

  base::TimeTicks timestamp = base::TimeTicks::Now();
  blink::WebGestureEvent tap1(blink::WebInputEvent::Type::kGestureTap, 0,
                              timestamp);
  tap1.SetPositionInScreen(gfx::PointF(100, 200));
  click_tracker.OnUserInput(tap1);
  EXPECT_EQ(1, click_tracker.GetCurrentBurstCountForTesting());

  timestamp += base::Milliseconds(100);
  blink::WebGestureEvent tap2(blink::WebInputEvent::Type::kGestureTap, 0,
                              timestamp);
  tap2.SetPositionInScreen(gfx::PointF(103, 198));
  click_tracker.OnUserInput(tap2);
  EXPECT_EQ(2, click_tracker.GetCurrentBurstCountForTesting());

  timestamp += base::Milliseconds(200);
  blink::WebGestureEvent tap3(blink::WebInputEvent::Type::kGestureTap, 0,
                              timestamp);
  tap3.SetPositionInScreen(gfx::PointF(99, 202));
  click_tracker.OnUserInput(tap3);
  EXPECT_EQ(3, click_tracker.GetCurrentBurstCountForTesting());

  timestamp += base::Milliseconds(300);
  blink::WebGestureEvent tap4(blink::WebInputEvent::Type::kGestureTap, 0,
                              timestamp);
  tap4.SetPositionInScreen(gfx::PointF(101, 201));
  click_tracker.OnUserInput(tap4);
  EXPECT_EQ(4, click_tracker.GetCurrentBurstCountForTesting());

  // Now exceed time delta threshold.
  timestamp += base::Milliseconds(800);
  blink::WebGestureEvent tap5(blink::WebInputEvent::Type::kGestureTap, 0,
                              timestamp);
  tap5.SetPositionInScreen(gfx::PointF(101, 201));
  click_tracker.OnUserInput(tap5);
  EXPECT_EQ(1, click_tracker.GetCurrentBurstCountForTesting());
  EXPECT_EQ(4, click_tracker.GetMaxBurstCountForTesting());

  timestamp += base::Milliseconds(100);
  blink::WebGestureEvent tap6(blink::WebInputEvent::Type::kGestureTap, 0,
                              timestamp);
  tap6.SetPositionInScreen(gfx::PointF(103, 198));
  click_tracker.OnUserInput(tap6);
  EXPECT_EQ(2, click_tracker.GetCurrentBurstCountForTesting());
  EXPECT_EQ(4, click_tracker.GetMaxBurstCountForTesting());

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ukm::SourceId ukm_source_id = ukm::SourceId(1);
  click_tracker.RecordClickBurst(ukm_source_id);
  histogram_tester.ExpectUniqueSample("PageLoad.Experimental.ClickInputBurst",
                                      4, 1);

  // Verify UKM entry.
  task_runner_->RunUntilIdle();
  using UkmEntry = ukm::builders::ClickInput;
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries.at(0).get();
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmEntry::kExperimental_ClickInputBurstName, 4);
}

TEST_F(ClickInputTrackerTest, OnUserInputMouseUpClickBurst) {
  ClickInputTracker click_tracker;

  base::TimeTicks timestamp = base::TimeTicks::Now();
  blink::WebMouseEvent click1(blink::WebInputEvent::Type::kMouseUp, 0,
                              timestamp);
  click1.SetPositionInScreen(gfx::PointF(100, 200));
  click_tracker.OnUserInput(click1);
  EXPECT_EQ(1, click_tracker.GetCurrentBurstCountForTesting());

  timestamp += base::Milliseconds(100);
  blink::WebMouseEvent click2(blink::WebInputEvent::Type::kMouseUp, 0,
                              timestamp);
  click2.SetPositionInScreen(gfx::PointF(103, 198));
  click_tracker.OnUserInput(click2);
  EXPECT_EQ(2, click_tracker.GetCurrentBurstCountForTesting());

  timestamp += base::Milliseconds(200);
  blink::WebMouseEvent click3(blink::WebInputEvent::Type::kMouseUp, 0,
                              timestamp);
  click3.SetPositionInScreen(gfx::PointF(99, 202));
  click_tracker.OnUserInput(click3);
  EXPECT_EQ(3, click_tracker.GetCurrentBurstCountForTesting());

  timestamp += base::Milliseconds(300);
  blink::WebMouseEvent click4(blink::WebInputEvent::Type::kMouseUp, 0,
                              timestamp);
  click4.SetPositionInScreen(gfx::PointF(101, 201));
  click_tracker.OnUserInput(click4);
  EXPECT_EQ(4, click_tracker.GetCurrentBurstCountForTesting());

  // Now exceed position delta threshold.
  timestamp += base::Milliseconds(100);
  blink::WebMouseEvent click5(blink::WebInputEvent::Type::kMouseUp, 0,
                              timestamp);
  click5.SetPositionInScreen(gfx::PointF(151, 201));
  click_tracker.OnUserInput(click5);
  EXPECT_EQ(1, click_tracker.GetCurrentBurstCountForTesting());
  EXPECT_EQ(4, click_tracker.GetMaxBurstCountForTesting());

  timestamp += base::Milliseconds(100);
  blink::WebMouseEvent click6(blink::WebInputEvent::Type::kMouseUp, 0,
                              timestamp);
  click6.SetPositionInScreen(gfx::PointF(153, 198));
  click_tracker.OnUserInput(click6);
  EXPECT_EQ(2, click_tracker.GetCurrentBurstCountForTesting());
  EXPECT_EQ(4, click_tracker.GetMaxBurstCountForTesting());

  timestamp += base::Milliseconds(100);
  blink::WebMouseEvent click7(blink::WebInputEvent::Type::kMouseUp, 0,
                              timestamp);
  click7.SetPositionInScreen(gfx::PointF(153, 198));
  click_tracker.OnUserInput(click7);
  EXPECT_EQ(3, click_tracker.GetCurrentBurstCountForTesting());
  EXPECT_EQ(4, click_tracker.GetMaxBurstCountForTesting());

  timestamp += base::Milliseconds(100);
  blink::WebMouseEvent click8(blink::WebInputEvent::Type::kMouseUp, 0,
                              timestamp);
  click8.SetPositionInScreen(gfx::PointF(153, 198));
  click_tracker.OnUserInput(click8);
  EXPECT_EQ(4, click_tracker.GetCurrentBurstCountForTesting());
  EXPECT_EQ(4, click_tracker.GetMaxBurstCountForTesting());

  timestamp += base::Milliseconds(100);
  blink::WebMouseEvent click9(blink::WebInputEvent::Type::kMouseUp, 0,
                              timestamp);
  click9.SetPositionInScreen(gfx::PointF(153, 198));
  click_tracker.OnUserInput(click9);
  EXPECT_EQ(5, click_tracker.GetCurrentBurstCountForTesting());
  EXPECT_EQ(5, click_tracker.GetMaxBurstCountForTesting());

  base::HistogramTester histogram_tester;
  ukm::SourceId ukm_source_id = ukm::SourceId(1);
  click_tracker.RecordClickBurst(ukm_source_id);
  histogram_tester.ExpectUniqueSample("PageLoad.Experimental.ClickInputBurst",
                                      5, 1);
}

}  // namespace page_load_metrics
