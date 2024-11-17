// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_controller.h"

#include <atomic>
#include <iomanip>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/auto_reset.h"
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
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/heap_profiling/in_process/browser_process_snapshot_controller.h"
#include "components/heap_profiling/in_process/child_process_snapshot_controller.h"
#include "components/heap_profiling/in_process/heap_profiler_parameters.h"
#include "components/heap_profiling/in_process/mojom/snapshot_controller.mojom.h"
#include "components/heap_profiling/in_process/mojom/test_connector.mojom.h"
#include "components/heap_profiling/in_process/switches.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "components/sampling_profiler/process_type.h"
#include "components/version_info/channel.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/metrics_proto/call_stack_profile.pb.h"
#include "third_party/metrics_proto/execution_context.pb.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

// Test printer for SampledProfile. This needs to be in the metrics namespace so
// GTest can find it.
void PrintTo(const SampledProfile& profile, std::ostream* os) {
  *os << "process:" << profile.process() << ",samples";
  if (profile.call_stack_profile().stack_sample_size() == 0) {
    *os << ":none";
  } else {
    for (const auto& sample : profile.call_stack_profile().stack_sample()) {
      *os << ":" << sample.count() << "/" << sample.weight();
    }
  }
  *os << ",metadata";
  if (profile.call_stack_profile().profile_metadata_size() == 0) {
    *os << ":none";
  } else {
    const auto& name_hashes = profile.call_stack_profile().metadata_name_hash();
    for (const auto& metadata :
         profile.call_stack_profile().profile_metadata()) {
      if (metadata.name_hash_index() < name_hashes.size()) {
        *os << ":" << std::hex << "0x"
            << name_hashes.at(metadata.name_hash_index()) << std::dec;
      } else {
        *os << ":unknown";
      }
      *os << "=" << metadata.value();
    }
  }
}

}  // namespace metrics

namespace heap_profiling {

namespace {

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
#define ENABLE_MULTIPROCESS_TESTS 0
#else
#define ENABLE_MULTIPROCESS_TESTS 1
#endif

using FeatureRef = base::test::FeatureRef;
using FeatureRefAndParams = base::test::FeatureRefAndParams;
using ProcessType = sampling_profiler::ProfilerProcessType;
using ProcessTypeSet =
    base::EnumSet<ProcessType, ProcessType::kUnknown, ProcessType::kMax>;
using ProfileCollectorCallback =
    base::RepeatingCallback<void(base::TimeTicks, metrics::SampledProfile)>;
using base::allocator::dispatcher::AllocationNotificationData;
using base::allocator::dispatcher::AllocationSubsystem;
using base::allocator::dispatcher::FreeNotificationData;
using ScopedMuteHookedSamplesForTesting =
    base::PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting;
using ScopedSuppressRandomnessForTesting =
    base::PoissonAllocationSampler::ScopedSuppressRandomnessForTesting;

using ::testing::_;
using ::testing::AllOf;
using ::testing::Conditional;
using ::testing::ElementsAre;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Lt;
using ::testing::Optional;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAreArray;

constexpr size_t kSamplingRate = 1024;
constexpr size_t kAllocationSize = 42 * kSamplingRate;

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
    ASSERT_TRUE(base::OptionalUnwrapTo(
        profile->contents.As<metrics::SampledProfile>(), sampled_profile));
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
  // If `expected_sampled_profiles` > 0, HeapProfilerController::TakeSnapshot()
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
                  size_t expected_sampled_profiles,
                  bool use_other_process_callback,
                  ProfileCollectorCallback profile_collector_callback,
                  base::OnceClosure quit_closure) {
    size_t num_callbacks = 0;
    if (expect_take_snapshot) {
      num_callbacks += 1;
    }
    num_callbacks += expected_sampled_profiles;
    if (use_other_process_callback) {
      // The test should invoke other_process_snapshot_callback() to simulate a
      // snapshot in another process.
      num_callbacks += 1;
    }
    barrier_closure_ =
        base::BarrierClosure(num_callbacks, std::move(quit_closure));

    // Each callback should invoke `barrier_closure_` once. If they're called
    // too often, log a test failure on the first extra call only to avoid log
    // spam. These lambdas need to take a copy of the method arguments since
    // they outlive the method` scope.
    first_snapshot_callback_ =
        base::BindLambdaForTesting([this, expect_take_snapshot] {
          if (!expect_take_snapshot) {
            FAIL() << "TakeSnapshot called unexpectedly.";
          }
          first_snapshot_count_++;
          if (first_snapshot_count_ == 1) {
            barrier_closure_.Run();
            return;
          }
          if (first_snapshot_count_ == 2) {
            FAIL() << "TakeSnapshot callback invoked too many times.";
          }
        });

    collector_callback_ = base::BindLambdaForTesting(
        [this, expected_sampled_profiles,
         callback = std::move(profile_collector_callback)](
            base::TimeTicks time_ticks, metrics::SampledProfile profile) {
          if (expected_sampled_profiles == 0) {
            FAIL() << "ProfileCollectorCallback called unexpectedly.";
          }
          collector_count_++;
          if (collector_count_ <= expected_sampled_profiles) {
            std::move(callback).Run(time_ticks, profile);
            barrier_closure_.Run();
            return;
          }
          if (collector_count_ == expected_sampled_profiles + 1) {
            FAIL() << "ProfileCollectorCallback invoked too many times.";
          }
        });

    other_process_callback_ =
        base::BindLambdaForTesting([this, use_other_process_callback] {
          if (!use_other_process_callback) {
            FAIL() << "Other process callback invoked unexpectedly.";
          }
          other_process_count_++;
          if (other_process_count_ == 1) {
            barrier_closure_.Run();
            return;
          }
          if (other_process_count_ == 2) {
            FAIL() << "Other process callback invoked too many times.";
          }
        });
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
    // Return by copy since this is a RepeatingCallback.
    return collector_callback_;
  }

