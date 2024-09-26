// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_controller.h"

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/allocator/dispatcher/reentry_guard.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/profiler/frame.h"
#include "base/profiler/metadata_recorder.h"
#include "base/profiler/module_cache.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/heap_profiling/in_process/browser_process_snapshot_controller.h"
#include "components/heap_profiling/in_process/heap_profiler_parameters.h"
#include "components/heap_profiling/in_process/switches.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/sampling_profiler/process_type.h"
#include "components/services/heap_profiling/public/cpp/merge_samples.h"
#include "components/version_info/channel.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace heap_profiling {

namespace {

using ProcessType = sampling_profiler::ProfilerProcessType;

// The heap profiler for this process. HeapProfilerController will set this on
// creation, and reset it to nullptr on destruction, so that it's always unset
// after each unit test that creates a HeapProfilerController.
HeapProfilerController* g_instance = nullptr;

base::TimeDelta RandomInterval(base::TimeDelta mean) {
  // Time intervals between profile collections form a Poisson stream with
  // given mean interval.
  double rnd = base::RandDouble();
  if (rnd == 0) {
    // log(0) is an error.
    rnd = std::numeric_limits<double>::min();
  }
  return -std::log(rnd) * mean;
}

// Returns true iff `process_type` is handled by ProcessHistogramName.
bool HasProcessHistogramName(ProcessType process_type) {
  switch (process_type) {
    case ProcessType::kBrowser:
    case ProcessType::kRenderer:
    case ProcessType::kGpu:
    case ProcessType::kUtility:
    case ProcessType::kNetworkService:
      return true;
    case ProcessType::kUnknown:
    default:
      // Profiler should not be enabled for these process types.
      return false;
  }
}

// Returns the full name of a histogram to record by appending the
// ProfiledProcess variant name for `process_type` (defined in
// tools/metrics/histograms/metadata/memory/histograms.xml) to `base_name`.
std::string ProcessHistogramName(std::string_view base_name,
                                 ProcessType process_type) {
  switch (process_type) {
    case ProcessType::kBrowser:
      return base::StrCat({base_name, ".Browser"});
    case ProcessType::kRenderer:
      return base::StrCat({base_name, ".Renderer"});
    case ProcessType::kGpu:
      return base::StrCat({base_name, ".GPU"});
    case ProcessType::kUtility:
      return base::StrCat({base_name, ".Utility"});
    case ProcessType::kNetworkService:
      return base::StrCat({base_name, ".NetworkService"});
    case ProcessType::kUnknown:
    default:
      // Profiler should not be enabled for these process types.
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

double GetChannelProbability(version_info::Channel channel,
                             const HeapProfilerParameters& params) {
  switch (channel) {
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
      // If the channel can't be determined, treat it as `stable` for safety.
      // Don't disable heap profiling completely so that developers can still
      // enable it with --enable-feature flags.
      return params.stable_probability;
    case version_info::Channel::BETA:
    case version_info::Channel::DEV:
    case version_info::Channel::CANARY:
      return params.nonstable_probability;
  }
  NOTREACHED();
}

// Returns true iff heap profiles should be collected for this process, along
// with a name for a synthetic field trial group based on the decision or
// nullopt if no group applies.
std::pair<bool, std::optional<std::string>> DecideIfCollectionIsEnabled(
    version_info::Channel channel,
    ProcessType process_type) {
  // Check the feature before the process type so that users are assigned to
  // groups in the browser process.
  if (base::FeatureList::IsEnabled(kHeapProfilerCentralControl) &&
      process_type != ProcessType::kBrowser) {
    // The browser process decided whether profiling is enabled and used
    // AppendCommandLineSwitchForChildProcess() to pass on the decision.
    const bool is_enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kSubprocessHeapProfiling);
    return {is_enabled, std::nullopt};
  }

  // Randomly determine whether profiling is enabled.
  HeapProfilerParameters params =
      GetHeapProfilerParametersForProcess(process_type);
  if (!params.is_supported) {
    return {false, std::nullopt};
  }

  const double seed = base::RandDouble();
  const double probability = GetChannelProbability(channel, params);
  if (seed < probability) {
    return {true, "Enabled"};
  }
  if (seed < 2 * probability && 2 * probability <= 1.0) {
    // Only register a Control group if it can be the same size as Enabled.
    return {false, "Control"};
  }
  return {false, "Default"};
}

}  // namespace

HeapProfilerController::SnapshotParams::SnapshotParams(
    std::optional<base::TimeDelta> mean_interval,
    bool use_random_interval,
    scoped_refptr<StoppedFlag> stopped,
    ProcessType process_type,
    base::TimeTicks profiler_creation_time,
    base::OnceClosure on_first_snapshot_callback)
    : mean_interval(std::move(mean_interval)),
      use_random_interval(use_random_interval),
      stopped(std::move(stopped)),
      process_type(process_type),
      profiler_creation_time(profiler_creation_time),
      on_first_snapshot_callback(std::move(on_first_snapshot_callback)) {}

HeapProfilerController::SnapshotParams::SnapshotParams(
    scoped_refptr<StoppedFlag> stopped,
    ProcessType process_type,
    base::TimeTicks profiler_creation_time,
    uint32_t process_probability_pct,
    size_t process_index,
    base::OnceClosure on_first_snapshot_callback)
    : stopped(std::move(stopped)),
      process_type(process_type),
      profiler_creation_time(profiler_creation_time),
      process_probability_pct(process_probability_pct),
      process_index(process_index),
      on_first_snapshot_callback(std::move(on_first_snapshot_callback)) {}

HeapProfilerController::SnapshotParams::~SnapshotParams() = default;

HeapProfilerController::SnapshotParams::SnapshotParams(SnapshotParams&& other) =
    default;

HeapProfilerController::SnapshotParams&
HeapProfilerController::SnapshotParams::operator=(SnapshotParams&& other) =
    default;

// static
HeapProfilerController* HeapProfilerController::GetInstance() {
  return g_instance;
}

HeapProfilerController::HeapProfilerController(version_info::Channel channel,
                                               ProcessType process_type)
    : process_type_(process_type),
      stopped_(base::MakeRefCounted<StoppedFlag>()),
      snapshot_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT})) {
  // Only one HeapProfilerController should exist at a time in each
  // process.
  CHECK(!g_instance);
  g_instance = this;

  std::tie(profiling_enabled_, synthetic_field_trial_group_) =
      DecideIfCollectionIsEnabled(channel, process_type);

  // Before starting the profiler, record the ReentryGuard's TLS slot to a crash
  // key to debug reentry into the profiler.
  // TODO(crbug.com/40062835): Remove this after diagnosing reentry crashes.
  base::allocator::dispatcher::ReentryGuard::RecordTLSSlotToCrashKey();

  if (profiling_enabled_ && process_type_ == ProcessType::kBrowser &&
      base::FeatureList::IsEnabled(kHeapProfilerCentralControl)) {
    browser_process_snapshot_controller_ =
        std::make_unique<BrowserProcessSnapshotController>(
            snapshot_task_runner_);
  }
}

