// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_WIN_H_
#define COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_WIN_H_

#include "base/byte_size.h"
#include "base/functional/callback_forward.h"
#include "base/memory/available_memory_monitor.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/sequence_checker.h"
#include "base/win/scoped_handle.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

namespace memory_pressure::win {

// Windows memory pressure voter that observes AvailableMemoryMonitor to check
// the amount of RAM left.
class SystemMemoryPressureEvaluator
    : public memory_pressure::SystemMemoryPressureEvaluator,
      public base::AvailableMemoryMonitor::Observer {
 public:
  // Available physical memory threshold to dispatch moderate/critical memory
  // pressure. Many years ago, we observed that Windows maintains ~300MB of
  // available memory, paging until that is the case (this may not be accurate
  // at the time of writing this). Therefore, we consider that there is critical
  // memory pressure when approaching this amount of available memory.
  static constexpr base::ByteSize kPhysicalMemoryDefaultModerateThreshold =
      base::MiB(1000);
  static constexpr base::ByteSize kPhysicalMemoryDefaultCriticalThreshold =
      base::MiB(400);

  // Default constructor. Will choose thresholds automatically based on the
  // actual amount of system memory.
  explicit SystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);

  // Constructor with explicit memory thresholds. These represent the amount of
  // free memory below which the applicable memory pressure state engages.
  // For testing purposes.
  SystemMemoryPressureEvaluator(base::ByteSize moderate_threshold,
                                base::ByteSize critical_threshold,
                                std::unique_ptr<MemoryPressureVoter> voter);

  ~SystemMemoryPressureEvaluator() override;

  SystemMemoryPressureEvaluator(const SystemMemoryPressureEvaluator&) = delete;
  SystemMemoryPressureEvaluator& operator=(
      const SystemMemoryPressureEvaluator&) = delete;

  // base::AvailableMemoryMonitor::Observer implementation:
  void OnAvailableMemoryUpdated(
      const base::AvailableMemoryMonitor::MemorySample& sample) override;

  // Returns the moderate pressure level free memory threshold.
  base::ByteSize moderate_threshold() const { return moderate_threshold_; }

  // Returns the critical pressure level free memory threshold.
  base::ByteSize critical_threshold() const { return critical_threshold_; }

 protected:
  // Internals are exposed for unittests.

  // Starts observing the memory fill level. Calls to StartObserving should
  // always be matched with calls to StopObserving.
  void StartObserving();

  // Stop observing the memory fill level. May be safely called if
  // StartObserving has not been called. Must be called from the same thread on
  // which the monitor was instantiated.
  void StopObserving();

  // Checks memory pressure, storing the current level and casting a vote to the
  // aggregator. Must be called from the same thread on which the monitor was
  // instantiated.
  void CheckMemoryPressure(
      const base::AvailableMemoryMonitor::MemorySample& sample);

  // Calculates the current instantaneous memory pressure level. Can be called
  // on any thread.
  base::MemoryPressureLevel CalculateCurrentPressureLevel(
      const base::AvailableMemoryMonitor::MemorySample& sample);

  // Records histograms about committed memory based on `sample`.
  static void RecordCommitHistograms(
      const base::AvailableMemoryMonitor::MemorySample& sample);

 private:
  // Threshold amounts of available memory that trigger pressure levels. See
  // memory_pressure_monitor.cc for a discussion of reasonable values for these.
  const base::ByteSize moderate_threshold_;
  const base::ByteSize critical_threshold_;

  // Ensures that this object is used from a single sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace memory_pressure::win

#endif  // COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_WIN_H_