  base::OnceClosure other_process_callback() {
    return std::move(other_process_callback_);
  }

 private:
  base::RepeatingClosure barrier_closure_;

  base::OnceClosure first_snapshot_callback_;
  ProfileCollectorCallback collector_callback_;
  base::OnceClosure other_process_callback_;

  size_t first_snapshot_count_ = 0;
  size_t collector_count_ = 0;
  size_t other_process_count_ = 0;
};

class ProfilerSetUpMixin {
 public:
  ProfilerSetUpMixin(const std::vector<FeatureRefAndParams>& enabled_features,
                     const std::vector<FeatureRef>& disabled_features) {
    // ScopedFeatureList must be initialized in the constructor, before any
    // threads are started.
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
    if (!base::FeatureList::IsEnabled(kHeapProfilerReporting)) {
      // Set the sampling rate manually since there's no feature param to read.
      base::SamplingHeapProfiler::Get()->SetSamplingInterval(kSamplingRate);
    }
  }

  ~ProfilerSetUpMixin() = default;

  base::test::TaskEnvironment& task_env() { return task_environment_; }

 private:
  // Initialize `mute_hooks_` before `task_environment_` so that memory
  // allocations aren't sampled while TaskEnvironment creates a thread. The
  // sampling is crashing in the hooked FreeFunc on some test bots.
  ScopedMuteHookedSamplesForTesting mute_hooks_ =
      base::SamplingHeapProfiler::Get()->MuteHookedSamplesForTesting();
  ScopedSuppressRandomnessForTesting suppress_randomness_;

  // Create `feature_list_` before `task_environment_` and destroy it after to
  // avoid a race in destruction.
  base::test::ScopedFeatureList feature_list_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO};
};

#if ENABLE_MULTIPROCESS_TESTS

constexpr char kTestChildTypeSwitch[] = "heap-profiler-test-child-type";
constexpr char kTestNumAllocationsSwitch[] =
    "heap-profiler-test-num-allocations";

