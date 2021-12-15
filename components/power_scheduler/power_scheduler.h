// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_SCHEDULER_POWER_SCHEDULER_H_
#define COMPONENTS_POWER_SCHEDULER_POWER_SCHEDULER_H_

#include "base/component_export.h"
#include "base/cpu_affinity_posix.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/process/process_metrics.h"
#include "base/task/task_observer.h"
#include "components/power_scheduler/power_mode_arbiter.h"

namespace power_scheduler {

enum class SchedulingPolicy {
  kNone,
  kLittleCoresOnly,
  kThrottleIdle,
  kThrottleIdleAndNopAnimation,
};

struct COMPONENT_EXPORT(POWER_SCHEDULER) SchedulingPolicyParams {
  SchedulingPolicy policy = SchedulingPolicy::kNone;

  // If specified, the policy's throttling only takes effect after at least
  // |min_time_in_mode| has elapsed since entering a throttleable PowerMode.
  base::TimeDelta min_time_in_mode;

  // If specified, the policy's throttling only takes effect if the process has
  // spent at least (min_cputime_ratio * elapsed time) CPU time since entering
  // the throttleable mode.
  double min_cputime_ratio = 0;
};

class COMPONENT_EXPORT(POWER_SCHEDULER) PowerScheduler
    : public base::TaskObserver,
      public power_scheduler::PowerModeArbiter::Observer {
 public:
  static PowerScheduler* GetInstance();

  PowerScheduler(const PowerScheduler&) = delete;
  PowerScheduler& operator=(const PowerScheduler&) = delete;

  // base::TaskObserver implementation.
  void WillProcessTask(const base::PendingTask&,
                       bool was_blocked_or_low_priority) override;
  void DidProcessTask(const base::PendingTask&) override;

  // power_scheduler::PowerModeArbiter::Observer implementation.
  void OnPowerModeChanged(power_scheduler::PowerMode old_mode,
                          power_scheduler::PowerMode new_mode) override;

  // Set up the power observer if required by current policy.
  // This function should be called from the main thread during the
  // initialization of the process. Subsequent calls from other threads will
  // have no effect.
  void Setup();

  // Set the scheduling policy based on the feature list and field trial
  // parameters. Does not have any effect if the PowerScheduler features are
  // disabled. Should be called on the process's main thread during process
  // startup after feature list initialization.
  void InitializePolicyFromFeatureList();

  // Set the scheduling policy for the current process. Should be called on the
  // process's main thread during process startup after feature list
  // initialization. The affinity might change at runtime (e.g. after Chrome
  // goes back from background), so the power scheduler will set up a polling
  // mechanism to enforce the given mode.
  void SetPolicy(SchedulingPolicy);
  void SetPolicy(SchedulingPolicyParams);

  // Protected methods for testing.
 protected:
  PowerScheduler(PowerModeArbiter*);
  ~PowerScheduler() override;

  void SetupTaskRunners(
      scoped_refptr<base::TaskRunner> thread_pool_task_runner);

  base::CpuAffinityMode GetEnforcedCpuAffinityForTesting() {
    return enforced_affinity_;
  }

  // Virtual for testing.
  virtual base::TimeDelta GetProcessCpuTime();

  base::CpuAffinityMode GetTargetCpuAffinity();

  // Set the CPU affinity of the current process and set up the polling
  // mechanism to enforce the affinity mode. The check is implemented as a
  // TaskObserver that runs every 100th main thread task.
  //
  // Virtual for testing.
  virtual void EnforceCpuAffinityOnSequence();

  SchedulingPolicyParams GetPolicyForTesting() { return current_policy_; }

 private:
  friend class base::NoDestructor<PowerScheduler>;

  // Register the power mode observer and apply the current policy if necessary.
  void SetupPolicyOnSequence(SchedulingPolicyParams);

  void OnPowerModeChangedOnSequence(power_scheduler::PowerMode old_mode,
                                    power_scheduler::PowerMode new_mode);

  // Apply CPU affinity settings according to current policy and power mode.
  void ApplyPolicyOnSequence();

  SEQUENCE_CHECKER(main_thread_checker_);
  SEQUENCE_CHECKER(thread_pool_checker_);

  scoped_refptr<base::TaskRunner> main_thread_task_runner_;
  scoped_refptr<base::TaskRunner> thread_pool_task_runner_;

  // Accessed only on the main thread.
  static constexpr int kUpdateAfterEveryNTasks = 100;
  int task_counter_ = 0;
  bool did_call_setup_ = false;
  SchedulingPolicyParams pending_policy_;

  // Accessed only on the |thread_pool_task_runner_| sequence.
  raw_ptr<PowerModeArbiter> arbiter_;
  bool power_observer_registered_ = false;
  bool task_observer_registered_ = false;
  base::CpuAffinityMode enforced_affinity_ = base::CpuAffinityMode::kDefault;
  base::TimeTicks enforced_affinity_setup_time_;
  power_scheduler::PowerMode current_power_mode_ =
      power_scheduler::PowerMode::kMaxValue;
  SchedulingPolicyParams current_policy_;
  base::TimeTicks time_entered_throttleable_mode_;
  base::TimeDelta cputime_entered_throttleable_mode_;
  std::unique_ptr<base::ProcessMetrics> process_metrics_;
};

}  // namespace power_scheduler

#endif  // COMPONENTS_POWER_SCHEDULER_POWER_SCHEDULER_H_
