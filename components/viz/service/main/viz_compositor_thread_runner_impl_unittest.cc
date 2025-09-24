// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/main/viz_compositor_thread_runner_impl.h"

#include "base/synchronization/waitable_event.h"
#include "base/test/manual_hang_watcher.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/threading_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

using ::base::Bucket;
using ::base::BucketsAre;
using ::base::HangWatcher;
using ::base::test::ManualHangWatcher;
using ::base::test::ScopedFeatureList;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

void WaitForThreadRunnerToStart(VizCompositorThreadRunnerImpl& thread_runner) {
  base::WaitableEvent thread_started;
  thread_runner.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                Unretained(&thread_started)));
  thread_started.Wait();
}

TEST(VizCompositorThreadRunnerImplTest, HangWatcherDisabledByDefault) {
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kGPUProcess);
  VizCompositorThreadRunnerImpl thread_runner;
  WaitForThreadRunnerToStart(thread_runner);
  EXPECT_FALSE(hang_watcher.IsWatchingThreads());
}

TEST(VizCompositorThreadRunnerImplTest, HangWatcherFeatureEnabled) {
  ScopedFeatureList enable_gpu_watcher(base::kEnableHangWatcherOnGpuProcess);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kGPUProcess);
  VizCompositorThreadRunnerImpl thread_runner;
  WaitForThreadRunnerToStart(thread_runner);
  EXPECT_TRUE(hang_watcher.IsWatchingThreads());
}

TEST(VizCompositorThreadRunnerImplTest,
     HangWatcherStopsWatchingWhenCompositorThreadExists) {
  ScopedFeatureList enable_gpu_watcher(base::kEnableHangWatcherOnGpuProcess);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kGPUProcess);

  // Create a compositor thread in a scope so that it gets destroyed.
  {
    base::HistogramTester histogram_tester;
    VizCompositorThreadRunnerImpl thread_runner;
    WaitForThreadRunnerToStart(thread_runner);
    ASSERT_TRUE(hang_watcher.IsWatchingThreads());
  }

  ASSERT_FALSE(hang_watcher.IsWatchingThreads());
}

TEST(VizCompositorThreadRunnerImplTest, HangWatcherWatchesCompositorThread) {
  ScopedFeatureList enable_gpu_watcher(base::kEnableHangWatcherOnGpuProcess);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kGPUProcess);

  // Create the compositor thread.
  VizCompositorThreadRunnerImpl thread_runner;
  WaitForThreadRunnerToStart(thread_runner);

  // Run hang monitoring in the process.
  base::HistogramTester histogram_tester;
  hang_watcher.TriggerSynchronousMonitoring();

  // The compositor thread isn't hung, so a `false` bucket should be recorded.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "HangWatcher.IsThreadHung.GpuProcess.CompositorThread"),
              BucketsAre(Bucket(false, 1)));
}

}  // namespace
}  // namespace viz