HeapProfilerController::~HeapProfilerController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stopped_->data.Set();
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;

  // BrowserProcessSnapshotController must be deleted on the sequence that its
  // WeakPtr's are bound to.
  if (browser_process_snapshot_controller_) {
    snapshot_task_runner_->DeleteSoon(
        FROM_HERE, std::move(browser_process_snapshot_controller_));
  }
}

bool HeapProfilerController::StartIfEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only supported processes are assigned a patterned histogram.
  if (HasProcessHistogramName(process_type_)) {
    constexpr char kEnabledHistogramName[] = "HeapProfiling.InProcess.Enabled";
    base::UmaHistogramBoolean(
        ProcessHistogramName(kEnabledHistogramName, process_type_),
        profiling_enabled_);
    // Also summarize over all supported process types.
    base::UmaHistogramBoolean(kEnabledHistogramName, profiling_enabled_);
  }
  if (!profiling_enabled_) {
    return false;
  }
  HeapProfilerParameters profiler_params =
      GetHeapProfilerParametersForProcess(process_type_);
  // DecideIfCollectionIsEnabled() should return false if not supported.
  DCHECK(profiler_params.is_supported);
  if (profiler_params.sampling_rate_bytes > 0) {
    base::SamplingHeapProfiler::Get()->SetSamplingInterval(
        profiler_params.sampling_rate_bytes);
  }
  base::SamplingHeapProfiler::Get()->Start();

  if (process_type_ != ProcessType::kBrowser &&
      base::FeatureList::IsEnabled(kHeapProfilerCentralControl)) {
    // ChildProcessSnapshotController will trigger snapshots.
    return true;
  }

  DCHECK(profiler_params.collection_interval.is_positive());
  SnapshotParams params(
      profiler_params.collection_interval,
      /*use_random_interval=*/!suppress_randomness_for_testing_, stopped_,
      process_type_, creation_time_, std::move(on_first_snapshot_callback_));
  if (base::FeatureList::IsEnabled(kHeapProfilerCentralControl)) {
    params.trigger_child_process_snapshot_closure = base::BindRepeating(
        &BrowserProcessSnapshotController::TakeSnapshotsOnSnapshotSequence,
        browser_process_snapshot_controller_->GetWeakPtr());
  } else {
    params.trigger_child_process_snapshot_closure = base::DoNothing();
  }
  snapshot_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&HeapProfilerController::ScheduleNextSnapshot,
                                std::move(params)));
  return true;
}

