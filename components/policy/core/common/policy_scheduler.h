// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_SCHEDULER_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_SCHEDULER_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/policy/policy_export.h"

namespace policy {

// Scheduler for driving repeated asynchronous tasks such as e.g. policy
// fetches. Subsequent tasks are guaranteed not to overlap. Tasks are posted to
// the current thread and therefore must not block (suitable e.g. for
// asynchronous D-Bus calls).
// Tasks scheduling begins immediately after instantiation of the class. Upon
// destruction, scheduled but not yet started tasks are cancelled. The result of
// started but not finished tasks is NOT reported.
class POLICY_EXPORT PolicyScheduler {
 public:
  // Callback for the task to report success or failure.
  using TaskCallback = base::OnceCallback<void(bool success)>;

  // Task to be performed at regular intervals. The task takes a |callback| to
  // return success or failure.
  using Task = base::RepeatingCallback<void(TaskCallback callback)>;

  // Callback for PolicyScheduler to report success or failure of the tasks.
  using SchedulerCallback = base::RepeatingCallback<void(bool success)>;

  // Defines the |task| to be run every |interval| and the |callback| for the
  // scheduler to report the result. (Intervals are computed as the time
  // difference between the end of the previous and the start of the subsequent
  // task.) Calling the constructor starts the loop and schedules the first task
  // to be run without delay.
  PolicyScheduler(Task task,
                  SchedulerCallback callback,
                  base::TimeDelta interval);
  PolicyScheduler(const PolicyScheduler&) = delete;
  PolicyScheduler& operator=(const PolicyScheduler&) = delete;
  ~PolicyScheduler();

  // Schedules a task to run immediately. Deletes any previously scheduled but
  // not yet started tasks. In case a task is running currently, the new task is
  // scheduled to run immediately after the end of the currently running task.
  void ScheduleTaskNow();

  base::TimeDelta interval() const { return interval_; }

  base::Time last_refresh_attempt() const { return last_refresh_attempt_; }

 private:
  // Schedules next task to run in |delay|. Deletes any previously scheduled
  // tasks.
  void ScheduleDelayedTask(base::TimeDelta delay);

  // Schedules next task to run in |interval_| or immediately in case of
  // overlap. Deletes any previously scheduled tasks.
  void ScheduleNextTask();

  // Actually executes the scheduled task.
  void RunScheduledTask();

  // Reports back the |result| of the previous task and schedules the next one.
  void OnTaskDone(bool result);

  Task task_;
  SchedulerCallback callback_;
  // Tasks are being run every |interval_|.
  const base::TimeDelta interval_;

  // Whether a task is in progress.
  bool task_in_progress_ = false;

  // Whether there had been an overlap of tasks and thus the next task needs to
  // be scheduled without delay.
  bool overlap_ = false;

  // End time of the previous task. Zero in case no task has ended yet.
  base::TimeTicks last_task_;

  // Last time refresh has been attempted.
  base::Time last_refresh_attempt_;

  std::unique_ptr<base::CancelableOnceClosure> job_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last member.
  base::WeakPtrFactory<PolicyScheduler> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_SCHEDULER_H_
