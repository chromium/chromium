// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/free_memory_pressure_evaluator.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "components/memory_pressure/memory_pressure_voter.h"

namespace memory_pressure {

namespace features {
BASE_FEATURE(kMacFreeMemoryPressureEvaluator,
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

namespace {

BASE_FEATURE_PARAM(
    int,
    kMacFreeModerateThresholdMB,
    &features::kMacFreeMemoryPressureEvaluator,
    FreeMemoryPressureEvaluator::kDefaultModerateThreshold.InMiB());
BASE_FEATURE_PARAM(
    int,
    kMacFreeCriticalThresholdMB,
    &features::kMacFreeMemoryPressureEvaluator,
    FreeMemoryPressureEvaluator::kDefaultCriticalThreshold.InMiB());

}  // namespace

FreeMemoryPressureEvaluator::FreeMemoryPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : FreeMemoryPressureEvaluator(kDefaultModerateThreshold,
                                  kDefaultCriticalThreshold,
                                  std::move(voter)) {}

FreeMemoryPressureEvaluator::FreeMemoryPressureEvaluator(
    base::ByteSize moderate_threshold,
    base::ByteSize critical_threshold,
    std::unique_ptr<MemoryPressureVoter> voter)
    : AvailableMemoryMonitorEvaluator(std::move(voter)),
      moderate_threshold_(moderate_threshold),
      critical_threshold_(critical_threshold) {
  CHECK_GE(moderate_threshold_, critical_threshold_);
  CHECK(critical_threshold_.is_positive());
}

// static
std::unique_ptr<FreeMemoryPressureEvaluator>
FreeMemoryPressureEvaluator::CreateWithMacFeatureParams(
    std::unique_ptr<MemoryPressureVoter> voter) {
  int moderate_mb = kMacFreeModerateThresholdMB.Get();
  int critical_mb = kMacFreeCriticalThresholdMB.Get();

  if (critical_mb <= 0 || moderate_mb < critical_mb) {
    DLOG(WARNING)
        << "Invalid Finch thresholds for Mac free memory pressure evaluator "
           "(Moderate: "
        << moderate_mb << " MiB, Critical: " << critical_mb
        << " MiB). Falling back to default thresholds.";
    return std::make_unique<FreeMemoryPressureEvaluator>(
        kDefaultModerateThreshold, kDefaultCriticalThreshold, std::move(voter));
  }

  return std::make_unique<FreeMemoryPressureEvaluator>(
      base::MiB(static_cast<uint64_t>(moderate_mb)),
      base::MiB(static_cast<uint64_t>(critical_mb)), std::move(voter));
}

FreeMemoryPressureEvaluator::~FreeMemoryPressureEvaluator() = default;

base::MemoryPressureLevel
FreeMemoryPressureEvaluator::CalculateCurrentPressureLevel(
    const base::AvailableMemoryMonitor::MemorySample& sample) {
  base::ByteSize free_phys = sample.free_physical_bytes;

  if (free_phys <= critical_threshold_) {
    return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }

  if (free_phys <= moderate_threshold_) {
    return base::MEMORY_PRESSURE_LEVEL_MODERATE;
  }

  return base::MEMORY_PRESSURE_LEVEL_NONE;
}

}  // namespace memory_pressure
