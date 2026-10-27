// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_THRESHOLD_MEMORY_PRESSURE_EVALUATOR_H_
#define COMPONENTS_MEMORY_PRESSURE_THRESHOLD_MEMORY_PRESSURE_EVALUATOR_H_

#include <memory>

#include "base/byte_size.h"
#include "base/feature_list.h"
#include "base/memory/available_memory_monitor.h"
#include "base/memory/memory_pressure_level.h"
#include "build/build_config.h"
#include "components/memory_pressure/available_memory_monitor_evaluator.h"

namespace memory_pressure {

class MemoryPressureVoter;

#if BUILDFLAG(IS_WIN)
namespace features {
BASE_DECLARE_FEATURE(kCommitAvailableMemoryPressureThresholds);
}  // namespace features
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
namespace features {
// Controls whether macOS uses physical available memory byte thresholds
// instead of native OS DISPATCH_SOURCE_TYPE_MEMORYPRESSURE notifications.
BASE_DECLARE_FEATURE(kMacAvailableMemoryPressureEvaluator);
}  // namespace features
#endif  // BUILDFLAG(IS_MAC)

// Memory pressure evaluator that observes AvailableMemoryMonitor to check
// available physical memory against thresholds.
class ThresholdMemoryPressureEvaluator
    : public AvailableMemoryMonitorEvaluator {
 public:
  // Constructor with explicit memory thresholds. These represent the amount of
  // available physical memory below which the applicable memory pressure state
  // engages. Platforms provide their thresholds through dedicated factories.
  ThresholdMemoryPressureEvaluator(base::ByteSize moderate_threshold,
                                   base::ByteSize critical_threshold,
                                   std::unique_ptr<MemoryPressureVoter> voter);

#if BUILDFLAG(IS_WIN)
  // Default Windows thresholds, derived from paging behavior observations
  // (~300MB available memory paging limit).
  static constexpr base::ByteSize kDefaultWinModerateThreshold =
      base::MiB(1000);
  static constexpr base::ByteSize kDefaultWinCriticalThreshold = base::MiB(400);

  // Static factory method initializing thresholds with the Windows defaults.
  static std::unique_ptr<ThresholdMemoryPressureEvaluator>
  CreateWithWinDefaults(std::unique_ptr<MemoryPressureVoter> voter);
#endif

#if BUILDFLAG(IS_MAC)
  // Static factory method initializing thresholds from macOS feature
  // parameters.
  static std::unique_ptr<ThresholdMemoryPressureEvaluator>
  CreateWithMacFeatureParams(std::unique_ptr<MemoryPressureVoter> voter);
#endif

  ~ThresholdMemoryPressureEvaluator() override;

  ThresholdMemoryPressureEvaluator(const ThresholdMemoryPressureEvaluator&) =
      delete;
  ThresholdMemoryPressureEvaluator& operator=(
      const ThresholdMemoryPressureEvaluator&) = delete;

  // Returns the moderate pressure level available physical memory threshold.
  base::ByteSize moderate_threshold() const { return moderate_threshold_; }

  // Returns the critical pressure level available physical memory threshold.
  base::ByteSize critical_threshold() const { return critical_threshold_; }

 protected:
  // Calculates the current instantaneous memory pressure level for a given
  // sample based on available physical memory (and commit limits on Windows
  // when kCommitAvailableMemoryPressureThresholds is enabled).
  base::MemoryPressureLevel CalculateCurrentPressureLevel(
      const base::AvailableMemoryMonitor::MemorySample& sample) override;

#if BUILDFLAG(IS_WIN)
  static void RecordCommitHistograms(
      const base::AvailableMemoryMonitor::MemorySample& sample);
#endif

 private:
  const base::ByteSize moderate_threshold_;
  const base::ByteSize critical_threshold_;
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_THRESHOLD_MEMORY_PRESSURE_EVALUATOR_H_
