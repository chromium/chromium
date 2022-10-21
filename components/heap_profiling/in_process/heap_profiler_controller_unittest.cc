// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_controller.h"

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/enum_set.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/heap_profiling/in_process/heap_profiler_parameters.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "components/version_info/channel.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/execution_context.pb.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace heap_profiling {

namespace {

using ProcessType = metrics::CallStackProfileParams::Process;
using ProcessTypeSet =
    base::EnumSet<ProcessType, ProcessType::kUnknown, ProcessType::kMax>;

constexpr size_t kSamplingRate = 1024;
constexpr size_t kAllocationSize = 42 * kSamplingRate;

using ProfileCollectorCallback =
    base::RepeatingCallback<void(base::TimeTicks, metrics::SampledProfile)>;

// A fake CallStackProfileCollector that deserializes profiles it receives from
// a fake child process, and passes them to the same callback that receives
// profiles from the fake browser process.
class TestCallStackProfileCollector final
    : public metrics::mojom::CallStackProfileCollector {
 public:
  explicit TestCallStackProfileCollector(
      ProfileCollectorCallback collector_callback)
      : collector_callback_(std::move(collector_callback)) {}

  TestCallStackProfileCollector(const TestCallStackProfileCollector&) = delete;
  TestCallStackProfileCollector& operator=(
      const TestCallStackProfileCollector&) = delete;

  ~TestCallStackProfileCollector() final = default;

  // metrics::mojom::CallStackProfileCollector
  void Collect(base::TimeTicks start_timestamp,
               metrics::mojom::ProfileType profile_type,
               metrics::mojom::SampledProfilePtr profile) final {
    metrics::SampledProfile sampled_profile;
    ASSERT_TRUE(profile);
    ASSERT_TRUE(sampled_profile.ParseFromString(profile->contents));
    EXPECT_EQ(profile_type == metrics::mojom::ProfileType::kHeap,
              sampled_profile.trigger_event() ==
                  metrics::SampledProfile::PERIODIC_HEAP_COLLECTION);
    collector_callback_.Run(start_timestamp, std::move(sampled_profile));
  }

 private:
  ProfileCollectorCallback collector_callback_;
};

// Converts the given HeapProfilerControllerTest constructor parameters to
// FieldTrialParams.
base::FieldTrialParams FieldTrialParamsForTestParams(
    const ProcessTypeSet& supported_processes = {},
    double stable_probability = 1.0,
    double nonstable_probability = 1.0) {
  base::FieldTrialParams field_trial_params;

  // Add the default params.
  base::Value::Dict dict;
  if (!supported_processes.Empty()) {
    // Explicitly disable profiling by default, so that only the processes
    // given in `supported_processes` will be enabled.
    dict.Set("is-supported", false);
  }
  dict.Set("stable-probability", stable_probability);
  dict.Set("nonstable-probability", nonstable_probability);
  dict.Set("sampling-rate-bytes", static_cast<int>(kSamplingRate));
  std::string param_string;
  base::JSONWriter::WriteWithOptions(
      dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &param_string);
  field_trial_params["default-params"] = param_string;

  // Add a field trial param that enables each process type in
  // `supported_processes`.
  base::Value::Dict is_supported_dict;
  is_supported_dict.Set("is-supported", true);
  std::string is_supported_string;
  base::JSONWriter::WriteWithOptions(is_supported_dict,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &is_supported_string);

  for (ProcessType process_type : supported_processes) {
    switch (process_type) {
      case ProcessType::kBrowser:
        field_trial_params["browser-process-params"] = is_supported_string;
        break;
      case ProcessType::kRenderer:
        field_trial_params["renderer-process-params"] = is_supported_string;
        break;
      case ProcessType::kGpu:
        field_trial_params["gpu-process-params"] = is_supported_string;
        break;
      case ProcessType::kUtility:
        field_trial_params["utility-process-params"] = is_supported_string;
        break;
      case ProcessType::kNetworkService:
        field_trial_params["network-process-params"] = is_supported_string;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  return field_trial_params;
}

}  // namespace

// HeapProfilerControllerTest can't be in an anonymous namespace because it is a
// friend of SamplingHeapProfiler.
class HeapProfilerControllerTest : public ::testing::Test {
 public:
  // Sets `sample_received_` to true if any sample is received. This will work
  // even without stack unwinding since it doesn't check the contents of the
  // sample. This must be public so that BindRepeating can access it from
  // subclasses.
  void RecordSampleReceived(base::TimeTicks,
                            metrics::SampledProfile sampled_profile) {
    EXPECT_EQ(sampled_profile.trigger_event(),
              metrics::SampledProfile::PERIODIC_HEAP_COLLECTION);
    EXPECT_EQ(sampled_profile.process(), expected_process_);
    // The mock clock should not have advanced since the sample was recorded, so
    // the collection time can be compared exactly.
    const base::TimeDelta expected_time_offset =
        task_environment_.NowTicks() - profiler_creation_time_;
    EXPECT_EQ(sampled_profile.call_stack_profile().profile_time_offset_ms(),
              expected_time_offset.InMilliseconds());
    sample_received_ = true;
  }

 protected:
  // The default constructor parameters enable the HeapProfilerReporting feature
  // on all channels. Child classes can override the constructor to create test
  // suites that test different configurations. Empty `supported_processes` uses
  // the default feature config, which should be browser-only.
  explicit HeapProfilerControllerTest(
      bool feature_enabled = true,
      const ProcessTypeSet& supported_processes = {},
      double stable_probability = 1.0,
      double nonstable_probability = 1.0) {
    // ScopedFeatureList must be initialized in the constructor, before any
    // threads are started.
    if (feature_enabled) {
      feature_list_.InitAndEnableFeatureWithParameters(
          kHeapProfilerReporting,
          FieldTrialParamsForTestParams(supported_processes, stable_probability,
                                        nonstable_probability));
    } else {
      feature_list_.InitAndDisableFeature(kHeapProfilerReporting);
      // Set the sampling rate manually since there's no param to read.
      base::SamplingHeapProfiler::Get()->SetSamplingInterval(kSamplingRate);
    }

    // Clear any samples set in the global SamplingHeapProfiler before the
    // ScopedMuteHookedSamplesForTesting was created.
    base::SamplingHeapProfiler::Get()->ClearSamplesForTesting();
  }

  ~HeapProfilerControllerTest() override {
    // Remove any collectors that were set in StartHeapProfiling.
    metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
        base::DoNothing());
    metrics::CallStackProfileBuilder::
        ResetChildCallStackProfileCollectorForTesting();
  }

  void StartHeapProfiling(version_info::Channel channel,
                          ProcessType process_type,
                          ProfileCollectorCallback collector_callback) {
    ASSERT_FALSE(controller_) << "StartHeapProfiling called twice";
    switch (process_type) {
      case ProcessType::kBrowser:
        expected_process_ = metrics::Process::BROWSER_PROCESS;
        metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
            std::move(collector_callback));
        break;
      case ProcessType::kUtility:
        expected_process_ = metrics::Process::UTILITY_PROCESS;
        ConnectRemoteProfileCollector(std::move(collector_callback));
        break;
      default:
        // Connect up the profile collector even though we expect the heap
        // profiler not to start, so that the test environment is complete.
        expected_process_ = metrics::Process::UNKNOWN_PROCESS;
        ConnectRemoteProfileCollector(std::move(collector_callback));
    }

    ASSERT_EQ(HeapProfilerController::GetProfilingEnabled(),
              HeapProfilerController::ProfilingEnabled::kNoController);
    profiler_creation_time_ = task_environment_.NowTicks();
    controller_ =
        std::make_unique<HeapProfilerController>(channel, process_type);
    controller_->SuppressRandomnessForTesting();
    controller_->StartIfEnabled();
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

  void ConnectRemoteProfileCollector(
      ProfileCollectorCallback collector_callback) {
    mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> remote;
    child_profile_collector_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<TestCallStackProfileCollector>(
            std::move(collector_callback)),
        remote.InitWithNewPipeAndPassReceiver());
    metrics::CallStackProfileBuilder::SetParentProfileCollectorForChildProcess(
        std::move(remote));
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
  mojo::SelfOwnedReceiverRef<metrics::mojom::CallStackProfileCollector>
      child_profile_collector_;

  // The creation time of the HeapProfilerController, saved so that
  // RecordSampleReceived() can test that SampledProfile::ms_after_login() in
  // that sample is a delta from the creation time.
  base::TimeTicks profiler_creation_time_;

  // Expected process type in a sample.
  metrics::Process expected_process_ = metrics::Process::UNKNOWN_PROCESS;

  // `sample_received_` is read from the main thread and written from a
  // background thread, but does not need to be atomic because the write happens
  // during a scheduled sample and the read happens well after that.
  bool sample_received_ = false;
};

namespace {

TEST_F(HeapProfilerControllerTest, EmptyProfileIsNotEmitted) {
  StartHeapProfiling(
      version_info::Channel::STABLE, ProcessType::kBrowser,
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

  StartHeapProfiling(version_info::Channel::STABLE, ProcessType::kBrowser,
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

TEST_F(HeapProfilerControllerTest, UnhandledProcess) {
  // Starting the heap profiler in an unhandled process type should safely do
  // nothing.
  StartHeapProfiling(version_info::Channel::STABLE, ProcessType::kUnknown,
                     base::DoNothing());
  EXPECT_EQ(HeapProfilerController::GetProfilingEnabled(),
            HeapProfilerController::ProfilingEnabled::kDisabled);
  // The Enabled summary histogram should not be logged for unsupported
  // processes, because they're not included in the per-process histograms that
  // are aggregated into it.
  histogram_tester_.ExpectTotalCount("HeapProfiling.InProcess.Enabled", 0);
}

// Configurations of the HeapProfilerReporting feature to test.
// The default parameters collect samples from stable and nonstable channels in
// the browser process only.
struct FeatureTestParams {
  struct ChannelParams {
    double probability = 1.0;
    bool expect_browser_sample = true;
    bool expect_child_sample = false;
  };
  bool feature_enabled = true;
  const ProcessTypeSet supported_processes;
  ChannelParams stable;
  ChannelParams nonstable;
};

std::ostream& operator<<(std::ostream& os, const FeatureTestParams& params) {
  os << "{";
  os << "enabled:" << params.feature_enabled << ",";
  os << "field_trial_params:{";
  for (const auto& field_trial_param : FieldTrialParamsForTestParams(
           params.supported_processes, params.stable.probability,
           params.nonstable.probability)) {
    os << field_trial_param.first << " : " << field_trial_param.second;
  }
  os << "},";
  os << "expect_samples:{";
  os << "stable/browser:" << params.stable.expect_browser_sample << ",";
  os << "stable/child:" << params.stable.expect_child_sample << ",";
  os << "nonstable/browser:" << params.stable.expect_browser_sample << ",";
  os << "nonstable/child:" << params.stable.expect_child_sample;
  os << "}";
  return os;
}

class HeapProfilerControllerFeatureTest
    : public HeapProfilerControllerTest,
      public ::testing::WithParamInterface<FeatureTestParams> {
 public:
  HeapProfilerControllerFeatureTest()
      : HeapProfilerControllerTest(GetParam().feature_enabled,
                                   GetParam().supported_processes,
                                   GetParam().stable.probability,
                                   GetParam().nonstable.probability) {}
};

// Test the feature on various channels.
constexpr FeatureTestParams kChannelConfigs[] = {
    // Disabled.
    {
        .feature_enabled = false,
        .stable = {.expect_browser_sample = false},
        .nonstable = {.expect_browser_sample = false},
    },
    // Enabled, but with probability 0 on all channels.
    {
        .feature_enabled = true,
        .stable = {.probability = 0.0, .expect_browser_sample = false},
        .nonstable = {.probability = 0.0, .expect_browser_sample = false},
    },
    // Enabled on all channels.
    {
        .feature_enabled = true,
        .stable = {.probability = 1.0, .expect_browser_sample = true},
        .nonstable = {.probability = 1.0, .expect_browser_sample = true},
    },
    // Enabled on stable channel only.
    {
        .feature_enabled = true,
        .stable = {.probability = 1.0, .expect_browser_sample = true},
        .nonstable = {.probability = 0.0, .expect_browser_sample = false},
    },
    // Enabled on non-stable channels only.
    {
        .feature_enabled = true,
        .stable = {.probability = 0.0, .expect_browser_sample = false},
        .nonstable = {.probability = 1.0, .expect_browser_sample = true},
    },
};

using HeapProfilerControllerChannelTest = HeapProfilerControllerFeatureTest;

INSTANTIATE_TEST_SUITE_P(All,
                         HeapProfilerControllerChannelTest,
                         ::testing::ValuesIn(kChannelConfigs));

TEST_P(HeapProfilerControllerChannelTest, StableChannel) {
  StartHeapProfiling(
      version_info::Channel::STABLE, ProcessType::kBrowser,
      base::BindRepeating(&HeapProfilerControllerTest::RecordSampleReceived,
                          base::Unretained(this)));
  EXPECT_EQ(HeapProfilerController::GetProfilingEnabled(),
            GetParam().stable.expect_browser_sample
                ? HeapProfilerController::ProfilingEnabled::kEnabled
                : HeapProfilerController::ProfilingEnabled::kDisabled);
  histogram_tester_.ExpectUniqueSample(
      "HeapProfiling.InProcess.Enabled.Browser",
      GetParam().stable.expect_browser_sample, 1);
  histogram_tester_.ExpectUniqueSample("HeapProfiling.InProcess.Enabled",
                                       GetParam().stable.expect_browser_sample,
                                       1);
  AddOneSampleAndWait();
  EXPECT_EQ(sample_received_, GetParam().stable.expect_browser_sample);
}

// TODO(crbug.com/1302007): This test hangs on iPad device.
#if BUILDFLAG(IS_IOS)
#define MAYBE_CanaryChannel DISABLED_CanaryChannel
#else
#define MAYBE_CanaryChannel CanaryChannel
#endif
TEST_P(HeapProfilerControllerChannelTest, MAYBE_CanaryChannel) {
  StartHeapProfiling(
      version_info::Channel::CANARY, ProcessType::kBrowser,
      base::BindRepeating(&HeapProfilerControllerTest::RecordSampleReceived,
                          base::Unretained(this)));
  EXPECT_EQ(HeapProfilerController::GetProfilingEnabled(),
            GetParam().nonstable.expect_browser_sample
                ? HeapProfilerController::ProfilingEnabled::kEnabled
                : HeapProfilerController::ProfilingEnabled::kDisabled);
  histogram_tester_.ExpectUniqueSample(
      "HeapProfiling.InProcess.Enabled.Browser",
      GetParam().nonstable.expect_browser_sample, 1);
  histogram_tester_.ExpectUniqueSample(
      "HeapProfiling.InProcess.Enabled",
      GetParam().nonstable.expect_browser_sample, 1);
  AddOneSampleAndWait();
  EXPECT_EQ(sample_received_, GetParam().nonstable.expect_browser_sample);
}

// Test the feature in various processes.
constexpr FeatureTestParams kProcessConfigs[] = {
    // Enabled in parent process only.
    {
        .supported_processes = {ProcessType::kBrowser},
        .stable = {.expect_browser_sample = true, .expect_child_sample = false},
        .nonstable = {.expect_browser_sample = true,
                      .expect_child_sample = false},
    },
    // Enabled in child process only.
    {
        .supported_processes = {ProcessType::kUtility},
        .stable = {.expect_browser_sample = false, .expect_child_sample = true},
        .nonstable = {.expect_browser_sample = false,
                      .expect_child_sample = true},
    },
    // Enabled in parent and child processes.
    {
        .supported_processes = {ProcessType::kBrowser, ProcessType::kUtility},
        .stable = {.expect_browser_sample = true, .expect_child_sample = true},
        .nonstable = {.expect_browser_sample = true,
                      .expect_child_sample = true},
    },
};

using HeapProfilerControllerProcessTest = HeapProfilerControllerFeatureTest;

INSTANTIATE_TEST_SUITE_P(All,
                         HeapProfilerControllerProcessTest,
                         ::testing::ValuesIn(kProcessConfigs));

TEST_P(HeapProfilerControllerProcessTest, BrowserProcess) {
  StartHeapProfiling(
      version_info::Channel::STABLE, ProcessType::kBrowser,
      base::BindRepeating(&HeapProfilerControllerTest::RecordSampleReceived,
                          base::Unretained(this)));
  EXPECT_EQ(HeapProfilerController::GetProfilingEnabled(),
            GetParam().stable.expect_browser_sample
                ? HeapProfilerController::ProfilingEnabled::kEnabled
                : HeapProfilerController::ProfilingEnabled::kDisabled);
  histogram_tester_.ExpectUniqueSample(
      "HeapProfiling.InProcess.Enabled.Browser",
      GetParam().stable.expect_browser_sample, 1);
  histogram_tester_.ExpectUniqueSample("HeapProfiling.InProcess.Enabled",
                                       GetParam().stable.expect_browser_sample,
                                       1);
  AddOneSampleAndWait();
  EXPECT_EQ(sample_received_, GetParam().stable.expect_browser_sample);
}

TEST_P(HeapProfilerControllerProcessTest, ChildProcess) {
  StartHeapProfiling(
      version_info::Channel::STABLE, ProcessType::kUtility,
      base::BindRepeating(&HeapProfilerControllerTest::RecordSampleReceived,
                          base::Unretained(this)));
  EXPECT_EQ(HeapProfilerController::GetProfilingEnabled(),
            GetParam().stable.expect_child_sample
                ? HeapProfilerController::ProfilingEnabled::kEnabled
                : HeapProfilerController::ProfilingEnabled::kDisabled);
  histogram_tester_.ExpectUniqueSample(
      "HeapProfiling.InProcess.Enabled.Utility",
      GetParam().stable.expect_child_sample, 1);
  histogram_tester_.ExpectUniqueSample("HeapProfiling.InProcess.Enabled",
                                       GetParam().stable.expect_child_sample,
                                       1);
  AddOneSampleAndWait();
  EXPECT_EQ(sample_received_, GetParam().stable.expect_child_sample);
}

}  // namespace

}  // namespace heap_profiling
