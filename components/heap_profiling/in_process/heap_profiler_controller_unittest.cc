// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_controller.h"

#include <atomic>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace heap_profiling {

namespace {

constexpr size_t kSamplingRate = 1024;
constexpr size_t kAllocationSize = 42 * kSamplingRate;

// Configurations of the HeapProfilerReporting feature to test.
// The default parameters always enable reporting on all channels.
struct HeapProfilerReportingConfig {
  bool enabled = true;
  double stable_probability = 1.0;
  double nonstable_probability = 1.0;
};

// A wrapper that sets up a HeapProfilerController for testing.
class HeapProfilerControllerTester {
 public:
  HeapProfilerControllerTester(
      version_info::Channel channel,
      base::RepeatingCallback<void(base::TimeTicks, metrics::SampledProfile)>
          receiver_callback,
      const HeapProfilerReportingConfig& feature_config =
          HeapProfilerReportingConfig()) {
    if (feature_config.enabled) {
      feature_list_.InitAndEnableFeatureWithParameters(
          HeapProfilerController::kHeapProfilerReporting,
          {{"stable-probability",
            base::NumberToString(feature_config.stable_probability)},
           {"nonstable-probability",
            base::NumberToString(feature_config.nonstable_probability)},
           {"sampling-rate", base::NumberToString(kSamplingRate)}});
    } else {
      feature_list_.InitAndDisableFeature(
          HeapProfilerController::kHeapProfilerReporting);
      // Set the sampling rate manually since there's no param to read.
      base::SamplingHeapProfiler::Get()->SetSamplingInterval(kSamplingRate);
    }

    metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
        std::move(receiver_callback));

    controller_ = std::make_unique<HeapProfilerController>(channel);
    controller_->SuppressRandomnessForTesting();
  }

  ~HeapProfilerControllerTester() {
    metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
        base::DoNothing());
  }

  HeapProfilerControllerTester(const HeapProfilerControllerTester&) = delete;
  HeapProfilerControllerTester& operator=(const HeapProfilerControllerTester&) =
      delete;

  HeapProfilerController& controller() { return *controller_; }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  std::unique_ptr<HeapProfilerController> controller_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

// A callback that fails the test if any samples are received.
void ExpectNoSamples(base::TimeTicks, metrics::SampledProfile) {
  ADD_FAILURE();
}

}  // namespace

class HeapProfilerControllerTest : public ::testing::Test {
 protected:
  HeapProfilerControllerTest() {
    // Clear any samples set in the global SamplingHeapProfiler before the
    // ScopedMuteHookedSamplesForTesting was created.
    base::SamplingHeapProfiler::Get()->ClearSamplesForTesting();
    base::PoissonAllocationSampler::Get()->SuppressRandomnessForTest(true);
  }

  ~HeapProfilerControllerTest() override {
    base::PoissonAllocationSampler::Get()->SuppressRandomnessForTest(false);
  }

  void AddOneSampleAndWait() {
    auto* sampler = base::PoissonAllocationSampler::Get();
    sampler->RecordAlloc(reinterpret_cast<void*>(0x1337), kAllocationSize,
                         base::PoissonAllocationSampler::kManualForTesting,
                         nullptr);
    // Advance several days to be sure the sample isn't scheduled right on the
    // boundary of the fast-forward.
    task_environment_.FastForwardBy(base::Days(2));
    // Free the allocation so that other tests can re-use the address.
    sampler->RecordFree(reinterpret_cast<void*>(0x1337));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting mute_hooks_;
};

TEST_F(HeapProfilerControllerTest, EmptyProfileIsNotEmitted) {
  HeapProfilerControllerTester tester(version_info::Channel::STABLE,
                                      base::BindRepeating(&ExpectNoSamples));
  tester.controller().Start();
  // Advance several days to be sure the sample isn't scheduled right on the
  // boundary of the fast-forward.
  task_environment_.FastForwardBy(base::Days(2));
}

// Sampling profiler is not capable of unwinding stack on Android under tests.
#if !defined(OS_ANDROID)

// See crbug.com/1276033
#if defined(OS_APPLE)
#define MAYBE_ProfileCollectionsScheduler DISABLED_ProfileCollectionsScheduler
#else
#define MAYBE_ProfileCollectionsScheduler ProfileCollectionsScheduler
#endif

TEST_F(HeapProfilerControllerTest, MAYBE_ProfileCollectionsScheduler) {
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

  HeapProfilerControllerTester tester(
      version_info::Channel::STABLE, base::BindLambdaForTesting(check_profile));
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

// Test that disabling the HeapProfilerReporting feature disables metrics
// uploading.
TEST_F(HeapProfilerControllerTest, DisableFeature) {
  HeapProfilerControllerTester tester(
      version_info::Channel::STABLE, base::BindRepeating(&ExpectNoSamples),
      HeapProfilerReportingConfig{.enabled = false});
  tester.controller().Start();
  tester.histogram_tester().ExpectUniqueSample(
      "HeapProfiling.InProcess.Enabled", false, 1);
  AddOneSampleAndWait();
}

// Test that the "stable-probability" param can disable metrics uploading on the
// stable channel.
TEST_F(HeapProfilerControllerTest, StableProbability) {
  HeapProfilerReportingConfig feature_config{
      .enabled = true, .stable_probability = 0.0, .nonstable_probability = 1.0};

  // Test stable channel.
  {
    HeapProfilerControllerTester tester(version_info::Channel::STABLE,
                                        base::BindRepeating(&ExpectNoSamples),
                                        feature_config);
    tester.controller().Start();
    tester.histogram_tester().ExpectUniqueSample(
        "HeapProfiling.InProcess.Enabled", false, 1);
    AddOneSampleAndWait();
  }

  // Test canary channel.
  {
    std::atomic<bool> got_sample;
    auto watch_for_sample = [&](base::TimeTicks, metrics::SampledProfile) {
      got_sample = true;
    };
    HeapProfilerControllerTester tester(
        version_info::Channel::CANARY,
        base::BindLambdaForTesting(watch_for_sample), feature_config);
    tester.controller().Start();
    tester.histogram_tester().ExpectUniqueSample(
        "HeapProfiling.InProcess.Enabled", true, 1);
    AddOneSampleAndWait();
    EXPECT_TRUE(got_sample);
  }
}

// Test that the "nonstable-probability" param can disable metrics uploading on
// the canary channel.
TEST_F(HeapProfilerControllerTest, NonStableProbability) {
  HeapProfilerReportingConfig feature_config{
      .enabled = true, .stable_probability = 1.0, .nonstable_probability = 0.0};

  // Test stable channel.
  {
    std::atomic<bool> got_sample;
    auto watch_for_sample = [&](base::TimeTicks, metrics::SampledProfile) {
      got_sample = true;
    };
    HeapProfilerControllerTester tester(
        version_info::Channel::STABLE,
        base::BindLambdaForTesting(watch_for_sample), feature_config);
    tester.controller().Start();
    tester.histogram_tester().ExpectUniqueSample(
        "HeapProfiling.InProcess.Enabled", true, 1);
    AddOneSampleAndWait();
    EXPECT_TRUE(got_sample);
  }

  // Test canary channel.
  {
    HeapProfilerControllerTester tester(version_info::Channel::CANARY,
                                        base::BindRepeating(&ExpectNoSamples),
                                        feature_config);
    tester.controller().Start();
    tester.histogram_tester().ExpectUniqueSample(
        "HeapProfiling.InProcess.Enabled", false, 1);
    AddOneSampleAndWait();
  }
}

}  // namespace heap_profiling
