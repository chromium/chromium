// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_WIN_H_
#define COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_WIN_H_

#include "base/byte_count.h"
#include "base/functional/callback_forward.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/scoped_handle.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

// To not pull in windows.h.
typedef struct _MEMORYSTATUSEX MEMORYSTATUSEX;

namespace memory_pressure::win {

// Windows memory pressure voter that checks the amount of RAM left at a low
// frequency and applies internal hysteresis.
class SystemMemoryPressureEvaluator
    : public memory_pressure::SystemMemoryPressureEvaluator {
 public:
  // The memory sampling period, currently 5s.
  static constexpr base::TimeDelta kDefaultPeriod = base::Seconds(5);

  // Constants governing the polling and hysteresis behaviour of the observer.
  // The time which should pass between 2 successive moderate memory pressure
  // signals, in milliseconds. The value has been lifted from similar values in
  // the ChromeOS memory pressure monitor. The values were determined
  // experimentally to ensure sufficient responsiveness of the memory pressure
  // subsystem, and minimal overhead.
  static constexpr base::TimeDelta kModeratePressureCooldown =
      base::Seconds(10);

  // Available physical memory threshold to dispatch moderate/critical memory
  // pressure. Many years ago, we observed that Windows maintains ~300MB of
  // available memory, paging until that is the case (this may not be accurate
  // at the time of writing this). Therefore, we consider that there is critical
  // memory pressure when approaching this amount of available memory.
  static constexpr base::ByteCount kPhysicalMemoryDefaultModerateThreshold =
      base::MiB(1000);
  static constexpr base::ByteCount kPhysicalMemoryDefaultCriticalThreshold =
      base::MiB(400);

  // Default constructor. Will choose thresholds automatically based on the
  // actual amount of system memory.
  explicit SystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);

  // Constructor with explicit memory thresholds. These represent the amount of
  // free memory below which the applicable memory pressure state engages.
  // For testing purposes.
  SystemMemoryPressureEvaluator(base::ByteCount moderate_threshold,
                                base::ByteCount critical_threshold,
                                std::unique_ptr<MemoryPressureVoter> voter);

  ~SystemMemoryPressureEvaluator() override;

  SystemMemoryPressureEvaluator(const SystemMemoryPressureEvaluator&) = delete;
  SystemMemoryPressureEvaluator& operator=(
      const SystemMemoryPressureEvaluator&) = delete;

  // Returns the moderate pressure level free memory threshold.
  base::ByteCount moderate_threshold() const { return moderate_threshold_; }

  // Returns the critical pressure level free memory threshold.
  base::ByteCount critical_threshold() const { return critical_threshold_; }

 protected:
  // Internals are exposed for unittests.

  // Starts observing the memory fill level. Calls to StartObserving should
  // always be matched with calls to StopObserving.
  void StartObserving();

  // Stop observing the memory fill level. May be safely called if
  // StartObserving has not been called. Must be called from the same thread on
  // which the monitor was instantiated.
  void StopObserving();

  // Checks memory pressure, storing the current level, applying any hysteresis
  // and emitting memory pressure level change signals as necessary. This
  // function is called periodically while the monitor is observing memory
  // pressure. Must be called from the same thread on which the monitor was
  // instantiated.
  void CheckMemoryPressure();

  // Calculates the current instantaneous memory pressure level. This does not
  // use any hysteresis and simply returns the result at the current moment. Can
  // be called on any thread.
  base::MemoryPressureLevel CalculateCurrentPressureLevel();

  // Gets system memory status. This is virtual as a unittesting hook. Returns
  // true if the system call succeeds, false otherwise. Can be called on any
  // thread.
  virtual bool GetSystemMemoryStatus(MEMORYSTATUSEX& mem_status);

  // Records histograms about committed memory based on `mem_status`.
  static void RecordCommitHistograms(const MEMORYSTATUSEX& mem_status);

 private:
  // Threshold amounts of available memory that trigger pressure levels. See
  // memory_pressure_monitor.cc for a discussion of reasonable values for these.
  const base::ByteCount moderate_threshold_;
  const base::ByteCount critical_threshold_;

  // A periodic timer to check for memory pressure changes.
  base::RepeatingTimer timer_;

  // To slow down the amount of moderate pressure event calls, this gets used to
  // count the number of events since the last event occurred. This is used by
  // |CheckMemoryPressure| to apply hysteresis on the raw results of
  // |CalculateCurrentPressureLevel|.
  int moderate_pressure_repeat_count_;

  // Ensures that this object is used from a single sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace memory_pressure::win

#endif  // COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_WIN_H_
