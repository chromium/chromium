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
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/profiler/frame.h"
#include "base/profiler/metadata_recorder.h"
#include "base/profiler/module_cache.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"
#include "base/sampling_heap_profiler/lock_free_bloom_filter.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
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
#include "components/variations/variations_switches.h"
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
// Returns `base_name` unchanged if `process_type` is nullopt.
std::string ProcessHistogramName(std::string_view base_name,
                                 std::optional<ProcessType> process_type) {
  if (!process_type.has_value()) {
    return std::string(base_name);
  }
  switch (process_type.value()) {
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
      NOTREACHED();
  }
}

double GetChannelProbability(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
      // If the channel can't be determined, treat it as `stable` for safety.
      // Don't disable heap profiling completely so that developers can still
      // enable it with --enable-feature flags.
      return kStableProbability.Get();
    case version_info::Channel::BETA:
    case version_info::Channel::DEV:
    case version_info::Channel::CANARY:
      return kNonStableProbability.Get();
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
  if (process_type != ProcessType::kBrowser) {
    // The browser process decided whether profiling is enabled and used
    // AppendCommandLineSwitchForChildProcess() to pass on the decision.
    const bool is_enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kSubprocessHeapProfiling);
    return {is_enabled, std::nullopt};
  }

  // Never profile during benchmarking.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          variations::switches::kEnableBenchmarking)) {
    return {false, std::nullopt};
  }

  if (!base::FeatureList::IsEnabled(kHeapProfilerReporting)) {
    return {false, std::nullopt};
  }

  // Randomly determine whether profiling is enabled.
  const double seed = base::RandDouble();
  const double probability = GetChannelProbability(channel);
  if (seed < probability) {
    return {true, "Enabled"};
  }
  if (seed < 2 * probability && 2 * probability <= 1.0) {
    // Only register a Control group if it can be the same size as Enabled.
    return {false, "Control"};
  }
  return {false, "Default"};
}

// Logs statistics about the sampling profiler.
void LogProfilerStats(std::optional<ProcessType> process_type,
                      const base::PoissonAllocationSamplerStats& profiler_stats,
                      size_t num_samples) {
  const double hit_rate =
      profiler_stats.address_cache_hits
          ? (static_cast<double>(profiler_stats.address_cache_hits) /
             (profiler_stats.address_cache_hits +
              profiler_stats.address_cache_misses))
          : 0.0;
  base::UmaHistogramCounts100000(
      ProcessHistogramName("HeapProfiling.InProcess.SamplesPerSnapshot",
                           process_type),
      num_samples);
  base::UmaHistogramCounts1M(
      ProcessHistogramName(
          "HeapProfiling.InProcess.SampledAddressCacheHitCount", process_type),
      profiler_stats.address_cache_hits);
  base::UmaHistogramCounts10000(
      ProcessHistogramName("HeapProfiling.InProcess.SampledAddressCacheHitRate",
                           process_type),
      hit_rate * 10000);
  base::UmaHistogramCounts1M(
      ProcessHistogramName("HeapProfiling.InProcess.SampledAddressCacheMaxSize",
                           process_type),
      profiler_stats.address_cache_max_size);
  base::UmaHistogramPercentage(
      ProcessHistogramName(
          "HeapProfiling.InProcess.SampledAddressCacheMaxLoadFactor",
          process_type),
      100 * profiler_stats.address_cache_max_load_factor);
  for (size_t bucket_length :
       profiler_stats.address_cache_bucket_stats.lengths) {
    base::UmaHistogramCounts100(
        ProcessHistogramName(
            "HeapProfiling.InProcess.SampledAddressCacheBucketLengths",
            process_type),
        bucket_length);
  }
  // Expected to cluster around 100% - target range is around 95% to 105%.
  base::UmaHistogramCustomCounts(
      ProcessHistogramName(
          "HeapProfiling.InProcess.SampledAddressCacheUniformity",
          process_type),
      100 * profiler_stats.address_cache_bucket_stats.chi_squared, 0, 200, 50);

  if (base::FeatureList::IsEnabled(base::kUseLockFreeBloomFilter)) {
    const size_t kMaxSaturationSize = 65;
    static_assert(kMaxSaturationSize == base::kMaxLockFreeBloomFilterBits + 1,
                  "LockFreeBloomFilter's max bits has changed. Need to update "
                  "the metric.");

    const double bloom_filter_hit_rate =
        profiler_stats.bloom_filter_hits
            ? (static_cast<double>(profiler_stats.bloom_filter_hits) /
               (profiler_stats.bloom_filter_hits +
                profiler_stats.bloom_filter_misses))
            : 0.0;
    base::UmaHistogramCounts1M(
        ProcessHistogramName("HeapProfiling.InProcess.BloomFilterHitCount",
                             process_type),
        profiler_stats.bloom_filter_hits);
    base::UmaHistogramCounts10000(
        ProcessHistogramName("HeapProfiling.InProcess.BloomFilterHitRate",
                             process_type),
        bloom_filter_hit_rate * 10000);
    base::UmaHistogramExactLinear(
        ProcessHistogramName("HeapProfiling.InProcess.BloomFilterMaxSaturation",
                             process_type),
        profiler_stats.bloom_filter_max_saturation, kMaxSaturationSize);

    base::UmaHistogramCounts1M("HeapProfiling.InProcess.BloomFilterHitCount",
                               profiler_stats.bloom_filter_hits);
    base::UmaHistogramCounts10000("HeapProfiling.InProcess.BloomFilterHitRate",
                                  bloom_filter_hit_rate * 10000);
    base::UmaHistogramExactLinear(
        "HeapProfiling.InProcess.BloomFilterMaxSaturation",
        profiler_stats.bloom_filter_max_saturation, kMaxSaturationSize);
  }
}

