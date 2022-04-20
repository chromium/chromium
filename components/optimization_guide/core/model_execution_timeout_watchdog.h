// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TIMEOUT_WATCHDOG_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TIMEOUT_WATCHDOG_H_

#include "base/metrics/histogram_functions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/watchdog.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"

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

// This is a helper class to |TFLiteModelExecutor| that watches for a model
// execution that runs for too long. This is done using a |base::Watchdog| that
// uses PlatformThread under the hood.
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
// * The base class' methods are privately inheirited because we must (re)set
// the |task_| pointer correctly with logic in this class, and not have the
// caller worry about it.
//
// Synchronization: This class takes a raw pointer to the TFLite Model Runner,
// which is created and destroyed frequently on the background task runner.
// Therefore the |task_| pointer is protected by a lock since it is set on one
// thread (the background sequence that executes the model), but maybe used on
// the watchdog's thread to cancel a long running execution.
// Care must be taken to ensure |ArmWithTask| and |DisarmOnExecutionComplete|
// are always called so that the internal |task_| pointer can be (re)set with
// the same timing as the model execution.
template <class OutputType, class... InputTypes>
class ModelExecutionTimeoutWatchdog : private base::Watchdog {
 public:
  explicit ModelExecutionTimeoutWatchdog(
      proto::OptimizationTarget optimization_target,
      base::TimeDelta duration)
      : base::Watchdog(
            duration,
            /*thread_watched_name=*/"OptGuideModelExecution_" +
                GetStringNameForOptimizationTarget(optimization_target),
            /*enabled=*/true),
        optimization_target_(optimization_target) {}

  void ArmWithTask(
      tflite::task::core::BaseTaskApi<OutputType, InputTypes...>* task) {
    {
      base::AutoLock lock(task_lock_);
      task_ = task;
    }
    Arm();
  }

  void DisarmOnExecutionComplete() {
    {
      base::AutoLock lock(task_lock_);
      if (!task_) {
        // This could happen in a race where Disarm was called at the same time
        // as the alarm going off.
        return;
      }
      task_ = nullptr;
    }
    Disarm();

    RecordDidTimeoutHistogram(optimization_target_, false);
  }

 private:
  // base::Watchdog:
  void Alarm() override {
    base::Watchdog::Alarm();

    {
      base::AutoLock lock(task_lock_);
      if (!task_) {
        // This could happen in a race where Disarm was called at the same time
        // as the alarm going off.
        return;
      }
      task_->Cancel();
      task_ = nullptr;
    }

    RecordDidTimeoutHistogram(optimization_target_, true);
  }

  const proto::OptimizationTarget optimization_target_;

  base::Lock task_lock_;
  raw_ptr<tflite::task::core::BaseTaskApi<OutputType, InputTypes...>> task_
      GUARDED_BY(task_lock_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TIMEOUT_WATCHDOG_H_