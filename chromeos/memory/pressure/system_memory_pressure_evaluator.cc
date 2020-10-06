// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/pressure/system_memory_pressure_evaluator.h"

#include <fcntl.h>
#include <sys/poll.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/memory/pressure/pressure.h"

namespace chromeos {
namespace memory {

namespace {
// Pointer to the SystemMemoryPressureEvaluator used by TabManagerDelegate for
// chromeos to need to call into ScheduleEarlyCheck.
SystemMemoryPressureEvaluator* g_system_evaluator = nullptr;

// We try not to re-notify on moderate too frequently, this time
// controls how frequently we will notify after our first notification.
constexpr base::TimeDelta kModerateMemoryPressureCooldownTime =
    base::TimeDelta::FromSeconds(10);

// Converts an available memory value in MB to a memory pressure level.
base::MemoryPressureListener::MemoryPressureLevel
GetMemoryPressureLevelFromAvailable(uint64_t available_mb,
                                    uint64_t moderate_avail_mb,
                                    uint64_t critical_avail_mb) {
  if (available_mb < critical_avail_mb)
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  if (available_mb < moderate_avail_mb)
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;

  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

}  // namespace

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    std::unique_ptr<util::MemoryPressureVoter> voter)
    : SystemMemoryPressureEvaluator(
          /*disable_timer_for_testing*/ false,
          std::move(voter)) {}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    bool disable_timer_for_testing,
    std::unique_ptr<util::MemoryPressureVoter> voter)
    : util::SystemMemoryPressureEvaluator(std::move(voter)),
      weak_ptr_factory_(this) {
  DCHECK(g_system_evaluator == nullptr);
  g_system_evaluator = this;

  std::pair<uint64_t, uint64_t> margins_kb =
      chromeos::memory::pressure::GetMemoryMarginsKB();
  critical_pressure_threshold_mb_ = margins_kb.first / 1024;
  moderate_pressure_threshold_mb_ = margins_kb.second / 1024;

  chromeos::memory::pressure::UpdateMemoryParameters();

  if (!disable_timer_for_testing) {
    // We will check the memory pressure and report the metric
    // (ChromeOS.MemoryPressureLevel) every 1 second.
    checking_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(1),
        base::BindRepeating(&SystemMemoryPressureEvaluator::
                                CheckMemoryPressureAndRecordStatistics,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}
SystemMemoryPressureEvaluator::~SystemMemoryPressureEvaluator() {
  DCHECK(g_system_evaluator);
  g_system_evaluator = nullptr;
}

// static
SystemMemoryPressureEvaluator* SystemMemoryPressureEvaluator::Get() {
  return g_system_evaluator;
}

// CheckMemoryPressure will get the current memory pressure level by checking
// the available memory.
void SystemMemoryPressureEvaluator::CheckMemoryPressure() {
  uint64_t mem_avail_mb =
      chromeos::memory::pressure::GetAvailableMemoryKB() / 1024;
  CheckMemoryPressureImpl(moderate_pressure_threshold_mb_,
                          critical_pressure_threshold_mb_, mem_avail_mb);
}

void SystemMemoryPressureEvaluator::CheckMemoryPressureImpl(
    uint64_t moderate_avail_mb,
    uint64_t critical_avail_mb,
    uint64_t mem_avail_mb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto old_vote = current_vote();

  SetCurrentVote(GetMemoryPressureLevelFromAvailable(
      mem_avail_mb, moderate_avail_mb, critical_avail_mb));
  bool notify = true;

  if (current_vote() ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    last_moderate_notification_ = base::TimeTicks();
    notify = false;
  } else if (current_vote() ==
             base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
    // In the case of MODERATE memory pressure we may be in this state for quite
    // some time so we limit the rate at which we dispatch notifications.
    if (old_vote == current_vote()) {
      if (base::TimeTicks::Now() - last_moderate_notification_ <
          kModerateMemoryPressureCooldownTime) {
        notify = false;
      } else if (old_vote ==
                 base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
        // Reset the moderate notification time if we just crossed back.
        last_moderate_notification_ = base::TimeTicks::Now();
        notify = false;
      }
    }

    if (notify)
      last_moderate_notification_ = base::TimeTicks::Now();
  }

  VLOG(1) << "SystemMemoryPressureEvaluator::CheckMemoryPressure dispatching "
             "at level: "
          << current_vote();
  SendCurrentVote(notify);
}

void SystemMemoryPressureEvaluator::CheckMemoryPressureAndRecordStatistics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: If we support notifications of memory pressure changes in both
  // directions we will not have to update the cached value as it will always
  // be correct.
  CheckMemoryPressure();

  // Record UMA histogram statistics for the current memory pressure level, it
  // would seem that only Memory.PressureLevel would be necessary.
  constexpr int kNumberPressureLevels = 3;
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.MemoryPressureLevel", current_vote(),
                            kNumberPressureLevels);
}

void SystemMemoryPressureEvaluator::ScheduleEarlyCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SystemMemoryPressureEvaluator::CheckMemoryPressure,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace memory
}  // namespace chromeos
