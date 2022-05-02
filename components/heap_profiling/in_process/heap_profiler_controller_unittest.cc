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

}  // namespace

// HeapProfilerControllerTest can't be in an anonymous namespace because it is a
// friend of SamplingHeapProfiler.
class HeapProfilerControllerTest : public ::testing::Test {
 public:
  // Sets `sample_received_` to true if any sample is received. This will work
  // even without stack unwinding since it doesn't check the contents of the
  // sample. This must be public so that BindRepeating can access it from
  // subclasses.
  void RecordSampleReceived(base::TimeTicks, metrics::SampledProfile) {
    sample_received_ = true;
  }

 protected:
  // The default constructor parameters enable the HeapProfilerReporting feature
  // on all channels. Child classes can override the constructor to create test
  // suites that test different configurations.
  explicit HeapProfilerControllerTest(bool feature_enabled = true,
                                      double stable_probability = 1.0,
                                      double nonstable_probability = 1.0) {
    // ScopedFeatureList must be initialized in the constructor, before any
    // threads are started.
    if (feature_enabled) {
      feature_list_.InitAndEnableFeatureWithParameters(
          HeapProfilerController::kHeapProfilerReporting,
          {{"stable-probability", base::NumberToString(stable_probability)},
           {"nonstable-probability",
            base::NumberToString(nonstable_probability)},
           {"sampling-rate", base::NumberToString(kSamplingRate)}});
    } else {
      feature_list_.InitAndDisableFeature(
          HeapProfilerController::kHeapProfilerReporting);
      // Set the sampling rate manually since there's no param to read.
      base::SamplingHeapProfiler::Get()->SetSamplingInterval(kSamplingRate);
    }

    // Clear any samples set in the global SamplingHeapProfiler before the
    // ScopedMuteHookedSamplesForTesting was created.
    base::SamplingHeapProfiler::Get()->ClearSamplesForTesting();
  }

  ~HeapProfilerControllerTest() override {
    // Remove any callback that was set in StartHeapProfiling.
    metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
        base::DoNothing());
  }

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64)
  void SetUp() override {
    // TODO(crbug.com/1297724): The heap profiler is never started on these
    // platforms so there is nothing to test.
    GTEST_SKIP();
  }
#endif

  void StartHeapProfiling(
      version_info::Channel channel,
      base::RepeatingCallback<void(base::TimeTicks, metrics::SampledProfile)>
          receiver_callback) {
    ASSERT_FALSE(controller_) << "StartHeapProfiling called twice";
    metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
        std::move(receiver_callback));

    controller_ = std::make_unique<HeapProfilerController>(channel);
    controller_->SuppressRandomnessForTesting();
    controller_->Start();
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

  // Initialize `mute_hooks_` before `task_environment_` so that memory
  // allocations aren't sampled while TaskEnvironment creates a thread. The
  // sampling is crashing in the hooked FreeFunc on some test bots.
  base::PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting mute_hooks_;
  base::PoissonAllocationSampler::ScopedSuppressRandomnessForTesting
      suppress_randomness_;

  // Create `feature_list_` before `task_environment_` and destroy it after to
  // avoid a race in destruction.
  base::test::ScopedFeatureList feature_list_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<HeapProfilerController> controller_;
  base::HistogramTester histogram_tester_;

  // `sample_received_` is read from the main thread and written from a
  // background thread, but does not need to be atomic because the write happens
  // during a scheduled sample and the read happens well after that.
  bool sample_received_ = false;
};

namespace {

TEST_F(HeapProfilerControllerTest, EmptyProfileIsNotEmitted) {
  StartHeapProfiling(
      version_info::Channel::STABLE,
      base::BindRepeating(&HeapProfilerControllerTest::RecordSampleReceived,
                          base::Unretained(this)));

  // Advance several days to be sure the sample isn't scheduled right on the
  // boundary of the fast-forward.
  task_environment_.FastForwardBy(base::Days(2));

  EXPECT_FALSE(sample_received_);
}

// Sampling profiler is not capable of unwinding stack on Android under tests.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(HeapProfilerControllerTest, ProfileCollectionsScheduler) {
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

  StartHeapProfiling(version_info::Channel::STABLE,
                     base::BindLambdaForTesting(check_profile));

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

// Configurations of the HeapProfilerReporting feature to test.
struct FeatureTestParams {
  bool feature_enabled = true;
  double stable_probability = 1.0;
  double nonstable_probability = 1.0;
  bool expect_stable_sample = true;
  bool expect_nonstable_sample = true;
};
constexpr FeatureTestParams kAllFeatureConfigs[] = {
    // Disabled.
    {.feature_enabled = false,
     .expect_stable_sample = false,
     .expect_nonstable_sample = false},
    // Enabled, but with probability 0 on all channels.
    {.feature_enabled = true,
     .stable_probability = 0.0,
     .nonstable_probability = 0.0,
     .expect_stable_sample = false,
     .expect_nonstable_sample = false},
    // Enabled on all channels.
    {.feature_enabled = true,
     .stable_probability = 1.0,
     .nonstable_probability = 1.0,
     .expect_stable_sample = true,
     .expect_nonstable_sample = true},
    // Enabled on stable channel only.
    {.feature_enabled = true,
     .stable_probability = 1.0,
     .nonstable_probability = 0.0,
     .expect_stable_sample = true,
     .expect_nonstable_sample = false},
    // Enabled on non-stable channels only.
    {.feature_enabled = true,
     .stable_probability = 0.0,
     .nonstable_probability = 1.0,
     .expect_stable_sample = false,
     .expect_nonstable_sample = true},
};

class HeapProfilerControllerFeatureTest
    : public HeapProfilerControllerTest,
      public ::testing::WithParamInterface<FeatureTestParams> {
 public:
  HeapProfilerControllerFeatureTest()
      : HeapProfilerControllerTest(GetParam().feature_enabled,
                                   GetParam().stable_probability,
                                   GetParam().nonstable_probability) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         HeapProfilerControllerFeatureTest,
                         ::testing::ValuesIn(kAllFeatureConfigs));

TEST_P(HeapProfilerControllerFeatureTest, StableChannel) {
  StartHeapProfiling(
      version_info::Channel::STABLE,
      base::BindRepeating(&HeapProfilerControllerTest::RecordSampleReceived,
                          base::Unretained(this)));
  histogram_tester_.ExpectUniqueSample("HeapProfiling.InProcess.Enabled",
                                       GetParam().expect_stable_sample, 1);
  AddOneSampleAndWait();
  EXPECT_EQ(sample_received_, GetParam().expect_stable_sample);
}

// TODO(crbug.com/1302007): This test hangs on iPad device.
#if BUILDFLAG(IS_IOS)
#define MAYBE_CanaryChannel DISABLED_CanaryChannel
#else
#define MAYBE_CanaryChannel CanaryChannel
#endif
TEST_P(HeapProfilerControllerFeatureTest, MAYBE_CanaryChannel) {
  StartHeapProfiling(
      version_info::Channel::CANARY,
      base::BindRepeating(&HeapProfilerControllerTest::RecordSampleReceived,
                          base::Unretained(this)));
  histogram_tester_.ExpectUniqueSample("HeapProfiling.InProcess.Enabled",
                                       GetParam().expect_nonstable_sample, 1);
  AddOneSampleAndWait();
  EXPECT_EQ(sample_received_, GetParam().expect_nonstable_sample);
}

}  // namespace

}  // namespace heap_profiling