// Runs the heap profiler in a multiprocess test child. This is used instead of
// HeapProfilerControllerTest::CreateHeapProfiler() in tests that create real
// child processes. (Most tests run only in the test main process and pretend
// that it's the Chrome browser process or a Chrome child process.)
class MultiprocessTestChild final : public mojom::TestConnector,
                                    public ProfilerSetUpMixin {
 public:
  MultiprocessTestChild(
      const std::vector<FeatureRefAndParams>& enabled_features,
      const std::vector<FeatureRef>& disabled_features)
      : ProfilerSetUpMixin(enabled_features, disabled_features),
        quit_closure_(task_env().QuitClosure()) {}

  ~MultiprocessTestChild() final = default;

  MultiprocessTestChild(const MultiprocessTestChild&) = delete;
  MultiprocessTestChild& operator=(const MultiprocessTestChild&) = delete;

  void RunTestInChild() {
    // Get the process type and number of allocations to simulate.
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    ASSERT_TRUE(command_line);
    int process_type = 0;
    ASSERT_TRUE(base::StringToInt(
        command_line->GetSwitchValueASCII(kTestChildTypeSwitch),
        &process_type));
    int num_allocations = 0;
    ASSERT_TRUE(base::StringToInt(
        command_line->GetSwitchValueASCII(kTestNumAllocationsSwitch),
        &num_allocations));

    // Set up mojo support and attach to the parent's pipe.
    mojo::core::ScopedIPCSupport enable_mojo(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
    mojo::IncomingInvitation invitation = mojo::IncomingInvitation::Accept(
        mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
            *command_line));

    // Handle the TestConnector::Connect() message that will connect the
    // SnapshotController and CallStackProfileCollector interfaces.
    mojo::PendingReceiver<mojom::TestConnector> pending_receiver(
        invitation.ExtractMessagePipe(0));
    mojo::Receiver<mojom::TestConnector> receiver(this,
                                                  std::move(pending_receiver));

    // Start the heap profiler and wait for TakeSnapshot() messages from the
    // parent.
    HeapProfilerController controller(version_info::Channel::STABLE,
                                      static_cast<ProcessType>(process_type));
    controller.SuppressRandomnessForTesting();
    ASSERT_TRUE(controller.IsEnabled());
    controller.StartIfEnabled();

    // Make a fixed number of allocations at different addresses to include in
    // snapshots. No need to free since the process will exit after the test.
    for (int i = 0; i < num_allocations; ++i) {
      base::PoissonAllocationSampler::Get()->OnAllocation(
          AllocationNotificationData(reinterpret_cast<void*>(0x1337 + i),
                                     kAllocationSize, nullptr,
                                     AllocationSubsystem::kManualForTesting));
    }

    // Loop until the TestConnector::Disconnect() message.
    task_env().RunUntilQuit();
  }

  // mojom::TestConnector:

  void ConnectSnapshotController(
      mojo::PendingReceiver<mojom::SnapshotController> controller,
      base::OnceClosure done_callback) final {
    ChildProcessSnapshotController::CreateSelfOwnedReceiver(
        std::move(controller));
    std::move(done_callback).Run();
  }

  void ConnectProfileCollector(
      mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> collector,
      base::OnceClosure done_callback) final {
    metrics::CallStackProfileBuilder::SetParentProfileCollectorForChildProcess(
        std::move(collector));
    std::move(done_callback).Run();
  }

  void Disconnect() final { std::move(quit_closure_).Run(); }

 private:
  base::OnceClosure quit_closure_;
};

// Manages a set of multiprocess test children and mojo connections to them.
// Created in test cases in the parent process.
class MultiprocessTestParent {
 public:
  MultiprocessTestParent() = default;

  MultiprocessTestParent(const MultiprocessTestParent&) = delete;
  MultiprocessTestParent& operator=(const MultiprocessTestParent&) = delete;

  ~MultiprocessTestParent() {
    // Tell all children to stop profiling and wait for them to exit.
    for (auto& connector : test_connectors_) {
      connector->Disconnect();
    }
    for (const auto& process : child_processes_) {
      int exit_code = 0;
      EXPECT_TRUE(base::WaitForMultiprocessTestChildExit(
          process, TestTimeouts::action_timeout(), &exit_code));
      EXPECT_EQ(exit_code, 0);
    }
  }

  // Waits until `num_children` are connected, then starts profiling the parent
  // process with `controller`.
  void StartHeapProfilingWhenChildrenConnected(
      size_t num_children,
      HeapProfilerController* controller) {
    // StartIfEnabled() needs to run on the current sequence no matter what
    // thread mojo calls `on_child_connected_closure_` from.
    on_child_connected_closure_ = base::BarrierClosure(
        num_children, base::BindPostTaskToCurrentDefault(
                          base::BindLambdaForTesting([this, controller] {
                            // Make sure all children connected successfully.
                            ASSERT_EQ(test_connectors_.size(),
                                      child_processes_.size());
                            EXPECT_TRUE(controller->StartIfEnabled());
                          })));
  }

