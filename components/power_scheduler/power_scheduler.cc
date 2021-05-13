// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_scheduler.h"

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/cpu_affinity_posix.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/typed_macros.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with base::CpuAffinityMode and CpuAffinityMode in enums.xml.
enum class CpuAffinityModeForUma {
  kDefault = 0,
  kLittleCoresOnly = 1,
  kMaxValue = kLittleCoresOnly,
};

CpuAffinityModeForUma GetCpuAffinityModeForUma(base::CpuAffinityMode affinity) {
  switch (affinity) {
    case base::CpuAffinityMode::kDefault:
      return CpuAffinityModeForUma::kDefault;
    case base::CpuAffinityMode::kLittleCoresOnly:
      return CpuAffinityModeForUma::kLittleCoresOnly;
  }
}

perfetto::StaticString TraceEventNameForAffinityMode(
    base::CpuAffinityMode affinity) {
  if (affinity == base::CpuAffinityMode::kDefault) {
    return "ApplyCpuAffinityModeDefault";
  } else if (affinity == base::CpuAffinityMode::kLittleCoresOnly) {
    return "ApplyCpuAffinityModeLittleCoresOnly";
  }
  return "ApplyCpuAffinityModeUnknown";
}

void ApplyProcessCpuAffinityMode(base::CpuAffinityMode affinity) {
  TRACE_EVENT("power", TraceEventNameForAffinityMode(affinity));

  // Restrict affinity of all existing threads of the current process. The
  // affinity is inherited by any subsequently created thread. Other threads may
  // already exist even during early startup (e.g. Java threads like
  // RenderThread), so setting the affinity only for the current thread is not
  // enough here.
  bool success = base::SetProcessCpuAffinityMode(
      base::GetCurrentProcessHandle(), affinity);

  base::UmaHistogramBoolean(
      "Power.CpuAffinityExperiments.ProcessAffinityUpdateSuccess", success);
  if (success) {
    base::UmaHistogramEnumeration(
        "Power.CpuAffinityExperiments.ProcessAffinityMode",
        GetCpuAffinityModeForUma(affinity));
  }
}

bool CpuAffinityApplicable() {
  // For now, affinity modes only have an effect on big.LITTLE architectures.
  return base::HasBigCpuCores();
}

}  // anonymous namespace

namespace power_scheduler {

PowerScheduler::PowerScheduler() = default;
PowerScheduler::~PowerScheduler() = default;

// static
PowerScheduler* PowerScheduler::GetInstance() {
  static base::NoDestructor<PowerScheduler> instance;
  return instance.get();
}

void PowerScheduler::WillProcessTask(const base::PendingTask& pending_task,
                                     bool was_blocked_or_low_priority) {}

void PowerScheduler::DidProcessTask(const base::PendingTask& pending_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ++task_counter_;
  if (task_counter_ == kUpdateAfterEveryNTasks) {
    EnforceCpuAffinity();
    task_counter_ = 0;
  }
}

void PowerScheduler::OnPowerModeChanged(power_scheduler::PowerMode old_mode,
                                        power_scheduler::PowerMode new_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_power_mode_ = new_mode;

  ApplyPolicy();
}

void PowerScheduler::Setup() {
  // The setup should be called once from the main thread. Subsequent calls
  // from other threads should be ignored.
  if (did_call_setup_)
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetupPolicy();
  did_call_setup_ = true;
}

void PowerScheduler::SetPolicy(SchedulingPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CpuAffinityApplicable())
    return;

  current_policy_ = policy;

  // Set up the power affinity observer and apply the policy if it's already
  // possible. Otherwise it will be set up after thread initialization via
  // Setup() (see app/content_main_runner_impl.cc and child/child_process.cc).
  if (base::CurrentThread::IsSet()) {
    SetupPolicy();
  }
}

void PowerScheduler::SetupPolicy() {
  DCHECK(power_scheduler::PowerModeArbiter::GetInstance());
  DCHECK(base::CurrentThread::IsSet());

  // Reset the power mode in case it contains an obsolete value.
  current_power_mode_ = power_scheduler::PowerMode::kMaxValue;

  if (current_policy_ == SchedulingPolicy::kThrottleIdle &&
      !power_observer_registered_) {
    power_scheduler::PowerModeArbiter::GetInstance()->AddObserver(this);
    power_observer_registered_ = true;
  } else if (current_policy_ != SchedulingPolicy::kThrottleIdle &&
             power_observer_registered_) {
    power_scheduler::PowerModeArbiter::GetInstance()->RemoveObserver(this);
    power_observer_registered_ = false;
  }

  ApplyPolicy();
}

void PowerScheduler::ApplyPolicy() {
  auto new_affinity = base::CpuAffinityMode::kDefault;
  if (current_policy_ == SchedulingPolicy::kLittleCoresOnly ||
      (current_policy_ == SchedulingPolicy::kThrottleIdle &&
       (current_power_mode_ == power_scheduler::PowerMode::kIdle ||
        current_power_mode_ == power_scheduler::PowerMode::kBackground))) {
    new_affinity = base::CpuAffinityMode::kLittleCoresOnly;
  }

  if (new_affinity != enforced_affinity_) {
    enforced_affinity_ = new_affinity;
    EnforceCpuAffinity();
  }
}

void PowerScheduler::EnforceCpuAffinity() {
  if (enforced_affinity_ == base::CpuAffinityMode::kLittleCoresOnly &&
      !task_observer_registered_) {
    base::CurrentThread::Get()->AddTaskObserver(this);
    task_observer_registered_ = true;
  } else if (enforced_affinity_ == base::CpuAffinityMode::kDefault &&
             task_observer_registered_) {
    // We don't have to enforce the default affinity.
    base::CurrentThread::Get()->RemoveTaskObserver(this);
    task_observer_registered_ = false;
  }

  if (enforced_affinity_ != base::CurrentThreadCpuAffinityMode())
    ApplyProcessCpuAffinityMode(enforced_affinity_);
}

}  // namespace power_scheduler
