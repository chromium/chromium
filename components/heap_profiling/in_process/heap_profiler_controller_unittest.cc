// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_controller.h"

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/heap_profiling/in_process/browser_process_snapshot_controller.h"
#include "components/heap_profiling/in_process/child_process_snapshot_controller.h"
#include "components/heap_profiling/in_process/heap_profiler_parameters.h"
#include "components/heap_profiling/in_process/mojom/snapshot_controller.mojom.h"
#include "components/heap_profiling/in_process/switches.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/metrics/call_stacks/call_stack_profile_params.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "components/version_info/channel.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/execution_context.pb.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace heap_profiling {

namespace {

using FeatureRef = base::test::FeatureRef;
using FeatureRefAndParams = base::test::FeatureRefAndParams;
using ProcessType = metrics::CallStackProfileParams::Process;
using ProcessTypeSet =
    base::EnumSet<ProcessType, ProcessType::kUnknown, ProcessType::kMax>;
using base::allocator::dispatcher::AllocationNotificationData;
using base::allocator::dispatcher::AllocationSubsystem;
using base::allocator::dispatcher::FreeNotificationData;

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

// A scoped holder for callbacks to pass to
// HeapProfilerControllerTest::StartHeapProfiling(), and a BarrierClosure that
// will quit a run loop. The ScopedCallbacks object must stay in scope until the
// run loop finishes.
class ScopedCallbacks {
 public:
  // Creates a BarrierClosure that will invoke the given `quit_closure` after
  // the expected callbacks are invoked:
  //
  // If `expect_take_snapshot` is true, HeapProfilerController::TakeSnapshot()
  // should be called, so first_snapshot_callback() returns a callback that's
  // expected to be invoked. If not, first_snapshot_callback() returns a
  // callback that's expected not to run.
  //
  // If `expect_sampled_profile` is true, HeapProfilerController::TakeSnapshot()
  // should find a sample to pass to CallStackProfileBuilder, so
  // collector_callback() returns a callback that's expected to be invoked. It
  // will also run the given `profile_collector_callback`. If not,
  // collector_callback() returns a callback that's expected not to run, and the
  // given `profile_collector_callback` is ignored.
  //
  // If `use_other_process_callback` is true, the test will also fake a snapshot
  // in another process, so other_process_callback() will return a callback to
  // invoke for this.
  ScopedCallbacks(bool expect_take_snapshot,
                  bool expect_sampled_profile,
                  bool use_other_process_callback,
                  ProfileCollectorCallback profile_collector_callback,
                  base::OnceClosure quit_closure) {
    size_t num_callbacks = 0;
    if (expect_take_snapshot) {
      num_callbacks += 1;
    }
    if (expect_sampled_profile) {
      num_callbacks += 1;
    }
    if (use_other_process_callback) {
      // The test should invoke other_process_snapshot_callback() to simulate a
      // snapshot in another process.
      num_callbacks += 1;
    }
    barrier_closure_ =
        base::BarrierClosure(num_callbacks, std::move(quit_closure));

    if (expect_take_snapshot) {
      first_snapshot_callback_ = barrier_closure_;
    } else {
      first_snapshot_callback_ =
          base::BindOnce([] { FAIL() << "TakeSnapshot called unexpectedly."; });
    }
    if (expect_sampled_profile) {
      collector_callback_ =
          std::move(profile_collector_callback).Then(barrier_closure_);
    } else {
      collector_callback_ =
          base::BindRepeating([](base::TimeTicks, metrics::SampledProfile) {
            FAIL() << "ProfileCollectorCallback called unexpectedly.";
          });
    }
    if (use_other_process_callback) {
      other_process_callback_ = barrier_closure_;
    } else {
      other_process_callback_ = base::BindOnce(
          [] { FAIL() << "Other process callback invoked unexpectedly."; });
    }
  }

  ~ScopedCallbacks() = default;

  // Move-only.
  ScopedCallbacks(const ScopedCallbacks&) = delete;
  ScopedCallbacks& operator=(const ScopedCallbacks&) = delete;
  ScopedCallbacks(ScopedCallbacks&&) = default;
  ScopedCallbacks& operator=(ScopedCallbacks&&) = default;

  base::OnceClosure first_snapshot_callback() {
    return std::move(first_snapshot_callback_);
  }

  ProfileCollectorCallback collector_callback() {
    return std::move(collector_callback_);
  }

  base::OnceClosure other_process_callback() {
    return std::move(other_process_callback_);
  }