  // Called from HeapProfilerController::AppendCommandLineSwitchForChildProcess
  // with `connector_id` and `receiver`, plus a `remote` added by the test.
  // `connector_id` is the id of a mojo TestConnector interface for the process.
  // In production this parameter is the child process id.
  void BindTestConnector(
      int connector_id,
      mojo::PendingReceiver<mojom::SnapshotController> receiver,
      mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> remote) {
    mojom::TestConnector* connector = test_connectors_.Get(
        mojo::RemoteSetElementId::FromUnsafeValue(connector_id));
    ASSERT_TRUE(connector);

    // BrowserProcessSnapshotController holds the remote end of the
    // mojom::SnapshotController. Pass the other end to the test child if it
    // should be profiled, otherwise drop it. This verifies that an unbound
    // SnapshotController is handled correctly.
    base::OnceClosure bind_receiver_closure;
    if (should_profile_next_launch_) {
      // Unretained is safe since `test_connectors_` won't be destroyed until
      // the reply callback runs.
      bind_receiver_closure =
          base::BindOnce(&mojom::TestConnector::ConnectSnapshotController,
                         base::Unretained(connector), std::move(receiver),
                         on_child_connected_closure_);
    } else {
      bind_receiver_closure = on_child_connected_closure_;
    }

    // The test fixture holds the receiver end of the CallStackProfileCollector.
    // Pass the other end to the test child. The response will eventually
    // trigger `on_child_connected_closure_`.
    connector->ConnectProfileCollector(std::move(remote),
                                       std::move(bind_receiver_closure));
  }

  // Launches a multiprocess test child and registers it with `controller`.
  // The child will simulate a process of type `process_type` and make
  // `num_allocations` memory allocations to report in heap snapshots. If
  // `should_profile` is false, simulate the embedder refusing to profile the
  // child process.
  void LaunchTestChild(HeapProfilerController* controller,
                       ProcessType process_type,
                       int num_allocations,
                       bool should_profile) {
    // `should_profile` will apply during next call to BindTestConnector().
    base::AutoReset should_profile_next_launch(&should_profile_next_launch_,
                                               should_profile);

    base::LaunchOptions launch_options;
    base::CommandLine child_command_line =
        base::GetMultiProcessTestChildBaseCommandLine();
    child_command_line.AppendSwitchASCII(
        kTestChildTypeSwitch,
        base::NumberToString(static_cast<int>(process_type)));
    child_command_line.AppendSwitchASCII(kTestNumAllocationsSwitch,
                                         base::NumberToString(num_allocations));

    // Attach a mojo channel to the child.
    mojo::PlatformChannel channel;
    channel.PrepareToPassRemoteEndpoint(&launch_options, &child_command_line);
    mojo::OutgoingInvitation invitation;
    mojo::PendingRemote<mojom::TestConnector> pending_connector(
        invitation.AttachMessagePipe(0), 0);
    mojo::RemoteSetElementId connector_id =
        test_connectors_.Add(std::move(pending_connector));

    // In production this only connects the parent end of the SnapshotController
    // since content::ChildProcessHost brokers the interface with the child. For
    // the test, smuggle the id of a TestConnector to broker the interface by
    // pretending it's the child process id.
    controller->AppendCommandLineSwitchForChildProcess(
        &child_command_line, process_type, connector_id.GetUnsafeValue());

    base::Process child_process = base::SpawnMultiProcessTestChild(
        "HeapProfilerControllerChildMain", child_command_line, launch_options);
    ASSERT_TRUE(child_process.IsValid());

    // Finish connecting the mojo channel. This passes the other end of the
    // TestConnector message pipe to the child.
    channel.RemoteProcessLaunchAttempted();
    mojo::OutgoingInvitation::Send(std::move(invitation),
                                   child_process.Handle(),
                                   channel.TakeLocalEndpoint());

    child_processes_.push_back(std::move(child_process));
  }

 private:
  // All child processes started by the test. If a child dies the process will
  // become invalid but remain in this list.
  std::vector<base::Process> child_processes_;

  // Test interface for controlling each child process. If a child dies the
  // interface will be disconnected and removed from this set.
  mojo::RemoteSet<mojom::TestConnector> test_connectors_;

  // Closure to call whenever a child process is finished connecting.
  base::RepeatingClosure on_child_connected_closure_;

  // While this is false, calls to BindTestConnector should skip binding the
  // mojom::SnapshotController, to verify that BrowserProcessSnapshotController
  // can handle an embedder that doesn't bind the controller to some processes.
  bool should_profile_next_launch_ = true;
};