bool HeapProfilerController::GetSyntheticFieldTrial(
    std::string& trial_name,
    std::string& group_name) const {
  CHECK_EQ(process_type_, ProcessType::kBrowser);
  if (!synthetic_field_trial_group_.has_value()) {
    return false;
  }
  trial_name = "SyntheticHeapProfilingConfiguration";
  group_name = synthetic_field_trial_group_.value();
  return true;
}

void HeapProfilerController::SuppressRandomnessForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  suppress_randomness_for_testing_ = true;
  if (browser_process_snapshot_controller_) {
    browser_process_snapshot_controller_
        ->SuppressRandomnessForTesting();  // IN-TEST
  }
}

void HeapProfilerController::SetFirstSnapshotCallbackForTesting(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_first_snapshot_callback_ = std::move(callback);
}

void HeapProfilerController::AppendCommandLineSwitchForChildProcess(
    base::CommandLine* command_line,
    ProcessType child_process_type,
    int child_process_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(process_type_, ProcessType::kBrowser);
  if (!base::FeatureList::IsEnabled(kHeapProfilerCentralControl)) {
    return;
  }
  // If profiling is disabled in the browser process, pass a null
  // BrowserProcessSnapshotController to disable it in the child process too.
  BrowserProcessSnapshotController* snapshot_controller = nullptr;
  if (profiling_enabled_) {
    CHECK(browser_process_snapshot_controller_);
    snapshot_controller = browser_process_snapshot_controller_.get();
  }
  AppendCommandLineSwitchInternal(command_line, child_process_type,
                                  child_process_id, snapshot_controller);
}

BrowserProcessSnapshotController*
HeapProfilerController::GetBrowserProcessSnapshotController() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return browser_process_snapshot_controller_.get();
}

void HeapProfilerController::TakeSnapshotInChildProcess(
    base::PassKey<ChildProcessSnapshotController>,
    uint32_t process_probability_pct,
    size_t process_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(process_type_, ProcessType::kBrowser);
  CHECK(base::FeatureList::IsEnabled(kHeapProfilerCentralControl));
  snapshot_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TakeSnapshot,
                     SnapshotParams(stopped_, process_type_, creation_time_,
                                    process_probability_pct, process_index,
                                    std::move(on_first_snapshot_callback_))));
}

// static
void HeapProfilerController::AppendCommandLineSwitchForTesting(
    base::CommandLine* command_line,
    ProcessType child_process_type,
    int child_process_id,
    BrowserProcessSnapshotController* snapshot_controller) {
  AppendCommandLineSwitchInternal(command_line, child_process_type,
                                  child_process_id, snapshot_controller);
}

// static
void HeapProfilerController::AppendCommandLineSwitchInternal(
    base::CommandLine* command_line,
    ProcessType child_process_type,
    int child_process_id,
    BrowserProcessSnapshotController* snapshot_controller) {
  CHECK_NE(child_process_type, ProcessType::kBrowser);
  CHECK(base::FeatureList::IsEnabled(kHeapProfilerCentralControl));
  if (snapshot_controller &&
      GetHeapProfilerParametersForProcess(child_process_type).is_supported) {
    command_line->AppendSwitch(switches::kSubprocessHeapProfiling);
    snapshot_controller->BindRemoteForChildProcess(child_process_id,
                                                   child_process_type);
  }
}

// static
void HeapProfilerController::ScheduleNextSnapshot(SnapshotParams params) {
  // Should only be called for repeating snapshots.
  CHECK(params.mean_interval.has_value());
  base::TimeDelta interval = params.use_random_interval
                                 ? RandomInterval(params.mean_interval.value())
                                 : params.mean_interval.value();
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HeapProfilerController::TakeSnapshot, std::move(params)),
      interval);
}

