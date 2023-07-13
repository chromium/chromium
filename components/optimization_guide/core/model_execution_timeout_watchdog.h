// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TIMEOUT_WATCHDOG_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TIMEOUT_WATCHDOG_H_

#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// This is a helper class to |TFLiteModelExecutor| that watches for a model
// execution that runs for too long.
//
// Background/Motivation: TFLite Model Execution occurs on a background task
// runner, but we've seen from metrics that some models are extremely long
// running (example: 8s at 95 %ile) which is simply not tolerable, and should
// instead be cancelled outright after a reasonable duration.
//
// Notes:
// * The TFLite Library guarantees that their Cancel() method can be called from
// any thread.
// * This class is owned by TFLiteModelExecutor and lives on a background
// SequencedTaskRunner.
// * The base class' methods are privately inherited because we must (re)set
// |cancel_closure_| correctly with logic in this class, and not have the caller
// worry about it.
//
// Synchronization: This class takes a raw pointer to the TFLite Model Runner,
// which is created and destroyed frequently on the background task runner.
// Therefore |cancel_closure_| is protected by a lock since it is set on one
// thread (the background sequence that executes the model), but maybe used on
// the watchdog's thread to cancel a long running execution.
// Care must be taken to ensure |ArmWithTask| and |DisarmOnExecutionComplete|
// are always called so that the internal |cancel_closure_| can be (re)set with
// the same timing as the model execution.
//
// The watchdog class is working on two sequences: execution sequence and
// wachdog sequence. |ArmWithTask| and |DisarmOnExecutionComplete| are called
// on the execution sequence. The watchdog is implemented with a
// base::OneShotTimer which lives and runs on the watchdog sequence. In case of
// an execution timeout, the watchdog notification is received by
// |AlarmOnWatchdogSequence| on the watchdog sequence. The deletion of this
// class must happen on the watchdog sequence to ensure that timer task can be
// cancelled on the right sequence.
class ModelExecutionTimeoutWatchdog {
 public:
  explicit ModelExecutionTimeoutWatchdog(
      scoped_refptr<base::SequencedTaskRunner> watchdog_task_runner,
      proto::OptimizationTarget optimization_target,
      base::TimeDelta duration);
  ~ModelExecutionTimeoutWatchdog();

  void ArmWithTask(base::OnceClosure cancel_closure);
  void DisarmOnExecutionComplete();

 private:
  void ArmOnWatchdogSequence();
  void DisarmOnWatchdogSequence();
  void AlarmOnWatchdogSequence();

  scoped_refptr<base::SequencedTaskRunner> watchdog_task_runner_;
  base::OneShotTimer watchdog_timer_;

  const proto::OptimizationTarget optimization_target_;
  const base::TimeDelta duration_;

  base::Lock cancel_lock_;
  base::OnceClosure cancel_closure_ GUARDED_BY(cancel_lock_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TIMEOUT_WATCHDOG_H_
