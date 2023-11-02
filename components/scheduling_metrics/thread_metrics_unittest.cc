// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduling_metrics/thread_metrics.h"

#include "base/task/sequence_manager/test/fake_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time_override.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::sequence_manager::TaskQueue;

namespace scheduling_metrics {

namespace {

using base::sequence_manager::FakeTask;
using base::sequence_manager::FakeTaskTiming;

base::TimeTicks Seconds(int seconds) {
  return base::TimeTicks() + base::Seconds(seconds);
}

base::ThreadTicks ThreadSeconds(int seconds) {
  return base::ThreadTicks() + base::Seconds(seconds);
}

}  // namespace

TEST(MetricsHelperTest, TaskDurationPerThreadType) {
  base::HistogramTester histogram_tester;

  ThreadMetrics main_thread_metrics(ThreadType::kRendererMainThread,
                                    false /* has_cpu_timing_for_each_task */);
  ThreadMetrics compositor_metrics(ThreadType::kRendererCompositorThread,
                                   false /* has_cpu_timing_for_each_task */);
  ThreadMetrics worker_metrics(ThreadType::kRendererOtherBlinkThread,
                               false /* has_cpu_timing_for_each_task */);

  main_thread_metrics.RecordTaskMetrics(
      FakeTask(), FakeTaskTiming(Seconds(10), Seconds(50), ThreadSeconds(0),
                                 ThreadSeconds(15)));
  compositor_metrics.RecordTaskMetrics(
      FakeTask(), FakeTaskTiming(Seconds(10), Seconds(80), ThreadSeconds(0),
                                 ThreadSeconds(5)));
  compositor_metrics.RecordTaskMetrics(
      FakeTask(), FakeTaskTiming(Seconds(100), Seconds(200)));
  worker_metrics.RecordTaskMetrics(
      FakeTask(), FakeTaskTiming(Seconds(10), Seconds(125), ThreadSeconds(0),
                                 ThreadSeconds(25)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Scheduler.Experimental.WallTimePerThread"),
      testing::UnorderedElementsAre(
          base::Bucket(static_cast<int>(ThreadType::kRendererMainThread), 40),
          base::Bucket(static_cast<int>(ThreadType::kRendererCompositorThread),
                       170),
          base::Bucket(static_cast<int>(ThreadType::kRendererOtherBlinkThread),
                       115)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Scheduler.Experimental.CPUTimePerThread"),
      testing::UnorderedElementsAre(
          base::Bucket(static_cast<int>(ThreadType::kRendererMainThread), 15),
          base::Bucket(static_cast<int>(ThreadType::kRendererCompositorThread),
                       5),
          base::Bucket(static_cast<int>(ThreadType::kRendererOtherBlinkThread),
                       25)));
}

TEST(MetricsHelperTest, TrackedCPUTimeMetrics) {
  base::HistogramTester histogram_tester;
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::Time(); }, []() { return Seconds(1); },
      []() { return ThreadSeconds(1); });

  ThreadMetrics main_thread_metrics(ThreadType::kRendererMainThread,
                                    true /* has_cpu_timing_for_each_task */);

  main_thread_metrics.RecordTaskMetrics(
      FakeTask(), FakeTaskTiming(Seconds(10), Seconds(50), ThreadSeconds(5),
                                 ThreadSeconds(15)));
  main_thread_metrics.RecordTaskMetrics(
      FakeTask(), FakeTaskTiming(Seconds(10), Seconds(50), ThreadSeconds(20),
                                 ThreadSeconds(25)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Scheduler.Experimental.CPUTimePerThread.Tracked"),
              testing::UnorderedElementsAre(base::Bucket(
                  static_cast<int>(ThreadType::kRendererMainThread), 15)));
  // 9 = 4 seconds before task 1 and 5 seconds between tasks 1 and 2.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Scheduler.Experimental.CPUTimePerThread.Untracked"),
              testing::UnorderedElementsAre(base::Bucket(
                  static_cast<int>(ThreadType::kRendererMainThread), 9)));
}

}  // namespace scheduling_metrics
