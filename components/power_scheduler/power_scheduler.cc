// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_scheduler.h"

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/cpu_affinity_posix.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
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
  switch (affinity) {
    case base::CpuAffinityMode::kDefault:
      return "ApplyCpuAffinityModeDefault";
    case base::CpuAffinityMode::kLittleCoresOnly:
      return "ApplyCpuAffinityModeLittleCoresOnly";
  }
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

PowerScheduler::PowerScheduler() {
  DETACH_FROM_SEQUENCE(thread_pool_checker_);
}

PowerScheduler::~PowerScheduler() = default;

// static
PowerScheduler* PowerScheduler::GetInstance() {
  static base::NoDestructor<PowerScheduler> instance;
  return instance.get();
}

void PowerScheduler::WillProcessTask(const base::PendingTask& pending_task,
                                     bool was_blocked_or_low_priority) {}

void PowerScheduler::DidProcessTask(const base::PendingTask& pending_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_checker_);

  ++task_counter_;
  if (task_counter_ == kUpdateAfterEveryNTasks) {
    thread_pool_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PowerScheduler::EnforceCpuAffinityOnSequence,
                                  base::Unretained(this)  // never destroyed.
                                  ));
    task_counter_ = 0;
  }
}

void PowerScheduler::OnPowerModeChanged(power_scheduler::PowerMode old_mode,
                                        power_scheduler::PowerMode new_mode) {
  TRACE_EVENT2("power", "PowerScheduler::OnPowerModeChanged", "old_mode",
               power_scheduler::PowerModeToString(old_mode), "new_mode",
               power_scheduler::PowerModeToString(new_mode));

  OnPowerModeChangedOnSequence(old_mode, new_mode);
}

void PowerScheduler::Setup() {
  // The setup should be called once from the main thread. In single-process
  // mode, it may later be called on other threads (which should be ignored).
  if (did_call_setup_)
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_checker_);

  main_thread_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  thread_pool_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  did_call_setup_ = true;

  if (pending_policy_ == SchedulingPolicy::kNone)
    return;

  thread_pool_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PowerScheduler::SetupPolicyOnSequence,
                                base::Unretained(this),  // never destroyed.
                                pending_policy_));
  pending_policy_ = SchedulingPolicy::kNone;
}

void PowerScheduler::SetPolicy(SchedulingPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_checker_);

  if (!CpuAffinityApplicable())
    return;

  // Set up the power affinity observer and apply the policy if it's already
  // possible. Otherwise it will be set up after thread initialization via
  // Setup() (see app/content_main_runner_impl.cc and child/child_process.cc).
  if (thread_pool_task_runner_) {
    thread_pool_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PowerScheduler::SetupPolicyOnSequence,
                                  base::Unretained(this),  // never destroyed.
                                  policy));
  } else {
    pending_policy_ = policy;
  }
}

void PowerScheduler::SetupPolicyOnSequence(SchedulingPolicy policy) {
  DCHECK(power_scheduler::PowerModeArbiter::GetInstance());
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_checker_);

  // Reset the power mode in case it contains an obsolete value.
  current_power_mode_ = power_scheduler::PowerMode::kMaxValue;
  current_policy_ = policy;

  bool needs_power_observer =
      current_policy_ == SchedulingPolicy::kThrottleIdle ||
      current_policy_ == SchedulingPolicy::kThrottleIdleAndNopAnimation;

  if (needs_power_observer && !power_observer_registered_) {
    power_scheduler::PowerModeArbiter::GetInstance()->AddObserver(this);
    power_observer_registered_ = true;
  } else if (!needs_power_observer && power_observer_registered_) {
    power_scheduler::PowerModeArbiter::GetInstance()->RemoveObserver(this);
    power_observer_registered_ = false;
  }

  ApplyPolicyOnSequence();
}

void PowerScheduler::OnPowerModeChangedOnSequence(
    power_scheduler::PowerMode old_mode,
    power_scheduler::PowerMode new_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_checker_);

  current_power_mode_ = new_mode;
  ApplyPolicyOnSequence();
}

void PowerScheduler::ApplyPolicyOnSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_checker_);

  bool should_throttle = false;
  switch (current_policy_) {
    case SchedulingPolicy::kNone:
      break;
    case SchedulingPolicy::kLittleCoresOnly:
      should_throttle = true;
      break;
    case SchedulingPolicy::kThrottleIdle:
      should_throttle =
          current_power_mode_ == power_scheduler::PowerMode::kIdle ||
          current_power_mode_ == power_scheduler::PowerMode::kBackground;
      break;
    case SchedulingPolicy::kThrottleIdleAndNopAnimation:
      should_throttle =
          current_power_mode_ == power_scheduler::PowerMode::kIdle ||
          current_power_mode_ == power_scheduler::PowerMode::kBackground ||
          current_power_mode_ == power_scheduler::PowerMode::kNopAnimation;
      break;
  }

  auto new_affinity = should_throttle ? base::CpuAffinityMode::kLittleCoresOnly
                                      : base::CpuAffinityMode::kDefault;

  if (new_affinity != enforced_affinity_) {
    base::TimeTicks now = base::TimeTicks::Now();
    if (new_affinity == base::CpuAffinityMode::kDefault &&
        !enforced_affinity_setup_time_.is_null()) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Power.PowerScheduler.ThrottlingDuration",
                                 now - enforced_affinity_setup_time_,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10), 100);
    }
    enforced_affinity_setup_time_ = now;
    enforced_affinity_ = new_affinity;
    EnforceCpuAffinityOnSequence();
  }
}

void PowerScheduler::EnforceCpuAffinityOnSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_checker_);

  if (enforced_affinity_ != base::CurrentThreadCpuAffinityMode())
    ApplyProcessCpuAffinityMode(enforced_affinity_);

  // Android system may reset a non-default affinity setting, so we need to
  // check periodically if we need to re-apply it.
  bool mode_needs_periodic_enforcement =
      enforced_affinity_ != base::CpuAffinityMode::kDefault;

  if (mode_needs_periodic_enforcement && !task_observer_registered_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce([] {
          base::CurrentThread::Get()->AddTaskObserver(
              PowerScheduler::GetInstance());
        }));
    task_observer_registered_ = true;
  } else if (!mode_needs_periodic_enforcement && task_observer_registered_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce([] {
          base::CurrentThread::Get()->RemoveTaskObserver(
              PowerScheduler::GetInstance());
          PowerScheduler::GetInstance()->task_counter_ = 0;
        }));
    task_observer_registered_ = false;
  }
}

}  // namespace power_scheduler