// static
void HeapProfilerController::TakeSnapshot(SnapshotParams params) {
  if (params.stopped->data.IsSet()) {
    return;
  }
  if (!params.on_first_snapshot_callback.is_null()) {
    std::move(params.on_first_snapshot_callback).Run();
  }
  RetrieveAndSendSnapshot(
      params.process_type,
      base::TimeTicks::Now() - params.profiler_creation_time,
      params.process_probability_pct, params.process_index);
  if (params.process_type == ProcessType::kBrowser) {
    // Also trigger snapshots in child processes.
    params.trigger_child_process_snapshot_closure.Run();
  }

  if (params.mean_interval.has_value()) {
    // Callback should be left as null for next snapshot.
    CHECK(params.on_first_snapshot_callback.is_null());
    ScheduleNextSnapshot(std::move(params));
  }
}

// static
void HeapProfilerController::RetrieveAndSendSnapshot(
    ProcessType process_type,
    base::TimeDelta time_since_profiler_creation,
    uint32_t process_probability_pct,
    size_t process_index) {
  using Sample = base::SamplingHeapProfiler::Sample;

  CHECK_GT(process_probability_pct, 0u);
  CHECK_LE(process_probability_pct, 100u);

  // Always log the total sampled memory before returning. If `samples` is empty
  // this will be logged as 0 MB.
  base::ClampedNumeric<uint64_t> total_sampled_bytes;
  absl::Cleanup log_total_sampled_memory = [&total_sampled_bytes, process_type,
                                            process_probability_pct] {
    // Scale this processes' memory by the inverse of the probability that it
    // was chosen to get its estimated contribution to the total memory.
    constexpr int kBytesPerMB = 1024 * 1024;
    base::UmaHistogramMemoryLargeMB(
        ProcessHistogramName("HeapProfiling.InProcess.TotalSampledMemory",
                             process_type),
        total_sampled_bytes / kBytesPerMB * (100.0 / process_probability_pct));
  };

  std::vector<Sample> samples =
      base::SamplingHeapProfiler::Get()->GetSamples(0);
  base::UmaHistogramCounts100000(
      ProcessHistogramName("HeapProfiling.InProcess.SamplesPerSnapshot",
                           process_type),
      samples.size());
  // Also summarize over all process types.
  base::UmaHistogramCounts100000("HeapProfiling.InProcess.SamplesPerSnapshot",
                                 samples.size());

  base::ModuleCache module_cache;
  sampling_profiler::CallStackProfileParams params(
      process_type, sampling_profiler::ProfilerThreadType::kUnknown,
      sampling_profiler::CallStackProfileParams::Trigger::
          kPeriodicHeapCollection,
      time_since_profiler_creation);
  metrics::CallStackProfileBuilder profile_builder(params);

  SampleMap merged_samples = MergeSamples(samples);

  for (auto& pair : merged_samples) {
    const Sample& sample = pair.first;
    const SampleValue& value = pair.second;

    const size_t stack_size = sample.stack.size();
    std::vector<base::Frame> frames;
    frames.reserve(stack_size);

    for (const void* frame : sample.stack) {
      const uintptr_t address = reinterpret_cast<const uintptr_t>(frame);
      const base::ModuleCache::Module* module =
          module_cache.GetModuleForAddress(address);
      frames.emplace_back(address, module);
    }

    // Heap "samples" represent allocation stacks aggregated over time so
    // do not have a meaningful timestamp.
    profile_builder.OnSampleCompleted(std::move(frames), base::TimeTicks(),
                                      value.total, value.count);

    total_sampled_bytes += value.total;
  }

  // Initialize on first call since HashMetricName isn't constexpr.
  static const uint64_t kProcessPercentHash =
      base::HashMetricName("process_percent");  // 0xd598e4d0a9e55408
  static const uint64_t kProcessIndexHash =
      base::HashMetricName("process_index");  // 0x28f4372e67b3f8f8

  profile_builder.AddProfileMetadata(
      base::MetadataRecorder::Item(kProcessPercentHash, std::nullopt,
                                   std::nullopt, process_probability_pct));
  profile_builder.AddProfileMetadata(base::MetadataRecorder::Item(
      kProcessIndexHash, std::nullopt, std::nullopt, process_index));

  profile_builder.OnProfileCompleted(base::TimeDelta(), base::TimeDelta());
}

}  // namespace heap_profiling
