// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_FREE_MEMORY_PRESSURE_EVALUATOR_H_
#define COMPONENTS_MEMORY_PRESSURE_FREE_MEMORY_PRESSURE_EVALUATOR_H_

#include <memory>

#include "base/byte_size.h"
#include "base/feature_list.h"
#include "base/memory/available_memory_monitor.h"
#include "base/memory/memory_pressure_level.h"
#include "components/memory_pressure/available_memory_monitor_evaluator.h"

namespace memory_pressure {

class MemoryPressureVoter;

namespace features {
// Controls whether macOS uses strictly unallocated free RAM byte thresholds
// to intercept acute memory exhaustion before reaching the kernel reserve
// floor.
BASE_DECLARE_FEATURE(kMacFreeMemoryPressureEvaluator);
}  // namespace features

// Memory pressure evaluator that observes AvailableMemoryMonitor to check
// strictly unallocated free physical memory (free_count - speculative_count)
// against byte thresholds.
class FreeMemoryPressureEvaluator : public AvailableMemoryMonitorEvaluator {
 public:
  // Default physical memory thresholds for raw unallocated free RAM on macOS.
  // The Mach kernel maintains a hard reserve floor below which allocations
  // block and heavy swap pageouts erupt. Defaulting to 150 MiB Moderate /
  // 50 MiB Critical should sit safely above this reserve floor, catching
  // acute allocation spikes before the system enters thrashing or OOM states.
  static constexpr base::ByteSize kDefaultModerateThreshold = base::MiB(150);
  static constexpr base::ByteSize kDefaultCriticalThreshold = base::MiB(50);

  // Default constructor. Uses default physical free memory thresholds.
  explicit FreeMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);

  // Constructor with explicit memory thresholds. These represent the amount of
  // unallocated free physical memory below which the applicable memory pressure
  // state engages. Primarily used for testing or specialized platform tuning.
  FreeMemoryPressureEvaluator(base::ByteSize moderate_threshold,
                              base::ByteSize critical_threshold,
                              std::unique_ptr<MemoryPressureVoter> voter);

  // Static factory method initializing thresholds from macOS feature
  // parameters.
  static std::unique_ptr<FreeMemoryPressureEvaluator>
  CreateWithMacFeatureParams(std::unique_ptr<MemoryPressureVoter> voter);

  ~FreeMemoryPressureEvaluator() override;

  FreeMemoryPressureEvaluator(const FreeMemoryPressureEvaluator&) = delete;
  FreeMemoryPressureEvaluator& operator=(const FreeMemoryPressureEvaluator&) =
      delete;

  // Returns the moderate pressure level unallocated free memory threshold.
  base::ByteSize moderate_threshold() const { return moderate_threshold_; }

  // Returns the critical pressure level unallocated free memory threshold.
  base::ByteSize critical_threshold() const { return critical_threshold_; }

 protected:
  // Calculates the current instantaneous memory pressure level based on
  // unallocated free physical memory.
  base::MemoryPressureLevel CalculateCurrentPressureLevel(
      const base::AvailableMemoryMonitor::MemorySample& sample) override;

 private:
  // Threshold amounts of unallocated free memory.
  const base::ByteSize moderate_threshold_;
  const base::ByteSize critical_threshold_;
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_FREE_MEMORY_PRESSURE_EVALUATOR_H_
