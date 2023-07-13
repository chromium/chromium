// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution_timeout_watchdog.h"

#include "base/metrics/histogram_functions.h"

namespace optimization_guide {

namespace {

void RecordDidTimeoutHistogram(proto::OptimizationTarget optimization_target,
                               bool did_timeout) {
  base::UmaHistogramBoolean(
      "OptimizationGuide.ModelExecutor.DidTimeout." +
          GetStringNameForOptimizationTarget(optimization_target),
      did_timeout);
}

}  // namespace

ModelExecutionTimeoutWatchdog::ModelExecutionTimeoutWatchdog(
    scoped_refptr<base::SequencedTaskRunner> watchdog_task_runner,
    proto::OptimizationTarget optimization_target,
    base::TimeDelta duration)
    : watchdog_task_runner_(watchdog_task_runner),
      optimization_target_(optimization_target),
      duration_(duration) {
  DCHECK_GE(duration, base::TimeDelta());
}

ModelExecutionTimeoutWatchdog::~ModelExecutionTimeoutWatchdog() {
  DCHECK(watchdog_task_runner_->RunsTasksInCurrentSequence());
}

void ModelExecutionTimeoutWatchdog::ArmWithTask(
    base::OnceClosure cancel_closure) {
  {
    base::AutoLock lock(cancel_lock_);
    cancel_closure_ = std::move(cancel_closure);
  }

  // Arm the watchdog timer. Since the dtor is on the watchdog sequence,
  // using base::Unretained is safe.
  watchdog_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ModelExecutionTimeoutWatchdog::ArmOnWatchdogSequence,
                     base::Unretained(this)));
}

void ModelExecutionTimeoutWatchdog::DisarmOnExecutionComplete() {
  {
    base::AutoLock lock(cancel_lock_);
    if (!cancel_closure_) {
      // This could happen in a race where Disarm was called at the same time
      // as the alarm going off.
      return;
    }
    cancel_closure_ = base::NullCallback();
  }

  // Disarm the watchdog timer. Since the dtor is on the watchdog sequence,
  // using base::Unretained is safe.
  watchdog_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ModelExecutionTimeoutWatchdog::DisarmOnWatchdogSequence,
                     base::Unretained(this)));
  RecordDidTimeoutHistogram(optimization_target_, false);
}

void ModelExecutionTimeoutWatchdog::ArmOnWatchdogSequence() {
  DCHECK(watchdog_task_runner_->RunsTasksInCurrentSequence());
  // Since the dtor is on the watchdog sequence, using base::Unretained is
  // safe. If the timer is released, the pending task will be canceled.
  watchdog_timer_.Start(
      FROM_HERE, duration_,
      base::BindOnce(&ModelExecutionTimeoutWatchdog::AlarmOnWatchdogSequence,
                     base::Unretained(this)));
}

void ModelExecutionTimeoutWatchdog::DisarmOnWatchdogSequence() {
  DCHECK(watchdog_task_runner_->RunsTasksInCurrentSequence());
  watchdog_timer_.Stop();
}

void ModelExecutionTimeoutWatchdog::AlarmOnWatchdogSequence() {
  DCHECK(watchdog_task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(cancel_lock_);
    if (!cancel_closure_) {
      // This could happen in a race where Disarm was called at the same time
      // as the alarm going off.
      return;
    }
    std::move(cancel_closure_).Run();
  }

  RecordDidTimeoutHistogram(optimization_target_, true);
}

}  // namespace optimization_guide