 private:
  base::RepeatingClosure barrier_closure_;
  base::OnceClosure first_snapshot_callback_;
  ProfileCollectorCallback collector_callback_;
  base::OnceClosure other_process_callback_;
};

class MockSnapshotController : public mojom::SnapshotController {
 public:
  MOCK_METHOD(void, TakeSnapshot, (), (override));
};

// Configurations of the HeapProfiler* features to test.
// The default parameters collect samples from stable and nonstable channels in
// the browser process only.
struct FeatureTestParams {
  struct ChannelParams {
    double probability = 1.0;
    bool expect_browser_sample = true;
    bool expect_child_sample = false;
  };
  // Whether HeapProfilerReporting is enabled.
  bool feature_enabled = true;
  const ProcessTypeSet supported_processes;
  ChannelParams stable;
  ChannelParams nonstable;
  // Whether HeapProfilerIncludeZero is enabled.
  bool include_zero_feature_enabled = true;
  // Whether HeapProfilerCentralControl is enabled.
  bool central_control_feature_enabled = false;

  base::FieldTrialParams ToFieldTrialParams() const;

  std::vector<FeatureRefAndParams> GetEnabledFeatures() const;
  std::vector<FeatureRef> GetDisabledFeatures() const;
};

// Converts the test params to field trial parameters for the
// HeapProfilerReporting feature.
base::FieldTrialParams FeatureTestParams::ToFieldTrialParams() const {
  base::FieldTrialParams field_trial_params;

  // Add the default params.
  base::Value::Dict dict;
  if (!supported_processes.empty()) {
    // Explicitly disable profiling by default, so that only the processes
    // given in `supported_processes` will be enabled.
    dict.Set("is-supported", false);
  }
  dict.Set("stable-probability", stable.probability);
  dict.Set("nonstable-probability", nonstable.probability);
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

std::vector<FeatureRefAndParams> FeatureTestParams::GetEnabledFeatures() const {
  std::vector<FeatureRefAndParams> enabled_features;
  if (feature_enabled) {
    enabled_features.push_back(
        FeatureRefAndParams(kHeapProfilerReporting, ToFieldTrialParams()));
  }
  if (include_zero_feature_enabled) {
    enabled_features.push_back(
        FeatureRefAndParams(kHeapProfilerIncludeZero, {}));
  }
  if (central_control_feature_enabled) {
    enabled_features.push_back(
        FeatureRefAndParams(kHeapProfilerCentralControl, {}));
  }
  return enabled_features;
}

std::vector<FeatureRef> FeatureTestParams::GetDisabledFeatures() const {
  std::vector<FeatureRef> disabled_features;
  if (!feature_enabled) {
    disabled_features.push_back(FeatureRef(kHeapProfilerReporting));
  }
  if (!include_zero_feature_enabled) {
    disabled_features.push_back(FeatureRef(kHeapProfilerIncludeZero));
  }
  if (!central_control_feature_enabled) {
    disabled_features.push_back(FeatureRef(kHeapProfilerCentralControl));
  }
  return disabled_features;
}

// Formats the test params for error messages.
std::ostream& operator<<(std::ostream& os, const FeatureTestParams& params) {
  os << "{";
  os << "enabled:" << params.feature_enabled << ",";
  os << "field_trial_params:{";
  for (const auto& field_trial_param : params.ToFieldTrialParams()) {
    os << field_trial_param.first << " : " << field_trial_param.second;
  }
  os << "},";
  os << "expect_samples:{";
  os << "stable/browser:" << params.stable.expect_browser_sample << ",";
  os << "stable/child:" << params.stable.expect_child_sample << ",";
  os << "nonstable/browser:" << params.stable.expect_browser_sample << ",";
  os << "nonstable/child:" << params.stable.expect_child_sample;
  os << "},";
  os << "include_zero:" << params.include_zero_feature_enabled << ",";
  os << "central_control:" << params.central_control_feature_enabled;
  os << "}";
  return os;
}

}  // namespace

// HeapProfilerControllerTest can't be in an anonymous namespace because it is a
// friend of SamplingHeapProfiler.
class HeapProfilerControllerTest
    : public ::testing::TestWithParam<FeatureTestParams> {
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
  HeapProfilerControllerTest() {
    // ScopedFeatureList must be initialized in the constructor, before any
    // threads are started.
    feature_list_.InitWithFeaturesAndParameters(
        GetParam().GetEnabledFeatures(), GetParam().GetDisabledFeatures());
    if (!GetParam().feature_enabled) {
      // Set the sampling rate manually since there's no feature param to read.
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

  // Creates a HeapProfilerController and mocks profiling a process of type
  // `process_type` on `channel`. The test should pass `expect_enabled` as true
  // if heap profiling should be enabled in this test setup.
  //
  // `first_snapshot_callback` will be invoked the first time
  // HeapProfilerController::TakeSnapshot() is called, even if it doesn't
  // collect a profile. `collector_callback` will be invoked whenever
  // TakeSnapshot() passes a profile to CallStackProfileBuilder.
  void StartHeapProfiling(
      version_info::Channel channel,
      ProcessType process_type,
      bool expect_enabled,
      base::OnceClosure first_snapshot_callback = base::DoNothing(),
      ProfileCollectorCallback collector_callback = base::DoNothing()) {
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
        break;
    }

    ASSERT_FALSE(HeapProfilerController::GetInstance());
    profiler_creation_time_ = task_environment_.NowTicks();
    controller_ =
        std::make_unique<HeapProfilerController>(channel, process_type);
    controller_->SuppressRandomnessForTesting();
    controller_->SetFirstSnapshotCallbackForTesting(
        std::move(first_snapshot_callback));

    EXPECT_EQ(HeapProfilerController::GetInstance(), controller_.get());
    EXPECT_EQ(controller_->IsEnabled(), expect_enabled);
    EXPECT_EQ(controller_->StartIfEnabled(), expect_enabled);
  }

  void AddOneSampleAndWait() {
    auto* sampler = base::PoissonAllocationSampler::Get();
    sampler->OnAllocation(AllocationNotificationData(
        reinterpret_cast<void*>(0x1337), kAllocationSize, nullptr,
        AllocationSubsystem::kManualForTesting));
    task_environment_.RunUntilQuit();
    // Free the allocation so that other tests can re-use the address.
    sampler->OnFree(
        FreeNotificationData(reinterpret_cast<void*>(0x1337),
                             AllocationSubsystem::kManualForTesting));
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

  ScopedCallbacks CreateScopedCallbacks(
      bool expect_take_snapshot,
      bool expect_sampled_profile,
      bool use_other_process_callback = false) {
    return ScopedCallbacks(
        expect_take_snapshot, expect_sampled_profile,
        use_other_process_callback,
        base::BindRepeating(&HeapProfilerControllerTest::RecordSampleReceived,
                            base::Unretained(this)),
        task_environment_.QuitClosure());
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

// Basic tests only use the default feature params.
INSTANTIATE_TEST_SUITE_P(All,
                         HeapProfilerControllerTest,
                         ::testing::Values(FeatureTestParams{}));

// Sampling profiler is not capable of unwinding stack on Android under tests.
#if !BUILDFLAG(IS_ANDROID)
TEST_P(HeapProfilerControllerTest, ProfileCollectionsScheduler) {
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
                     /*expect_enabled=*/true,
                     /*first_snapshot_callback=*/base::DoNothing(),
                     base::BindLambdaForTesting(check_profile));

  auto* sampler = base::PoissonAllocationSampler::Get();
  sampler->OnAllocation(AllocationNotificationData(
      reinterpret_cast<void*>(0x1337), kAllocationSize, nullptr,
      AllocationSubsystem::kManualForTesting));
  sampler->OnAllocation(AllocationNotificationData(
      reinterpret_cast<void*>(0x7331), kAllocationSize, nullptr,
      AllocationSubsystem::kManualForTesting));

  // The profiler should continue to collect snapshots as long as this memory is
  // allocated. If not the test will time out.
  while (profile_count < kSnapshotsToCollect) {
    task_environment_.FastForwardBy(base::Days(1));
  }

  // Free all recorded memory so the address list is empty for the next test.
  sampler->OnFree(FreeNotificationData(reinterpret_cast<void*>(0x1337),
                                       AllocationSubsystem::kManualForTesting));
  sampler->OnFree(FreeNotificationData(reinterpret_cast<void*>(0x7331),
                                       AllocationSubsystem::kManualForTesting));
}
#endif

TEST_P(HeapProfilerControllerTest, UnhandledProcess) {
  // Starting the heap profiler in an unhandled process type should safely do
  // nothing.
  StartHeapProfiling(version_info::Channel::STABLE, ProcessType::kUnknown,
                     /*expect_enabled=*/false);
  // The Enabled summary histogram should not be logged for unsupported
  // processes, because they're not included in the per-process histograms that
  // are aggregated into it.
  histogram_tester_.ExpectTotalCount("HeapProfiling.InProcess.Enabled", 0);
}

// Test the feature on various channels in the browser process.
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

using HeapProfilerControllerChannelTest = HeapProfilerControllerTest;

INSTANTIATE_TEST_SUITE_P(All,
                         HeapProfilerControllerChannelTest,
                         ::testing::ValuesIn(kChannelConfigs));

TEST_P(HeapProfilerControllerChannelTest, StableChannel) {
  const bool profiling_enabled =
      GetParam().feature_enabled && GetParam().stable.probability > 0.0;
  ScopedCallbacks callbacks = CreateScopedCallbacks(
      /*expect_take_snapshot=*/profiling_enabled,
      GetParam().stable.expect_browser_sample);
  StartHeapProfiling(version_info::Channel::STABLE, ProcessType::kBrowser,
                     profiling_enabled, callbacks.first_snapshot_callback(),
                     callbacks.collector_callback());
  histogram_tester_.ExpectUniqueSample(
      "HeapProfiling.InProcess.Enabled.Browser", profiling_enabled, 1);
  histogram_tester_.ExpectUniqueSample("HeapProfiling.InProcess.Enabled",
                                       profiling_enabled, 1);
  AddOneSampleAndWait();
  EXPECT_EQ(sample_received_, GetParam().stable.expect_browser_sample);
}

TEST_P(HeapProfilerControllerChannelTest, CanaryChannel) {
  const bool profiling_enabled =
      GetParam().feature_enabled && GetParam().nonstable.probability > 0.0;
  ScopedCallbacks callbacks = CreateScopedCallbacks(
      /*expect_take_snapshot=*/profiling_enabled,
      GetParam().nonstable.expect_browser_sample);
  StartHeapProfiling(version_info::Channel::CANARY, ProcessType::kBrowser,
                     profiling_enabled, callbacks.first_snapshot_callback(),
                     callbacks.collector_callback());
  histogram_tester_.ExpectUniqueSample(
      "HeapProfiling.InProcess.Enabled.Browser", profiling_enabled, 1);
  histogram_tester_.ExpectUniqueSample("HeapProfiling.InProcess.Enabled",
                                       profiling_enabled, 1);
  AddOneSampleAndWait();
  EXPECT_EQ(sample_received_, GetParam().nonstable.expect_browser_sample);
}

TEST_P(HeapProfilerControllerChannelTest, UnknownChannel) {
  // An unknown channel should be treated like stable, in case a large
  // population doesn't have the channel set.
  const bool profiling_enabled =
      GetParam().feature_enabled && GetParam().stable.probability > 0.0;
  ScopedCallbacks callbacks = CreateScopedCallbacks(
      /*expect_take_snapshot=*/profiling_enabled,
      GetParam().stable.expect_browser_sample);
  StartHeapProfiling(version_info::Channel::UNKNOWN, ProcessType::kBrowser,
                     profiling_enabled, callbacks.first_snapshot_callback(),
                     callbacks.collector_callback());
  histogram_tester_.ExpectUniqueSample(
      "HeapProfiling.InProcess.Enabled.Browser", profiling_enabled, 1);
  histogram_tester_.ExpectUniqueSample("HeapProfiling.InProcess.Enabled",
                                       profiling_enabled, 1);
  AddOneSampleAndWait();
  EXPECT_EQ(sample_received_, GetParam().stable.expect_browser_sample);
}

// Test the feature in various processes on the stable channel.
constexpr FeatureTestParams kProcessConfigs[] = {
    // Enabled in parent process only.
    {
        .supported_processes = {ProcessType::kBrowser},
        .stable = {.expect_browser_sample = true, .expect_child_sample = false},
        .central_control_feature_enabled = false,
    },
    {
        .supported_processes = {ProcessType::kBrowser},
        .stable = {.expect_browser_sample = true, .expect_child_sample = false},
        .central_control_feature_enabled = true,
    },
    // Enabled in child process only.
    {
        .supported_processes = {ProcessType::kUtility},
        .stable = {.expect_browser_sample = false, .expect_child_sample = true},
        .central_control_feature_enabled = false,
    },
    {
        // Central control only samples child processes when the browser process
        // is sampled, so no samples are expected even though sampling is
        // supported in the child process.
        .supported_processes = {ProcessType::kUtility},
        .stable = {.expect_browser_sample = false,
                   .expect_child_sample = false},
        .central_control_feature_enabled = true,
    },
    // Enabled in parent and child processes.
    {
        .supported_processes = {ProcessType::kBrowser, ProcessType::kUtility},
        .stable = {.expect_browser_sample = true, .expect_child_sample = true},
        .central_control_feature_enabled = false,
    },
    {
        .supported_processes = {ProcessType::kBrowser, ProcessType::kUtility},
        .stable = {.expect_browser_sample = true, .expect_child_sample = true},
        .central_control_feature_enabled = true,
    },
};

using HeapProfilerControllerProcessTest = HeapProfilerControllerTest;

INSTANTIATE_TEST_SUITE_P(All,
                         HeapProfilerControllerProcessTest,
                         ::testing::ValuesIn(kProcessConfigs));

TEST_P(HeapProfilerControllerProcessTest, BrowserProcess) {
  const bool profiling_enabled =
      base::Contains(GetParam().supported_processes, ProcessType::kBrowser);
  ScopedCallbacks callbacks = CreateScopedCallbacks(
      /*expect_take_snapshot=*/profiling_enabled,
      GetParam().stable.expect_browser_sample,
      /*use_other_process_callback=*/
      GetParam().central_control_feature_enabled &&
          GetParam().stable.expect_child_sample);

  // Mock the child end of the SnapshotController mojo pipe. (Only used when
  // central control is enabled.)
  MockSnapshotController mock_child_snapshot_controller;
  mojo::Receiver<mojom::SnapshotController> mock_receiver(
      &mock_child_snapshot_controller);

  StartHeapProfiling(version_info::Channel::STABLE, ProcessType::kBrowser,
                     profiling_enabled, callbacks.first_snapshot_callback(),
                     callbacks.collector_callback());
  histogram_tester_.ExpectUniqueSample(
      "HeapProfiling.InProcess.Enabled.Browser", profiling_enabled, 1);
  histogram_tester_.ExpectUniqueSample("HeapProfiling.InProcess.Enabled",
                                       profiling_enabled, 1);

  constexpr int kTestChildProcessId = 1;
  if (profiling_enabled && GetParam().central_control_feature_enabled) {
    ASSERT_TRUE(controller_->GetBrowserProcessSnapshotController());

    // This callback should be invoked from
    // AppendCommandLineSwitchForChildProcess to bind the child end of the mojo
    // pipe.
    controller_->GetBrowserProcessSnapshotController()
        ->SetBindRemoteForChildProcessCallback(base::BindLambdaForTesting(
            [&](int child_process_id,
                mojo::PendingReceiver<mojom::SnapshotController> receiver) {
              EXPECT_EQ(child_process_id, kTestChildProcessId);
              mock_receiver.Bind(std::move(receiver));
            }));
  } else {
    EXPECT_FALSE(controller_->GetBrowserProcessSnapshotController());
  }

  if (GetParam().central_control_feature_enabled) {
    // Simulate a child process launch. If profiling is enabled in both browser
    // and child processes, this will bind the browser end of the mojo pipe to
    // the BrowserProcessSnapshotController and use the above callback to bind
    // the child end to `mock_child_snapshot_controller`.
    base::CommandLine child_command_line(base::CommandLine::NO_PROGRAM);
    controller_->AppendCommandLineSwitchForChildProcess(
        &child_command_line, ProcessType::kUtility, kTestChildProcessId);

    if (GetParam().stable.expect_child_sample) {
      EXPECT_CALL(mock_child_snapshot_controller, TakeSnapshot()).WillOnce([&] {
        // Record that BrowserProcessSnapshotController triggered a fake
        // snapshot in the child process.
        callbacks.other_process_callback().Run();
      });
    } else {
      EXPECT_CALL(mock_child_snapshot_controller, TakeSnapshot()).Times(0);
    }
  }

  AddOneSampleAndWait();
  EXPECT_EQ(sample_received_, GetParam().stable.expect_browser_sample);
}

TEST_P(HeapProfilerControllerProcessTest, ChildProcess) {
  const bool profiling_enabled =
      base::Contains(GetParam().supported_processes, ProcessType::kUtility);
  // If HeapProfilingCentralControl is enabled, TakeSnapshot() is only called in
  // the child process when the browser process triggers it. Otherwise it's
  // always called when profiling is enabled for the process.
  ScopedCallbacks callbacks = CreateScopedCallbacks(
      /*expect_take_snapshot=*/GetParam().central_control_feature_enabled
          ? GetParam().stable.expect_child_sample
          : profiling_enabled,
      /*expect_sampled_profile=*/GetParam().stable.expect_child_sample,
      /*use_other_process_callback=*/
      GetParam().central_control_feature_enabled);

  // Simulate the browser side of child process launching.
  base::test::ScopedCommandLine scoped_command_line;
  if (GetParam().central_control_feature_enabled) {
    constexpr int kTestChildProcessId = 1;

    // Create a snapshot controller to hold the browser end of the mojo pipe.
    auto snapshot_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    auto fake_browser_snapshot_controller =
        std::make_unique<BrowserProcessSnapshotController>(
            snapshot_task_runner);

    // This callback should be invoked from AppendCommandLineSwitchForTesting to
    // bind the child end of the mojo pipe.
    fake_browser_snapshot_controller->SetBindRemoteForChildProcessCallback(
        base::BindLambdaForTesting(
            [&](int child_process_id,
                mojo::PendingReceiver<mojom::SnapshotController> receiver) {
              EXPECT_EQ(child_process_id, kTestChildProcessId);
              ChildProcessSnapshotController::CreateSelfOwnedReceiver(
                  std::move(receiver));
            }));

    HeapProfilerController::AppendCommandLineSwitchForTesting(
        scoped_command_line.GetProcessCommandLine(), ProcessType::kUtility,
        kTestChildProcessId, fake_browser_snapshot_controller.get());

    // Simulate the browser process taking a sample after a delay. If profiling
    // isn't enabled in the browser process, just quit waiting after the delay.
    base::OnceClosure browser_snapshot_callback = base::DoNothing();
    if (base::Contains(GetParam().supported_processes, ProcessType::kBrowser)) {
      browser_snapshot_callback = base::BindOnce(
          &BrowserProcessSnapshotController::TakeSnapshotsOnSnapshotSequence,
          std::move(fake_browser_snapshot_controller));
    }
    snapshot_task_runner->PostDelayedTask(
        FROM_HERE,
        std::move(browser_snapshot_callback)
            .Then(callbacks.other_process_callback()),
        TestTimeouts::action_timeout());
  }

  StartHeapProfiling(version_info::Channel::STABLE, ProcessType::kUtility,
                     profiling_enabled, callbacks.first_snapshot_callback(),
                     callbacks.collector_callback());
  histogram_tester_.ExpectUniqueSample(
      "HeapProfiling.InProcess.Enabled.Utility", profiling_enabled, 1);
  histogram_tester_.ExpectUniqueSample("HeapProfiling.InProcess.Enabled",
                                       profiling_enabled, 1);

  // The child process HeapProfilerController should never have a
  // BrowserProcessSnapshotController. (`fake_browser_snapshot_controller`
  // simulates the browser side of the connection.)
  EXPECT_EQ(controller_->GetBrowserProcessSnapshotController(), nullptr);

  AddOneSampleAndWait();
  EXPECT_EQ(sample_received_, GetParam().stable.expect_child_sample);
}

// Test the HeapProfilerIncludeZero feature.
constexpr FeatureTestParams kIncludeZeroConfigs[] = {
    {
        .include_zero_feature_enabled = true,
    },
    {
        .include_zero_feature_enabled = false,
    },
};

using HeapProfilerControllerIncludeZeroTest = HeapProfilerControllerTest;

INSTANTIATE_TEST_SUITE_P(All,
                         HeapProfilerControllerIncludeZeroTest,
                         ::testing::ValuesIn(kIncludeZeroConfigs));

TEST_P(HeapProfilerControllerIncludeZeroTest, EmptyProfile) {
  // TakeSnapshot() is always called, but since no memory is allocated it will
  // only save a profile if HeapProfilerIncludeZero is enabled.
  ScopedCallbacks callbacks = CreateScopedCallbacks(
      /*expect_take_snapshot=*/true, GetParam().include_zero_feature_enabled);
  StartHeapProfiling(version_info::Channel::STABLE, ProcessType::kBrowser,
                     /*expect_enabled=*/true,
                     callbacks.first_snapshot_callback(),
                     callbacks.collector_callback());
  task_environment_.RunUntilQuit();
  EXPECT_EQ(sample_received_, GetParam().include_zero_feature_enabled);
}

}  // namespace

}  // namespace heap_profiling
