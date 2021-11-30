// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_controller.h"

#include <atomic>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace heap_profiling {

class HeapProfilerControllerTest : public ::testing::Test {
 protected:
  HeapProfilerControllerTest() {
    feature_list_.InitAndEnableFeature(
        metrics::CallStackProfileMetricsProvider::kHeapProfilerReporting);
    // Clear any samples set in the global SamplingHeapProfiler before the
    // ScopedMuteHookedSamplesForTesting was created.
    base::SamplingHeapProfiler::Get()->ClearSamplesForTesting();
    base::PoissonAllocationSampler::Get()->SuppressRandomnessForTest(true);
  }

  ~HeapProfilerControllerTest() override {
    base::PoissonAllocationSampler::Get()->SuppressRandomnessForTest(false);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  base::PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting mute_hooks_;
};

// A wrapper that sets up a HeapProfilerController for testing.
class HeapProfilerControllerTester {
 public:
  explicit HeapProfilerControllerTester(
      base::RepeatingCallback<void(base::TimeTicks, metrics::SampledProfile)>
          receiver_callback) {
    controller_.SuppressRandomnessForTesting();
    metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
        std::move(receiver_callback));
  }

  ~HeapProfilerControllerTester() {
    metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
        base::DoNothing());
  }

  HeapProfilerControllerTester(const HeapProfilerControllerTester&) = delete;
  HeapProfilerControllerTester& operator=(const HeapProfilerControllerTester&) =
      delete;

  HeapProfilerController& controller() { return controller_; }

 private:
  HeapProfilerController controller_;
};

TEST_F(HeapProfilerControllerTest, EmptyProfileIsNotEmitted) {
  HeapProfilerControllerTester tester(base::BindLambdaForTesting(
      [](base::TimeTicks time, metrics::SampledProfile profile) {
        ADD_FAILURE();
      }));
  tester.controller().Start();

  task_environment_.FastForwardBy(base::Days(365));
}

// Sampling profiler is not capable of unwinding stack on Android under tests.
#if !defined(OS_ANDROID)
TEST_F(HeapProfilerControllerTest, ProfileCollectionsScheduler) {
  constexpr size_t kSamplingRate = 1024;
  constexpr size_t kAllocationSize = 42 * kSamplingRate;
  constexpr int kSnapshotsToCollect = 3;

  std::atomic<int> profile_count{0};
  auto check_profile = [&](base::TimeTicks time,
                           metrics::SampledProfile profile) {
    EXPECT_EQ(metrics::SampledProfile::PERIODIC_HEAP_COLLECTION,
              profile.trigger_event());
    EXPECT_LT(0, profile.call_stack_profile().stack_sample_size());

    bool found = false;
    for (const metrics::CallStackProfile::StackSample& sample :
         profile.call_stack_profile().stack_sample()) {
      // Check that the samples being reported are the allocations created
      // below. The sampler calculates the average weight of each sample, and
      // sometimes rounds up to more than kAllocationSize.
      if (sample.has_weight() &&
          static_cast<size_t>(sample.weight()) >= kAllocationSize) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
    ++profile_count;
  };

  base::SamplingHeapProfiler::Get()->SetSamplingInterval(kSamplingRate);

  HeapProfilerControllerTester tester(
      base::BindLambdaForTesting(check_profile));
  tester.controller().Start();

  auto* sampler = base::PoissonAllocationSampler::Get();
  sampler->RecordAlloc(reinterpret_cast<void*>(0x1337), kAllocationSize,
                       base::PoissonAllocationSampler::kManualForTesting,
                       nullptr);
  sampler->RecordAlloc(reinterpret_cast<void*>(0x7331), kAllocationSize,
                       base::PoissonAllocationSampler::kManualForTesting,
                       nullptr);

  // The profiler should continue to collect snapshots as long as this memory is
  // allocated. If not the test will time out.
  while (profile_count < kSnapshotsToCollect) {
    task_environment_.FastForwardBy(base::Days(1));
  }

  // Free all recorded memory so the address list is empty for the next test.
  sampler->RecordFree(reinterpret_cast<void*>(0x1337));
  sampler->RecordFree(reinterpret_cast<void*>(0x7331));
}
#endif

}  // namespace heap_profiling
