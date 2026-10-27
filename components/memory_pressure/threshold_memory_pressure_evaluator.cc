// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/threshold_memory_pressure_evaluator.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "components/memory_pressure/memory_pressure_voter.h"

#if BUILDFLAG(IS_WIN)
#include "base/metrics/histogram_functions.h"
#endif

namespace memory_pressure {

#if BUILDFLAG(IS_WIN)
namespace features {

// When enabled, allows setting custom thresholds for commit-based
// memory pressure detection via the |kCommitAvailableCriticalThresholdMB|
// and |kCommitAvailableModerateThresholdMB| parameters.
BASE_FEATURE(kCommitAvailableMemoryPressureThresholds,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace {

// Default thresholds for commit-based memory pressure detection.
constexpr base::ByteSize kDefaultCommitAvailableCriticalThreshold =
    base::MiB(200);
constexpr base::ByteSize kDefaultCommitAvailableModerateThreshold =
    base::MiB(500);

// The amount of commit available below which the system is considered to be
// under critical memory pressure. The default value is equal to
// kSmallMemoryDefaultCriticalThreshold (200MiB).
BASE_FEATURE_PARAM(int,
                   kCommitAvailableCriticalThresholdMB,
                   &features::kCommitAvailableMemoryPressureThresholds,
                   kDefaultCommitAvailableCriticalThreshold.InMiB());

// The amount of commit available (in MB) below which the system is considered
// to be under moderate memory pressure. The default value is equal to
// kSmallMemoryDefaultModerateThresholdMb (500).
BASE_FEATURE_PARAM(int,
                   kCommitAvailableModerateThresholdMB,
                   &features::kCommitAvailableMemoryPressureThresholds,
                   kDefaultCommitAvailableModerateThreshold.InMiB());

// Constant for early exit commit threshold. Used for the initial pressure check
// to avoid activating the feature study group for users with ample memory.
// Value based on Memory.CommitAvailableMB UMA, aiming to capture a population
// similar in size (~13%) to the existing physical memory signal.
constexpr base::ByteSize kEarlyExitCommitThreshold = base::GiB(2);

}  // namespace
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
namespace features {
BASE_FEATURE(kMacAvailableMemoryPressureEvaluator,
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

namespace {

// Default physical memory thresholds for macOS.
//
// Unlike Windows (~300 MiB available RAM paging limit), Chromium's available
// physical memory equation on macOS incorporates file-backed caches:
// ((free_count - speculative_count) * PAGE_SIZE) + file_backed_cache.
//
// On Apple platforms, the operating system retains a substantial, irreducible
// baseline of disk-mapped executable code and system frameworks within
// file-backed RAM even during high memory pressure and memory compression
// exhaustion. As a result, system lockups and OS out-of-memory conditions occur
// at significantly higher computed memory floors than on Windows.
//
// Defaulting to 1200 MiB Moderate / 800 MiB Critical sits safely above this
// file-backed baseline without triggering premature discarding on consumer
// Macs.
constexpr base::ByteSize kDefaultMacModerateThreshold = base::MiB(1200);
constexpr base::ByteSize kDefaultMacCriticalThreshold = base::MiB(800);

BASE_FEATURE_PARAM(int,
                   kMacModerateThresholdMB,
                   &features::kMacAvailableMemoryPressureEvaluator,
                   kDefaultMacModerateThreshold.InMiB());
BASE_FEATURE_PARAM(int,
                   kMacCriticalThresholdMB,
                   &features::kMacAvailableMemoryPressureEvaluator,
                   kDefaultMacCriticalThreshold.InMiB());

}  // namespace
#endif  // BUILDFLAG(IS_MAC)

ThresholdMemoryPressureEvaluator::ThresholdMemoryPressureEvaluator(
    base::ByteSize moderate_threshold,
    base::ByteSize critical_threshold,
    std::unique_ptr<MemoryPressureVoter> voter)
    : AvailableMemoryMonitorEvaluator(std::move(voter)),
      moderate_threshold_(moderate_threshold),
      critical_threshold_(critical_threshold) {
  DCHECK_GE(moderate_threshold_, critical_threshold_);
  DCHECK(critical_threshold_.is_positive());
}

ThresholdMemoryPressureEvaluator::~ThresholdMemoryPressureEvaluator() = default;

#if BUILDFLAG(IS_WIN)
// static
std::unique_ptr<ThresholdMemoryPressureEvaluator>
ThresholdMemoryPressureEvaluator::CreateWithWinDefaults(
    std::unique_ptr<MemoryPressureVoter> voter) {
  return std::make_unique<ThresholdMemoryPressureEvaluator>(
      kDefaultWinModerateThreshold, kDefaultWinCriticalThreshold,
      std::move(voter));
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
// static
std::unique_ptr<ThresholdMemoryPressureEvaluator>
ThresholdMemoryPressureEvaluator::CreateWithMacFeatureParams(
    std::unique_ptr<MemoryPressureVoter> voter) {
  int moderate_mb = kMacModerateThresholdMB.Get();
  int critical_mb = kMacCriticalThresholdMB.Get();

  if (critical_mb <= 0 || moderate_mb < critical_mb) {
    DLOG(WARNING)
        << "Invalid Finch thresholds for Mac threshold memory pressure "
           "evaluator (Moderate: "
        << moderate_mb << " MiB, Critical: " << critical_mb
        << " MiB). Falling back to default thresholds.";
    return std::make_unique<ThresholdMemoryPressureEvaluator>(
        kDefaultMacModerateThreshold, kDefaultMacCriticalThreshold,
        std::move(voter));
  }

  return std::make_unique<ThresholdMemoryPressureEvaluator>(
      base::MiB(static_cast<uint64_t>(moderate_mb)),
      base::MiB(static_cast<uint64_t>(critical_mb)), std::move(voter));
}
#endif  // BUILDFLAG(IS_MAC)

base::MemoryPressureLevel
ThresholdMemoryPressureEvaluator::CalculateCurrentPressureLevel(
    const base::AvailableMemoryMonitor::MemorySample& sample) {
  // How much physical system memory is available for use right now.
  base::ByteSize phys_free = sample.available_physical_bytes;

#if BUILDFLAG(IS_WIN)
  RecordCommitHistograms(sample);

  // The maximum amount of memory the current process can commit.
  base::ByteSize commit_available = sample.available_commit_bytes;

  if (phys_free > moderate_threshold_ &&
      commit_available > kEarlyExitCommitThreshold) {
    // No memory pressure under any of the 2 detection systems. Return
    // early to avoid activating the experiment for clients who don't
    // have memory pressure.
    return base::MEMORY_PRESSURE_LEVEL_NONE;
  }

  if (base::FeatureList::IsEnabled(
          features::kCommitAvailableMemoryPressureThresholds)) {
    if (commit_available < base::MiB(base::saturated_cast<uint64_t>(
                               kCommitAvailableCriticalThresholdMB.Get()))) {
      return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
    }

    if (commit_available < base::MiB(base::saturated_cast<uint64_t>(
                               kCommitAvailableModerateThresholdMB.Get()))) {
      return base::MEMORY_PRESSURE_LEVEL_MODERATE;
    }

    return base::MEMORY_PRESSURE_LEVEL_NONE;
  }
#endif  // BUILDFLAG(IS_WIN)

  // Note: This evaluator monitors system physical and commit memory pressure,
  // not virtual address space pressure. The browser process effectively never
  // runs out of address space. Renderers occasionally do, but long term,
  // renderers should run their own address space pressure monitors and act
  // accordingly, with the browser making cross-process decisions based on
  // system memory pressure.

  if (phys_free <= critical_threshold_) {
    return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }

  if (phys_free <= moderate_threshold_) {
    return base::MEMORY_PRESSURE_LEVEL_MODERATE;
  }

  return base::MEMORY_PRESSURE_LEVEL_NONE;
}

#if BUILDFLAG(IS_WIN)
void ThresholdMemoryPressureEvaluator::RecordCommitHistograms(
    const base::AvailableMemoryMonitor::MemorySample& sample) {
  base::ByteSize commit_limit = sample.total_commit_bytes;
  base::ByteSize commit_available = sample.available_commit_bytes;

  base::UmaHistogramCounts10M("Memory.CommitLimitMB",
                              base::saturated_cast<int>(commit_limit.InMiB()));
  base::UmaHistogramCounts10M(
      "Memory.CommitAvailableMB",
      base::saturated_cast<int>(commit_available.InMiB()));

  int percentage_used;
  if (commit_limit.is_zero()) {
    // Handle division by zero.
    percentage_used = 0;
  } else {
    uint64_t percentage_remaining =
        commit_available.InBytesF() * 100 / commit_limit.InBytesF();
    // Clamping occurs because available commit can occasionally exceed total
    // commit due to OS sampling races.
    percentage_used = static_cast<int>(
        percentage_remaining > 100 ? 0u : 100 - percentage_remaining);
  }

  base::UmaHistogramPercentage("Memory.CommitPercentageUsed", percentage_used);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace memory_pressure
