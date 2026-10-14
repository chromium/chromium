// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_AVAILABLE_MEMORY_MONITOR_EVALUATOR_H_
#define COMPONENTS_MEMORY_PRESSURE_AVAILABLE_MEMORY_MONITOR_EVALUATOR_H_

#include <memory>

#include "base/memory/available_memory_monitor.h"
#include "base/memory/memory_pressure_level.h"
#include "base/sequence_checker.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

namespace memory_pressure {

class MemoryPressureVoter;

// Base class for memory pressure evaluators that observe
// base::AvailableMemoryMonitor to calculate memory pressure levels from
// periodically sampled system memory metrics.
//
// Centralizes observer lifecycle management and sequence enforcement. Derived
// classes implement CalculateCurrentPressureLevel to determine the pressure
// level from the memory sample according to their specific metrics and
// thresholds.
class AvailableMemoryMonitorEvaluator
    : public SystemMemoryPressureEvaluator,
      public base::AvailableMemoryMonitor::Observer {
 public:
  explicit AvailableMemoryMonitorEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);

  ~AvailableMemoryMonitorEvaluator() override;

  AvailableMemoryMonitorEvaluator(const AvailableMemoryMonitorEvaluator&) =
      delete;
  AvailableMemoryMonitorEvaluator& operator=(
      const AvailableMemoryMonitorEvaluator&) = delete;

  // base::AvailableMemoryMonitor::Observer implementation:
  void OnAvailableMemoryUpdated(
      const base::AvailableMemoryMonitor::MemorySample& sample) override;

 protected:
  // Starts observing the memory monitor.
  void StartObserving();

  // Stops observing the memory monitor.
  void StopObserving();

  // Checks memory pressure, storing the current level and casting a vote to the
  // aggregator. Must be called from the same sequence on which the monitor was
  // instantiated.
  void CheckMemoryPressure(
      const base::AvailableMemoryMonitor::MemorySample& sample);

  // Calculates the current pressure level for a given memory sample. Must be
  // implemented by derived classes.
  virtual base::MemoryPressureLevel CalculateCurrentPressureLevel(
      const base::AvailableMemoryMonitor::MemorySample& sample) = 0;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_AVAILABLE_MEMORY_MONITOR_EVALUATOR_H_
