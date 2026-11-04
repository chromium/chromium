// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/compressor_memory_pressure_evaluator.h"

#include <utility>

#include "base/byte_size.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "components/memory_pressure/memory_pressure_voter.h"

namespace memory_pressure {

namespace features {
BASE_FEATURE(kMacCompressorMemoryPressureEvaluator,
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

namespace {

BASE_FEATURE_PARAM(
    int,
    kMacCompressorCriticalThresholdPercent,
    &features::kMacCompressorMemoryPressureEvaluator,
    CompressorMemoryPressureEvaluator::kDefaultCriticalThresholdPercent);

}  // namespace

CompressorMemoryPressureEvaluator::CompressorMemoryPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : CompressorMemoryPressureEvaluator(kDefaultCriticalThresholdPercent,
                                        std::move(voter)) {}

CompressorMemoryPressureEvaluator::CompressorMemoryPressureEvaluator(
    int critical_threshold_percent,
    std::unique_ptr<MemoryPressureVoter> voter)
    : AvailableMemoryMonitorEvaluator(std::move(voter)),
      critical_threshold_percent_(critical_threshold_percent) {
  CHECK_GT(critical_threshold_percent_, 0);
  CHECK_LE(critical_threshold_percent_, 100);
}

// static
std::unique_ptr<CompressorMemoryPressureEvaluator>
CompressorMemoryPressureEvaluator::CreateWithMacFeatureParams(
    std::unique_ptr<MemoryPressureVoter> voter) {
  int critical_percent = kMacCompressorCriticalThresholdPercent.Get();

  if (critical_percent <= 0 || critical_percent > 100) {
    DLOG(WARNING)
        << "Invalid Finch critical percentage threshold for Mac compressor "
           "memory pressure evaluator ("
        << critical_percent << "%). Falling back to default threshold ("
        << kDefaultCriticalThresholdPercent << "%).";
    critical_percent = kDefaultCriticalThresholdPercent;
  }

  return std::make_unique<CompressorMemoryPressureEvaluator>(critical_percent,
                                                             std::move(voter));
}

CompressorMemoryPressureEvaluator::~CompressorMemoryPressureEvaluator() =
    default;

base::MemoryPressureLevel
CompressorMemoryPressureEvaluator::CalculateCurrentPressureLevel(
    const base::AvailableMemoryMonitor::MemorySample& sample) {
  if (!sample.total_physical_bytes.is_positive() ||
      sample.total_physical_bytes <= sample.wired_bytes) {
    return base::MEMORY_PRESSURE_LEVEL_NONE;
  }

  base::ByteSize unwired_bytes = base::ByteSize::FromByteSizeDelta(
      sample.total_physical_bytes - sample.wired_bytes);

  if (sample.compressor_bytes * 100 >=
      unwired_bytes * critical_threshold_percent_) {
    return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }

  return base::MEMORY_PRESSURE_LEVEL_NONE;
}

}  // namespace memory_pressure