// Retrieves a snapshot from the SamplingHeapProfiler and logs metrics about
// profiler performance.
std::vector<base::SamplingHeapProfiler::Sample> RetrieveAndLogSnapshot(
    ProcessType process_type) {
  auto samples = base::SamplingHeapProfiler::Get()->GetSamples(0);
  const base::PoissonAllocationSamplerStats profiler_stats =
      base::PoissonAllocationSampler::Get()->GetAndResetStats();
  LogProfilerStats(process_type, profiler_stats, samples.size());
  // Also summarize over all process types.
  LogProfilerStats(std::nullopt, profiler_stats, samples.size());
  return samples;
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

  if (profiling_enabled_ && process_type_ == ProcessType::kBrowser) {
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
  const size_t sampling_rate_bytes = GetSamplingRateForProcess(process_type_);
  if (sampling_rate_bytes > 0) {
    base::SamplingHeapProfiler::Get()->SetSamplingInterval(sampling_rate_bytes);
  }
  const float hash_set_load_factor =
      GetHashSetLoadFactorForProcess(process_type_);
  if (hash_set_load_factor > 0) {
    base::PoissonAllocationSampler::Get()->SetTargetHashSetLoadFactor(
        hash_set_load_factor);
  }
  base::SamplingHeapProfiler::Get()->Start();

  if (process_type_ != ProcessType::kBrowser) {
    // ChildProcessSnapshotController will trigger snapshots.
    return true;
  }

  const base::TimeDelta collection_interval = kCollectionInterval.Get();
  CHECK(collection_interval.is_positive());
  SnapshotParams params(
      collection_interval,
      /*use_random_interval=*/!suppress_randomness_for_testing_, stopped_,
      process_type_, creation_time_, std::move(on_first_snapshot_callback_));
  params.trigger_child_process_snapshot_closure = base::BindRepeating(
      &BrowserProcessSnapshotController::TakeSnapshotsOnSnapshotSequence,
      browser_process_snapshot_controller_->GetWeakPtr());
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
  snapshot_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TakeSnapshot,
                     SnapshotParams(stopped_, process_type_, creation_time_,
                                    process_probability_pct, process_index,
                                    std::move(on_first_snapshot_callback_))));
}

void HeapProfilerController::LogMetricsWithoutSnapshotInChildProcess(
    base::PassKey<ChildProcessSnapshotController>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(process_type_, ProcessType::kBrowser);
  snapshot_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ProcessType process_type, scoped_refptr<StoppedFlag> stopped) {
            if (!stopped->data.IsSet()) {
              // Log metrics about the snapshot, but don't upload it to UMA.
              RetrieveAndLogSnapshot(process_type);
            }
          },
          process_type_, stopped_));
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
  if (snapshot_controller &&
      GetSnapshotProbabilityForProcess(child_process_type) > 0) {
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
    const int scaled_sampled_memory =
        total_sampled_bytes / kBytesPerMB * (100.0 / process_probability_pct);
    base::UmaHistogramMemoryLargeMB(
        ProcessHistogramName("HeapProfiling.InProcess.TotalSampledMemory",
                             process_type),
        scaled_sampled_memory);
    // Also summarize over all process types.
    base::UmaHistogramMemoryLargeMB(
        "HeapProfiling.InProcess.TotalSampledMemory", scaled_sampled_memory);
  };

  std::vector<Sample> samples = RetrieveAndLogSnapshot(process_type);

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
