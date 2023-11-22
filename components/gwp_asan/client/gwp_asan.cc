// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/gwp_asan.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <tuple>

#include "base/allocator/partition_alloc_support.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_math.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/client/gwp_asan_features.h"
#include "components/gwp_asan/client/lightweight_detector/poison_metadata_recorder.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "components/gwp_asan/client/lightweight_detector/malloc_shims.h"
#include "components/gwp_asan/client/sampling_malloc_shims.h"
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "components/gwp_asan/client/lightweight_detector/partitionalloc_shims.h"
#include "components/gwp_asan/client/sampling_partitionalloc_shims.h"
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

namespace gwp_asan {

namespace internal {
namespace {

[[maybe_unused]] constexpr bool kCpuIs64Bit =
#if defined(ARCH_CPU_64_BITS)
    true;
#else
    false;
#endif

// GWP-ASAN's default parameters are as follows:
// MaxAllocations determines the maximum number of simultaneous allocations
// allocated from the GWP-ASAN region.
//
// MaxMetadata determines the number of slots in the GWP-ASAN region that have
// associated metadata (e.g. alloc/dealloc stack traces).
//
// TotalPages determines the maximum number of slots used for allocations in the
// GWP-ASAN region. The defaults below use MaxMetadata * 2 on 32-bit builds
// (where OOMing due to lack of address space is a concern.)
//
// The allocation sampling frequency is calculated using the formula:
// SamplingMultiplier * AllocationSamplingRange**rand
// where rand is a random real number in the range [0,1).
//
// ProcessSamplingProbability is the probability of enabling GWP-ASAN in a new
// process.
//
// ProcessSamplingBoost is the multiplier to increase the
// ProcessSamplingProbability in scenarios where we want to perform additional
// testing (e.g., on canary/dev builds).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
constexpr int kDefaultMaxAllocations = 50;
constexpr int kDefaultMaxMetadata = 210;
constexpr int kDefaultTotalPages = kCpuIs64Bit ? 2048 : kDefaultMaxMetadata * 2;
constexpr int kDefaultAllocationSamplingMultiplier = 1500;
constexpr int kDefaultAllocationSamplingRange = 16;
constexpr double kDefaultProcessSamplingProbability = 0.01;
#elif BUILDFLAG(IS_ANDROID)
constexpr int kDefaultMaxAllocations = 70;
constexpr int kDefaultMaxMetadata = 255;
constexpr int kDefaultTotalPages = 512;
constexpr int kDefaultAllocationSamplingMultiplier = 2000;
constexpr int kDefaultAllocationSamplingRange = 20;
constexpr double kDefaultProcessSamplingProbability = 0.015;
#else
constexpr int kDefaultMaxAllocations = 70;
constexpr int kDefaultMaxMetadata = 255;
constexpr int kDefaultTotalPages = kCpuIs64Bit ? 2048 : kDefaultMaxMetadata * 2;
constexpr int kDefaultAllocationSamplingMultiplier = 1000;
constexpr int kDefaultAllocationSamplingRange = 16;
constexpr double kDefaultProcessSamplingProbability = 0.015;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_FUCHSIA)
constexpr int kDefaultProcessSamplingBoost2 = 10;

#if defined(ARCH_CPU_64_BITS)
// The aim is to have the same memory overhead as the default GWP-ASan mode,
// which is:
//   sizeof(SlotMetadata) * kDefaultMaxMetadata +
//     sizeof(SystemPage) * kDefaultMaxAllocations
// The memory overhead of Lightweight UAF detector is:
//   sizeof(LightweightSlotMetadata) * kDefaultMaxLightweightMetadata
constexpr int kDefaultMaxLightweightMetadata = 3000;
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
constexpr int kDefaultMaxTotalSize = 65536;

// A set of parameters temporarily used by the random sampling LUD experiment.
constexpr int kDefaultTotalSizeHighWaterMark = kDefaultMaxTotalSize * 0.8;
constexpr int kDefaultTotalSizeLowWaterMark = kDefaultMaxTotalSize * 0.7;
constexpr int kDefaultEvictionChunkSize = 128;
constexpr int kDefaultEvictionTaskIntervalMs = 1000;

constexpr int kMaxMaxTotalSize = 2 * 1024 * 1024;
constexpr int kMaxEvictionChunkSize = 1024;
constexpr int kMaxEvictionTaskIntervalMs = 10000;
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
#endif  // defined(ARCH_CPU_64_BITS)

BASE_FEATURE(kLightweightUafDetector,
             "LightweightUafDetector",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<LightweightDetectorMode>::Option
    kLightweightUafDetectorModeOptions[] = {
        {LightweightDetectorMode::kBrpQuarantine, "BrpQuarantine"},
        {LightweightDetectorMode::kRandom, "Random"}};

const base::FeatureParam<LightweightDetectorMode>
    kLightweightUafDetectorModeParam{&kLightweightUafDetector, "Mode",
                                     LightweightDetectorMode::kBrpQuarantine,
                                     &kLightweightUafDetectorModeOptions};

// Returns whether this process should be sampled to enable GWP-ASan.
bool SampleProcess(const base::Feature& feature, bool boost_sampling) {
  double process_sampling_probability =
      GetFieldTrialParamByFeatureAsDouble(feature, "ProcessSamplingProbability",
                                          kDefaultProcessSamplingProbability);
  if (process_sampling_probability < 0.0 ||
      process_sampling_probability > 1.0) {
    DLOG(ERROR) << feature.name
                << " ProcessSamplingProbability is out-of-range: "
                << process_sampling_probability;
    return false;
  }

  int process_sampling_boost = GetFieldTrialParamByFeatureAsInt(
      feature, "ProcessSamplingBoost2", kDefaultProcessSamplingBoost2);
  if (process_sampling_boost < 1) {
    DLOG(ERROR) << feature.name
                << " ProcessSampling multiplier is out-of-range: "
                << process_sampling_boost;
    return false;
  }

  base::CheckedNumeric<double> sampling_prob_mult =
      process_sampling_probability;
  if (boost_sampling)
    sampling_prob_mult *= process_sampling_boost;
  if (!sampling_prob_mult.IsValid()) {
    DLOG(ERROR) << feature.name << " multiplier caused out-of-range multiply: "
                << process_sampling_boost;
    return false;
  }

  process_sampling_probability = sampling_prob_mult.ValueOrDie();
  return (base::RandDouble() < process_sampling_probability);
}

// Returns the allocation sampling frequency, or 0 on error.
size_t AllocationSamplingFrequency(const base::Feature& feature) {
  int multiplier =
      GetFieldTrialParamByFeatureAsInt(feature, "AllocationSamplingMultiplier",
                                       kDefaultAllocationSamplingMultiplier);
  if (multiplier < 1) {
    DLOG(ERROR) << feature.name
                << " AllocationSamplingMultiplier is out-of-range: "
                << multiplier;
    return 0;
  }

  int range = GetFieldTrialParamByFeatureAsInt(
      feature, "AllocationSamplingRange", kDefaultAllocationSamplingRange);
  if (range < 1) {
    DLOG(ERROR) << feature.name
                << " AllocationSamplingRange is out-of-range: " << range;
    return 0;
  }

  base::CheckedNumeric<size_t> frequency = multiplier;
  frequency *= std::pow(range, base::RandDouble());
  if (!frequency.IsValid()) {
    DLOG(ERROR) << feature.name << "Out-of-range multiply " << multiplier << " "
                << range;
    return 0;
  }

  return frequency.ValueOrDie();
}

}  // namespace

// Exported for testing.
GWP_ASAN_EXPORT absl::optional<AllocatorSettings> GetAllocatorSettings(
    const base::Feature& feature,
    bool boost_sampling,
    const char* process_type) {
  if (!base::FeatureList::IsEnabled(feature))
    return absl::nullopt;

  static_assert(
      AllocatorState::kMaxRequestedSlots <= std::numeric_limits<int>::max(),
      "kMaxRequestedSlots out of range");
  constexpr int kMaxRequestedSlots =
      static_cast<int>(AllocatorState::kMaxRequestedSlots);

  static_assert(AllocatorState::kMaxMetadata <= std::numeric_limits<int>::max(),
                "AllocatorState::kMaxMetadata out of range");
  constexpr int kMaxMetadata = static_cast<int>(AllocatorState::kMaxMetadata);

  int total_pages = GetFieldTrialParamByFeatureAsInt(feature, "TotalPages",
                                                     kDefaultTotalPages);
  if (total_pages < 1 || total_pages > kMaxRequestedSlots) {
    DLOG(ERROR) << feature.name
                << " TotalPages is out-of-range: " << total_pages;
    return absl::nullopt;
  }

  int max_metadata = GetFieldTrialParamByFeatureAsInt(feature, "MaxMetadata",
                                                      kDefaultMaxMetadata);
  if (max_metadata < 1 || max_metadata > std::min(total_pages, kMaxMetadata)) {
    DLOG(ERROR) << feature.name
                << " MaxMetadata is out-of-range: " << max_metadata
                << " with TotalPages = " << total_pages;
    return absl::nullopt;
  }

  int max_allocations = GetFieldTrialParamByFeatureAsInt(
      feature, "MaxAllocations", kDefaultMaxAllocations);
  if (max_allocations < 1 || max_allocations > max_metadata) {
    DLOG(ERROR) << feature.name
                << " MaxAllocations is out-of-range: " << max_allocations
                << " with MaxMetadata = " << max_metadata;
    return absl::nullopt;
  }

  size_t alloc_sampling_freq = AllocationSamplingFrequency(feature);
  if (!alloc_sampling_freq)
    return absl::nullopt;

  if (!SampleProcess(feature, boost_sampling))
    return absl::nullopt;

  return AllocatorSettings{
      static_cast<size_t>(max_allocations), static_cast<size_t>(max_metadata),
      static_cast<size_t>(total_pages), alloc_sampling_freq};
}

bool MaybeEnableLightweightDetectorInternal(bool boost_sampling,
                                            const char* process_type) {
// The detector is not used on 32-bit systems because pointers there aren't big
// enough to safely store metadata IDs.
#if defined(ARCH_CPU_64_BITS)
  const auto& feature = kLightweightUafDetector;

  if (!base::FeatureList::IsEnabled(feature)) {
    return false;
  }

  if (!SampleProcess(feature, boost_sampling)) {
    return false;
  }

  static_assert(
      LightweightDetectorState::kMaxMetadata <= std::numeric_limits<int>::max(),
      "LightweightDetectorState::kMaxMetadata out of range");
  constexpr int kMaxMetadata =
      static_cast<int>(LightweightDetectorState::kMaxMetadata);

  int max_metadata = GetFieldTrialParamByFeatureAsInt(
      feature, "MaxMetadata", kDefaultMaxLightweightMetadata);
  if (max_metadata < 1 || max_metadata > kMaxMetadata) {
    DLOG(ERROR) << feature.name
                << " MaxMetadata is out-of-range: " << max_metadata;
    return false;
  }

  switch (kLightweightUafDetectorModeParam.Get()) {
#if BUILDFLAG(USE_PARTITION_ALLOC)
    case LightweightDetectorMode::kBrpQuarantine: {
      if (!base::allocator::PartitionAllocSupport::GetBrpConfiguration(
               process_type)
               .enable_brp) {
        return false;
      }

      lud::PoisonMetadataRecorder::Init(LightweightDetectorMode::kBrpQuarantine,
                                        static_cast<size_t>(max_metadata));
      static crash_reporter::CrashKeyString<24> crash_key(
          kLightweightDetectorCrashKey);
      crash_key.Set(lud::PoisonMetadataRecorder::Get()->GetCrashKey());
      lud::InstallPartitionAllocHooks();
      return true;
    }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
    case LightweightDetectorMode::kRandom: {
      int max_allocations = GetFieldTrialParamByFeatureAsInt(
          feature, "MaxAllocations", kDefaultMaxAllocations);
      if (max_allocations < 1 || max_allocations > max_metadata) {
        DLOG(ERROR) << feature.name
                    << " MaxAllocations is out-of-range: " << max_allocations
                    << " with MaxMetadata = " << max_metadata;
        return false;
      }

      int max_total_size = GetFieldTrialParamByFeatureAsInt(
          feature, "MaxTotalSize", kDefaultMaxTotalSize);
      if (max_total_size < 1 || max_total_size > kMaxMaxTotalSize) {
        DLOG(ERROR) << feature.name
                    << " MaxTotalSize is out-of-range: " << max_total_size;
        return false;
      }

      int total_size_high_water_mark = GetFieldTrialParamByFeatureAsInt(
          feature, "TotalSizeHighWaterMark", kDefaultTotalSizeHighWaterMark);
      if (total_size_high_water_mark < 1 ||
          total_size_high_water_mark >= max_total_size) {
        DLOG(ERROR) << feature.name
                    << " TotalSizeHighWaterMark is out-of-range: "
                    << total_size_high_water_mark;
        return false;
      }

      int total_size_low_water_mark = GetFieldTrialParamByFeatureAsInt(
          feature, "TotalSizeLowWaterMark", kDefaultTotalSizeLowWaterMark);
      if (total_size_low_water_mark < 1 ||
          total_size_low_water_mark >= total_size_high_water_mark) {
        DLOG(ERROR) << feature.name
                    << " TotalSizeLowWaterMark is out-of-range: "
                    << total_size_low_water_mark;
        return false;
      }

      int eviction_chunk_size = GetFieldTrialParamByFeatureAsInt(
          feature, "EvictionChunkSize", kDefaultEvictionChunkSize);
      if (eviction_chunk_size < 1 ||
          eviction_chunk_size > kMaxEvictionChunkSize) {
        DLOG(ERROR) << feature.name << " EvictionChunkSize is out-of-range: "
                    << eviction_chunk_size;
        return false;
      }

      int eviction_task_interval_ms = GetFieldTrialParamByFeatureAsInt(
          feature, "EvictionTaskIntervalMs", kDefaultEvictionTaskIntervalMs);
      if (eviction_task_interval_ms < 1 ||
          eviction_task_interval_ms > kMaxEvictionTaskIntervalMs) {
        DLOG(ERROR) << feature.name
                    << " EvictionTaskIntervalMs is out-of-range: "
                    << eviction_task_interval_ms;
        return false;
      }

      size_t alloc_sampling_freq = AllocationSamplingFrequency(feature);
      if (!alloc_sampling_freq) {
        return false;
      }

      lud::PoisonMetadataRecorder::Init(LightweightDetectorMode::kRandom,
                                        static_cast<size_t>(max_metadata));
      static crash_reporter::CrashKeyString<24> crash_key(
          kLightweightDetectorCrashKey);
      crash_key.Set(lud::PoisonMetadataRecorder::Get()->GetCrashKey());
      lud::InstallMallocHooks(static_cast<size_t>(max_allocations),
                              static_cast<size_t>(max_total_size),
                              static_cast<size_t>(total_size_high_water_mark),
                              static_cast<size_t>(total_size_low_water_mark),
                              static_cast<size_t>(eviction_chunk_size),
                              static_cast<size_t>(eviction_task_interval_ms),
                              alloc_sampling_freq);
      return true;
    }
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

    default: {
      DLOG(ERROR) << "Unsupported Lightweight UAF Detector mode.";
      return false;
    }
  }
#else   // defined(ARCH_CPU_64_BITS)
  std::ignore = boost_sampling;
  std::ignore = process_type;
  std::ignore = kLightweightUafDetectorModeParam;
  return false;
#endif  // defined(ARCH_CPU_64_BITS)
}

}  // namespace internal

void EnableForMalloc(bool boost_sampling, const char* process_type) {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  static bool init_once = [&]() -> bool {
    auto settings = internal::GetAllocatorSettings(
        internal::kGwpAsanMalloc, boost_sampling, process_type);
    if (!settings)
      return false;

    internal::InstallMallocHooks(
        settings->max_allocated_pages, settings->num_metadata,
        settings->total_pages, settings->sampling_frequency, base::DoNothing());
    return true;
  }();
  std::ignore = init_once;
#else
  std::ignore = internal::kGwpAsanMalloc;
  DLOG(WARNING) << "base::allocator shims are unavailable for GWP-ASan.";
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
}

void EnableForPartitionAlloc(bool boost_sampling, const char* process_type) {
#if BUILDFLAG(USE_PARTITION_ALLOC)
  static bool init_once = [&]() -> bool {
    auto settings = internal::GetAllocatorSettings(
        internal::kGwpAsanPartitionAlloc, boost_sampling, process_type);
    if (!settings)
      return false;

    internal::InstallPartitionAllocHooks(
        settings->max_allocated_pages, settings->num_metadata,
        settings->total_pages, settings->sampling_frequency, base::DoNothing());
    return true;
  }();
  std::ignore = init_once;
#else
  std::ignore = internal::kGwpAsanPartitionAlloc;
  DLOG(WARNING) << "PartitionAlloc hooks are unavailable for GWP-ASan.";
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)
}

void MaybeEnableLightweightDetector(bool boost_sampling,
                                    const char* process_type) {
  [[maybe_unused]] static bool init_once =
      internal::MaybeEnableLightweightDetectorInternal(boost_sampling,
                                                       process_type);
}

}  // namespace gwp_asan
