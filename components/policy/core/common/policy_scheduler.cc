// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_scheduler.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace policy {

PolicyScheduler::PolicyScheduler(Task task,
                                 SchedulerCallback callback,
                                 base::TimeDelta interval)
    : task_(task), callback_(callback), interval_(interval) {
  ScheduleTaskNow();
}

PolicyScheduler::~PolicyScheduler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PolicyScheduler::ScheduleTaskNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleDelayedTask(base::TimeDelta());
}

void PolicyScheduler::ScheduleDelayedTask(base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (job_) {
    job_->Cancel();
  }
  job_ = std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
      &PolicyScheduler::RunScheduledTask, weak_ptr_factory_.GetWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE,
                                                       job_->callback(), delay);
}

void PolicyScheduler::ScheduleNextTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeDelta interval = overlap_ ? base::TimeDelta() : interval_;
  const base::TimeTicks now(base::TimeTicks::Now());
  // Time uses saturated arithmetics thus no under/overflow possible.
  const base::TimeDelta delay = last_task_ + interval - now;
  // Clamping delay to non-negative values just to be on the safe side.
  ScheduleDelayedTask(std::max(base::TimeDelta(), delay));
}

void PolicyScheduler::RunScheduledTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (task_in_progress_) {
    overlap_ = true;
    return;
  }

  overlap_ = false;
  task_in_progress_ = true;
  task_.Run(base::BindOnce(&PolicyScheduler::OnTaskDone,
                           weak_ptr_factory_.GetWeakPtr()));
}

void PolicyScheduler::OnTaskDone(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_in_progress_ = false;
  last_task_ = base::TimeTicks::Now();
  callback_.Run(success);
  ScheduleNextTask();
}

}  // namespace policy
