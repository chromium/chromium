// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_linux.h"

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_metrics.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

namespace {

constexpr int kKiBperMiB = 1024;

int GetAvailableSystemMemoryMiB(const base::SystemMemoryInfoKB& mem_info) {
  // Use 'available' metric if is is present,
  // if no (kernels < 3.14), let's make a rough evaluation using free physical
  // memory plus buffers and caches (that OS can free in case of low memory
  // state)
  int mem_available =
      mem_info.available ? mem_info.available
                         : (mem_info.free + mem_info.buffers + mem_info.cached);
  // How much physical memory is actively available for use right now, in MBs.
  return mem_available / kKiBperMiB;
}

}  // namespace

namespace memory_pressure {
namespace os_linux {

const base::TimeDelta SystemMemoryPressureEvaluator::kMemorySamplingPeriod =
    base::Seconds(5);

const base::TimeDelta SystemMemoryPressureEvaluator::kModeratePressureCooldown =
    base::Seconds(10);

const int SystemMemoryPressureEvaluator::kDefaultModerateThresholdPc = 75;
const int SystemMemoryPressureEvaluator::kDefaultCriticalThresholdPc = 85;

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      moderate_pressure_repeat_count_(0) {
  if (InferThresholds()) {
    StartObserving();
  }
}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    int moderate_threshold_mb,
    int critical_threshold_mb,
    std::unique_ptr<MemoryPressureVoter> voter)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      moderate_threshold_mb_(moderate_threshold_mb),
      critical_threshold_mb_(critical_threshold_mb),
      moderate_pressure_repeat_count_(0) {
  DCHECK_GE(moderate_threshold_mb_, critical_threshold_mb_);
  DCHECK_GT(critical_threshold_mb_, 0);
  StartObserving();
}

void SystemMemoryPressureEvaluator::StartObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Start(
      FROM_HERE, kMemorySamplingPeriod,
      base::BindRepeating(&SystemMemoryPressureEvaluator::CheckMemoryPressure,
                          base::Unretained(this)));
}

void SystemMemoryPressureEvaluator::StopObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If StartObserving failed, StopObserving will still get called.
  timer_.Stop();
}

bool SystemMemoryPressureEvaluator::GetSystemMemoryInfo(
    base::SystemMemoryInfoKB* mem_info) {
  return base::GetSystemMemoryInfo(mem_info);
}

void SystemMemoryPressureEvaluator::CheckMemoryPressure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get the previous pressure level and update the current one.
  MemoryPressureLevel old_vote = current_vote();
  SetCurrentVote(CalculateCurrentPressureLevel());

  // |notify| will be set to true if MemoryPressureListeners need to be
  // notified of a memory pressure level state change.
  bool notify = false;
  switch (current_vote()) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;

    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      if (old_vote != current_vote()) {
        // This is a new transition to moderate pressure so notify.
        moderate_pressure_repeat_count_ = 0;
        notify = true;
      } else {
        // Already in moderate pressure, only notify if sustained over the
        // cooldown period.
        const int kModeratePressureCooldownCycles =
            kModeratePressureCooldown / kMemorySamplingPeriod;
        if (++moderate_pressure_repeat_count_ ==
            kModeratePressureCooldownCycles) {
          moderate_pressure_repeat_count_ = 0;
          notify = true;
        }
      }
      break;

    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // Always notify of critical pressure levels.
      notify = true;
      break;
  }

  SendCurrentVote(notify);
}

bool SystemMemoryPressureEvaluator::InferThresholds() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SystemMemoryInfoKB mem_info;
  if (!GetSystemMemoryInfo(&mem_info)) {
    return false;
  }

  // The computation of the different thresholds assumes that
  // SystemMemoryInfoKB::total is stored as an integer and so the result of
  // |static_cast<uint64_t>(mem_info.total) * (100 - kThresholdPc)| won't
  // overflow.
  static_assert(
      std::is_same<decltype(mem_info.total), int>::value,
      "SystemMemoryInfoKB::total is expected to be stored as an integer.");
  critical_threshold_mb_ = base::checked_cast<int>(
      static_cast<uint64_t>(mem_info.total) *
      (100 - kDefaultCriticalThresholdPc) / 100 / kKiBperMiB);
  moderate_threshold_mb_ = base::checked_cast<int>(
      static_cast<uint64_t>(mem_info.total) *
      (100 - kDefaultModerateThresholdPc) / 100 / kKiBperMiB);
  return true;
}

base::MemoryPressureListener::MemoryPressureLevel
SystemMemoryPressureEvaluator::CalculateCurrentPressureLevel() {
  base::SystemMemoryInfoKB mem_info;
  if (GetSystemMemoryInfo(&mem_info)) {
    // How much system memory is actively available for use right now, in MBs.
    int available = GetAvailableSystemMemoryMiB(mem_info);

    // Determine if the available memory is under critical memory pressure.
    if (available <= critical_threshold_mb_) {
      return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
    }

    // Determine if the available memory is under moderate memory pressure.
    if (available <= moderate_threshold_mb_) {
      return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
    }
  }
  // No memory pressure was detected.
  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

}  // namespace os_linux
}  // namespace memory_pressure
