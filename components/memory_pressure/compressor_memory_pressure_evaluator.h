// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_COMPRESSOR_MEMORY_PRESSURE_EVALUATOR_H_
#define COMPONENTS_MEMORY_PRESSURE_COMPRESSOR_MEMORY_PRESSURE_EVALUATOR_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/available_memory_monitor.h"
#include "base/memory/memory_pressure_level.h"
#include "components/memory_pressure/available_memory_monitor_evaluator.h"

namespace memory_pressure {

class MemoryPressureVoter;

namespace features {
// Controls whether macOS uses an in-RAM compressor saturation percentage
// threshold to detect memory pressure before disk swap-thrashing begins.
BASE_DECLARE_FEATURE(kMacCompressorMemoryPressureEvaluator);
}  // namespace features

// Memory pressure evaluator that observes AvailableMemoryMonitor to monitor
// the size of macOS kernel-compressed RAM as a percentage of compressible
// (unwired) physical RAM (total_physical_bytes - wired_bytes).
//
// macOS absorbs heavy memory allocations into an in-RAM compressor pool before
// spilling over to active swap thrashing. This evaluator casts a critical
// memory pressure vote when the compressed memory footprint saturates a
// calibrated percentage ceiling of compressible physical RAM (discounting wired
// memory). Moderate pressure is left as a no-op because baseline memory
// compression is healthy and normal under macOS memory management.
class CompressorMemoryPressureEvaluator
    : public AvailableMemoryMonitorEvaluator {
 public:
  // Default critical memory pressure threshold expressed as a percentage of
  // unwired physical RAM (45%).
  static constexpr int kDefaultCriticalThresholdPercent = 45;

  // Default constructor. Uses default compressor saturation percentage.
  explicit CompressorMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);

  // Constructor with explicit critical percentage threshold (1 to 100).
  // Primarily used for testing or specialized platform tuning.
  CompressorMemoryPressureEvaluator(int critical_threshold_percent,
                                    std::unique_ptr<MemoryPressureVoter> voter);

  // Static factory method initializing the threshold from macOS feature
  // parameters.
  static std::unique_ptr<CompressorMemoryPressureEvaluator>
  CreateWithMacFeatureParams(std::unique_ptr<MemoryPressureVoter> voter);

  ~CompressorMemoryPressureEvaluator() override;

  CompressorMemoryPressureEvaluator(const CompressorMemoryPressureEvaluator&) =
      delete;
  CompressorMemoryPressureEvaluator& operator=(
      const CompressorMemoryPressureEvaluator&) = delete;

  // Returns the critical pressure level percentage threshold.
  int critical_threshold_percent() const { return critical_threshold_percent_; }

 protected:
  // Calculates the current pressure level for a given memory sample based on
  // compressor saturation.
  base::MemoryPressureLevel CalculateCurrentPressureLevel(
      const base::AvailableMemoryMonitor::MemorySample& sample) override;

 private:
  // Critical threshold as a percentage of unwired physical RAM (1 - 100).
  const int critical_threshold_percent_;
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_COMPRESSOR_MEMORY_PRESSURE_EVALUATOR_H_