#endif  // ENABLE_MULTIPROCESS_TESTS

class MockSnapshotController : public mojom::SnapshotController {
 public:
  MOCK_METHOD(void, TakeSnapshot, (uint32_t, uint32_t), (override));
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
  // Whether HeapProfilerCentralControl is enabled.
  bool central_control_feature_enabled = false;
  // Probabilities for snapshotting child processes. Only used of
  // HeapProfilerCentralControl is enabled.
  int gpu_snapshot_prob = 100;
  int network_snapshot_prob = 100;
  int renderer_snapshot_prob = 100;
  int utility_snapshot_prob = 100;

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
  if (central_control_feature_enabled) {
    enabled_features.push_back(FeatureRefAndParams(
        kHeapProfilerCentralControl,
        {
            {"gpu-prob-pct", base::NumberToString(gpu_snapshot_prob)},
            {"network-prob-pct", base::NumberToString(network_snapshot_prob)},
            {"renderer-prob-pct", base::NumberToString(renderer_snapshot_prob)},
            {"utility-prob-pct", base::NumberToString(utility_snapshot_prob)},
        }));
  }
  return enabled_features;
}

std::vector<FeatureRef> FeatureTestParams::GetDisabledFeatures() const {
  std::vector<FeatureRef> disabled_features;
  if (!feature_enabled) {
    disabled_features.push_back(FeatureRef(kHeapProfilerReporting));
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
  os << "central_control:" << params.central_control_feature_enabled;
  if (params.central_control_feature_enabled) {
    os << ",gpu-prob:" << params.gpu_snapshot_prob << ",";
    os << "network-prob:" << params.network_snapshot_prob << ",";
    os << "renderer-prob:" << params.renderer_snapshot_prob << ",";
    os << "utility-prob:" << params.utility_snapshot_prob;
  }
  os << "}";
  return os;
}

class HeapProfilerControllerTest
    : public ::testing::TestWithParam<FeatureTestParams>,
      public ProfilerSetUpMixin {
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
        task_env().NowTicks() - profiler_creation_time_;
    EXPECT_EQ(sampled_profile.call_stack_profile().profile_time_offset_ms(),
              expected_time_offset.InMilliseconds());
    sample_received_ = true;
  }

 protected:
  HeapProfilerControllerTest()
      : ProfilerSetUpMixin(GetParam().GetEnabledFeatures(),
                           GetParam().GetDisabledFeatures()) {}

  ~HeapProfilerControllerTest() override {
    // Remove any collectors that were set in StartHeapProfiling.
    metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
        base::DoNothing());
    metrics::CallStackProfileBuilder::
        ResetChildCallStackProfileCollectorForTesting();
  }

  // Creates a HeapProfilerController to mock profiling a process of type
  // `process_type` on `channel`. The test should pass `expect_enabled` as true
  // if heap profiling should be enabled in this test setup.
  //
  // `first_snapshot_callback` will be invoked the first time
  // HeapProfilerController::TakeSnapshot() is called, even if it doesn't
  // collect a profile. `collector_callback` will be invoked whenever
  // TakeSnapshot() passes a profile to CallStackProfileBuilder.
  //
  // The test must call StartIfEnabled() after this to start profiling.
  void CreateHeapProfiler(
      version_info::Channel channel,
      ProcessType process_type,
      bool expect_enabled,
      base::OnceClosure first_snapshot_callback = base::DoNothing(),
      ProfileCollectorCallback collector_callback = base::DoNothing()) {
    ASSERT_FALSE(controller_) << "CreateHeapProfiler called twice";
    switch (process_type) {
      case ProcessType::kBrowser:
        expected_process_ = metrics::Process::BROWSER_PROCESS;
        metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
            std::move(collector_callback));
        break;
      case ProcessType::kUtility:
        expected_process_ = metrics::Process::UTILITY_PROCESS;
        metrics::CallStackProfileBuilder::
            SetParentProfileCollectorForChildProcess(
                AddTestProfileCollector(std::move(collector_callback)));
        break;
      default:
        // Connect up the profile collector even though we expect the heap
        // profiler not to start, so that the test environment is complete.
        expected_process_ = metrics::Process::UNKNOWN_PROCESS;
        metrics::CallStackProfileBuilder::
            SetParentProfileCollectorForChildProcess(
                AddTestProfileCollector(std::move(collector_callback)));
        break;
    }

    ASSERT_FALSE(HeapProfilerController::GetInstance());
    profiler_creation_time_ = task_env().NowTicks();
    controller_ =
        std::make_unique<HeapProfilerController>(channel, process_type);
    controller_->SuppressRandomnessForTesting();
    controller_->SetFirstSnapshotCallbackForTesting(
        std::move(first_snapshot_callback));

    EXPECT_EQ(HeapProfilerController::GetInstance(), controller_.get());
    EXPECT_EQ(controller_->IsEnabled(), expect_enabled);
  }

  // Creates a HeapProfilerController with CreateHeapProfiler() and starts
  // profiling.
  void StartHeapProfiling(
      version_info::Channel channel,
      ProcessType process_type,
      bool expect_enabled,
      base::OnceClosure first_snapshot_callback = base::DoNothing(),
      ProfileCollectorCallback collector_callback = base::DoNothing()) {
    CreateHeapProfiler(channel, process_type, expect_enabled,
                       std::move(first_snapshot_callback),
                       std::move(collector_callback));
    EXPECT_EQ(controller_->StartIfEnabled(), expect_enabled);
  }

  void AddOneSampleAndWait() {
    auto* sampler = base::PoissonAllocationSampler::Get();
    sampler->OnAllocation(AllocationNotificationData(
        reinterpret_cast<void*>(0x1337), kAllocationSize, nullptr,
        AllocationSubsystem::kManualForTesting));
    task_env().RunUntilQuit();
    // Free the allocation so that other tests can re-use the address.
    sampler->OnFree(
        FreeNotificationData(reinterpret_cast<void*>(0x1337),
                             AllocationSubsystem::kManualForTesting));
  }

  // Creates a TestCallStackProfileCollector that accepts callstacks from the
  // and passes them to `collector_callback`. Returns a remote for the profiler
  // to pass the callstacks to.
  mojo::PendingRemote<metrics::mojom::CallStackProfileCollector>
  AddTestProfileCollector(ProfileCollectorCallback collector_callback) {
    mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> remote;
    profile_collector_receivers_.Add(
        std::make_unique<TestCallStackProfileCollector>(
            std::move(collector_callback)),
        remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  ScopedCallbacks CreateScopedCallbacks(
      bool expect_take_snapshot,
      bool expect_sampled_profile,
      bool use_other_process_callback = false) {
    return ScopedCallbacks(
        expect_take_snapshot, expect_sampled_profile ? 1 : 0,
        use_other_process_callback,
        base::BindRepeating(&HeapProfilerControllerTest::RecordSampleReceived,
                            base::Unretained(this)),
        task_env().QuitClosure());
  }

  std::unique_ptr<HeapProfilerController> controller_;
  base::HistogramTester histogram_tester_;

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

  // Receivers for callstack profiles. Each element of the set is a
  // TestCallStackProfileCollecter and associated mojo::Receiver.
  mojo::UniqueReceiverSet<metrics::mojom::CallStackProfileCollector>
      profile_collector_receivers_;
};

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
    task_env().FastForwardBy(base::Days(1));
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

TEST_P(HeapProfilerControllerTest, EmptyProfile) {
  // Should save an empty profile even though no memory is allocated.
  ScopedCallbacks callbacks = CreateScopedCallbacks(
      /*expect_take_snapshot=*/true, /*expect_sampled_profile=*/true);
  StartHeapProfiling(version_info::Channel::STABLE, ProcessType::kBrowser,
                     /*expect_enabled=*/true,
                     callbacks.first_snapshot_callback(),
                     callbacks.collector_callback());
  task_env().RunUntilQuit();
  EXPECT_TRUE(sample_received_);
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
      EXPECT_CALL(mock_child_snapshot_controller, TakeSnapshot(100, 0))
          .WillOnce([&] {
            // Record that BrowserProcessSnapshotController triggered a fake
            // snapshot in the child process.
            callbacks.other_process_callback().Run();
          });
    } else {
      EXPECT_CALL(mock_child_snapshot_controller, TakeSnapshot(_, _)).Times(0);
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

#if ENABLE_MULTIPROCESS_TESTS

// Returns a lambda that can be called from a GMock matcher. It will return the
// value of a MetadataItem named `name` in a given CallStackProfile, or nullopt
// if there's no metadata with that name.
auto GetProfileMetadataFunc(std::string_view name) {
  auto get_metadata =
      [name_hash = base::HashMetricName(name)](
          const metrics::CallStackProfile& profile) -> std::optional<int64_t> {
    for (int32_t i = 0; i < profile.metadata_name_hash_size(); ++i) {
      if (profile.metadata_name_hash(i) == name_hash) {
        // Found index of `name_hash`.
        for (const auto& metadata_item : profile.profile_metadata()) {
          if (metadata_item.name_hash_index() == i) {
            return metadata_item.value();
          }
        }
      }
    }
    // No metadata matched `name_hash`.
    return std::nullopt;
  };
  return get_metadata;
}

// End-to-end test of the HeapProfilerCentralControl feature with multiple child
// processes.
constexpr FeatureTestParams kMultipleChildConfigs[] = {
    {
        .supported_processes = {ProcessType::kBrowser, ProcessType::kGpu,
                                ProcessType::kUtility, ProcessType::kRenderer},
        .central_control_feature_enabled = true,
        .renderer_snapshot_prob = 66,
        .utility_snapshot_prob = 50,
    },
};

using HeapProfilerControllerMultipleChildTest = HeapProfilerControllerTest;

INSTANTIATE_TEST_SUITE_P(All,
                         HeapProfilerControllerMultipleChildTest,
                         ::testing::ValuesIn(kMultipleChildConfigs));

MULTIPROCESS_TEST_MAIN(HeapProfilerControllerChildMain) {
  MultiprocessTestChild child(kMultipleChildConfigs[0].GetEnabledFeatures(),
                              kMultipleChildConfigs[0].GetDisabledFeatures());
  child.RunTestInChild();
  return 0;
}

TEST_P(HeapProfilerControllerMultipleChildTest, EndToEnd) {
  // Initialize mojo IPC support.
  mojo::core::ScopedIPCSupport enable_mojo(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  // Process types to test. Each will make a different
  // number of memory allocations so their reports are all different.
  const std::vector<std::pair<ProcessType, size_t>> kProcessesToTest{
      {ProcessType::kBrowser, 0},
      {ProcessType::kGpu, 1},
      // 2 utility processes.
      {ProcessType::kUtility, 2},
      {ProcessType::kUtility, 3},
      // 5 renderer processes including one with no samples. The first one will
      // be ignored to simulate the embedder refusing to profile it.
      {ProcessType::kRenderer, 10},
      {ProcessType::kRenderer, 0},
      {ProcessType::kRenderer, 4},
      {ProcessType::kRenderer, 5},
      {ProcessType::kRenderer, 6},
  };

  // Expect only 1 utility process and 3 renderer processes to be sampled due
  // to the "renderer-prob" and "utility-prob" params.
  constexpr size_t kExpectedSampledProfiles =
      /*browser*/ 1 + /*gpu*/ 1 + /*utility*/ 1 + /*renderer*/ 3;

  // Create callbacks that store profiles from all processes in a vector.
  std::vector<metrics::SampledProfile> received_profiles;
  auto collector_callback = base::BindLambdaForTesting(
      [&](base::TimeTicks, metrics::SampledProfile profile) {
        received_profiles.push_back(std::move(profile));
      });
  ScopedCallbacks callbacks(
      /*expect_take_snapshot=*/true, kExpectedSampledProfiles,
      /*use_other_process_callback=*/false, std::move(collector_callback),
      task_env().QuitClosure());

  // Snapshots from the children take real time to be passed back to the parent.
  // The mock clock will advance to the next snapshot time while waiting, so
  // stop profiling after the first snapshot by deleting the controller.
  auto stop_after_first_snapshot_callback =
      callbacks.first_snapshot_callback().Then(base::BindLambdaForTesting(
          [this, task_runner = base::SequencedTaskRunner::GetCurrentDefault()] {
            task_runner->DeleteSoon(FROM_HERE, controller_.release());
          }));

  CreateHeapProfiler(version_info::Channel::STABLE, ProcessType::kBrowser,
                     /*expect_enabled=*/true,
                     std::move(stop_after_first_snapshot_callback),
                     callbacks.collector_callback());
  ASSERT_TRUE(controller_);

  // Start all processes in `kProcessesToTest` except the browser.
  MultiprocessTestParent test_parent;
  test_parent.StartHeapProfilingWhenChildrenConnected(
      kProcessesToTest.size() - 1, controller_.get());

  // On every process launch, create a TestCallStackProfileCollector to collect
  // profiles from the child. BrowserProcessSnapshotController will create a
  // SnapshotController to trigger snapshots in the child.
  auto* browser_snapshot_controller =
      controller_->GetBrowserProcessSnapshotController();
  ASSERT_TRUE(browser_snapshot_controller);
  auto binder_callback = base::BindLambdaForTesting(
      [&](int id, mojo::PendingReceiver<mojom::SnapshotController> receiver) {
        mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> remote =
            AddTestProfileCollector(callbacks.collector_callback());
        test_parent.BindTestConnector(id, std::move(receiver),
                                      std::move(remote));
      });
  browser_snapshot_controller->SetBindRemoteForChildProcessCallback(
      std::move(binder_callback));

  bool renderer_was_skipped = false;
  for (const auto [process_type, num_allocations] : kProcessesToTest) {
    if (process_type != ProcessType::kBrowser) {
      // Skip the first renderer.
      bool should_profile = true;
      if (process_type == ProcessType::kRenderer && !renderer_was_skipped) {
        should_profile = false;
        renderer_was_skipped = true;
      }
      test_parent.LaunchTestChild(controller_.get(), process_type,
                                  num_allocations, should_profile);
    }
  }

  // Loop until all children are connected and all processes send snapshots.
  task_env().RunUntilQuit();

  // GMock matcher that tests that the given CallStackProfile contains `count`
  // stack samples with metadata containing `process_percent` and
  // "process_index" < `sampled_processes`.
  auto call_stack_profile_matches = [](size_t count, int64_t process_percent,
                                       int64_t sampled_processes) {
    using StackSample = metrics::CallStackProfile::StackSample;
    return AllOf(
        Property(
            "stack_sample", &metrics::CallStackProfile::stack_sample,
            Conditional(
                count > 0,
                // The test makes allocations at addresses without symbols, so
                // they're all counted in the same stack frame.
                ElementsAre(AllOf(Property("count", &StackSample::count, count),
                                  Property("weight", &StackSample::weight,
                                           count * kAllocationSize))),
                // No allocations means no stack frames.
                IsEmpty())),
        ResultOf("process_percent metadata",
                 GetProfileMetadataFunc("process_percent"),
                 Optional(process_percent)),
        ResultOf("process_index metadata",
                 GetProfileMetadataFunc("process_index"),
                 // Processes can be sampled in any order, so just check the
                 // range of "process_index".
                 Optional(AllOf(Ge(0), Lt(sampled_processes)))));
  };

  // GMock matcher that tests that the given SampledProfile is a heap snapshot
  // for the given `process_type` containing `count` stack samples with metadata
  // containing `process_percent` and "process_index" < `sampled_processes`.
  auto sampled_profile_matches = [&](metrics::Process process_type,
                                     size_t count, int64_t process_percent,
                                     int64_t sampled_processes) {
    return AllOf(
        Property("process", &metrics::SampledProfile::process, process_type),
        Property("call_stack_profile",
                 &metrics::SampledProfile::call_stack_profile,
                 call_stack_profile_matches(count, process_percent,
                                            sampled_processes)));
  };

  // Only the first 1/2 of utility processes and 2/3 of renderers should be
  // included due to sampling. Renderers are rounded up to 3 of the 4 that can
  // be profiled - the 5th is invisible to the profiler.
  EXPECT_THAT(
      received_profiles,
      UnorderedElementsAreArray({
          sampled_profile_matches(metrics::Process::BROWSER_PROCESS, 0, 100, 1),
          sampled_profile_matches(metrics::Process::GPU_PROCESS, 1, 100, 1),
          sampled_profile_matches(metrics::Process::UTILITY_PROCESS, 2, 50, 1),
          // The first renderer should be skipped.
          sampled_profile_matches(metrics::Process::RENDERER_PROCESS, 0, 66, 3),
          sampled_profile_matches(metrics::Process::RENDERER_PROCESS, 4, 66, 3),
          sampled_profile_matches(metrics::Process::RENDERER_PROCESS, 5, 66, 3),
      }));
}

#endif  // ENABLE_MULTIPROCESS_TESTS

}  // namespace

}  // namespace heap_profiling
