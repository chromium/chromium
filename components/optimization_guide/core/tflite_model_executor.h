// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TFLITE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TFLITE_MODEL_EXECUTOR_H_

#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/execution_status.h"
#include "components/optimization_guide/core/model_enums.h"
#include "components/optimization_guide/core/model_execution_timeout_watchdog.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"

namespace optimization_guide {

namespace {

// Util class for recording the result of the model execution. The result is
// recorded when it goes out of scope and its destructor is called.
class ScopedExecutionStatusResultRecorder {
 public:
  explicit ScopedExecutionStatusResultRecorder(
      proto::OptimizationTarget optimization_target)
      : optimization_target_(optimization_target) {}

  ~ScopedExecutionStatusResultRecorder() {
    base::UmaHistogramEnumeration(
        "OptimizationGuide.ModelExecutor.ExecutionStatus." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        status_);
  }

  ExecutionStatus* mutable_status() { return &status_; }

  ExecutionStatus status() const { return status_; }

  void set_status(ExecutionStatus status) { status_ = status; }

 private:
  // The OptimizationTarget of the model being executed.
  const proto::OptimizationTarget optimization_target_;

  ExecutionStatus status_ = ExecutionStatus::kUnknown;
};

}  // namespace

// An ModelExecutor that executes tflite models with arbitrary
// input and output types. Note that callers will need to give an implementation
// of this class to a |ModelHandler|, whereas the
// handle is the actual class that calling code would own and call into.
//
// By default, the model file will be (re)loaded for every execution and then
// unloaded from memory after every execution (e.g.: "OnComplete"). This helps
// to keep memory usage of the browser process down, but does delay model
// execution by the time it takes to load the model (about 50ms in practice).
// See |SetShouldUnloadModelOnComplete| to override this behavior.
template <class OutputType, class... InputTypes>
class TFLiteModelExecutor : public ModelExecutor<OutputType, InputTypes...> {
 public:
  TFLiteModelExecutor()
      : watchdog_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}
  ~TFLiteModelExecutor() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // Should be called on the same sequence as the ctor, but once called |this|
  // must only be used from the |execution_task_runner| thread/sequence.
  void InitializeAndMoveToExecutionThread(
      absl::optional<base::TimeDelta> model_inference_timeout,
      proto::OptimizationTarget optimization_target,
      scoped_refptr<base::SequencedTaskRunner> execution_task_runner,
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner) override {
    DCHECK(!execution_task_runner_);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_NE(optimization_target,
              proto::OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN);

    DETACH_FROM_SEQUENCE(sequence_checker_);
    optimization_target_ = optimization_target;
    execution_task_runner_ = execution_task_runner;
    reply_task_runner_ = reply_task_runner;
    if (features::IsModelExecutionWatchdogEnabled()) {
      // The sequence |watchdog_sequence| is used to run watchdog's task. The
      // watchdog must be deleted on that sequence to guarantee that pending
      // tasks can safely be executed.
      scoped_refptr<base::SequencedTaskRunner> watchdog_sequence =
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
      using WatchdogType =
          ModelExecutionTimeoutWatchdog<OutputType, InputTypes...>;
      watchdog_ = std::unique_ptr<WatchdogType, base::OnTaskRunnerDeleter>(
          new WatchdogType(
              watchdog_sequence, optimization_target_,
              model_inference_timeout.value_or(
                  features::ModelExecutionWatchdogDefaultTimeout())),
          base::OnTaskRunnerDeleter(watchdog_sequence));
    }
  }

  // Called when a model file is available to load. Immediately loads model into
  // memory when `should_unload_model_on_complete_` is false.
  void UpdateModelFile(const base::FilePath& file_path) override {
    DCHECK(execution_task_runner_ &&
           execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    UnloadModel();

    model_file_path_ = file_path;

    // crbug/1257189: Histogram enums can't use dynamically created histogram
    // names, so factory create the local histogram (used in testing).
    base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
        "OptimizationGuide.ModelExecutor.ModelFileUpdated." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        base::Histogram::kNoFlags);
    histogram->Add(true);

    if (should_unload_model_on_complete_) {
      ExecutionStatus out_status;
      LoadModelFile(&out_status);
    }
  }

  // Calling this method allows the default model loading/unloading behavior to
  // be overridden. Setting this to true will cause the model to remain loaded
  // afterwards a model execution (e.g.: "OnComplete"), until |UnloadModel| is
  // called. False is the default behavior (see class comment).
  void SetShouldUnloadModelOnComplete(
      bool should_unload_model_on_complete) override {
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    should_unload_model_on_complete_ = should_unload_model_on_complete;
  }

  // Clears the loaded model from memory if it is loaded. Safe to call when the
  // model is already unloaded, and becomes a no-op.
  void UnloadModel() override {
    TRACE_EVENT1("browser", "OptGuideModelExecutor::UnloadModel",
                 "OptimizationTarget",
                 optimization_guide::GetStringNameForOptimizationTarget(
                     optimization_target_));
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    loaded_model_.reset();
    model_fb_.reset();
  }

  // Starts the execution of the model. When complete, |callback_on_complete|
  // will be run via |reply_task_runner_| with the output of the model.
  using ExecutionCallback =
      base::OnceCallback<void(const absl::optional<OutputType>&)>;
  void SendForExecution(ExecutionCallback callback_on_complete,
                        base::TimeTicks start_time,
                        InputTypes... args) override {
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(reply_task_runner_);

    base::TimeDelta task_scheduling_latency =
        base::TimeTicks::Now() - start_time;
    base::UmaHistogramMediumTimes(
        "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        task_scheduling_latency);

    ScopedExecutionStatusResultRecorder status_recorder(optimization_target_);

    // Attempt to load the model file if it isn't loaded yet, fail if loading is
    // unsuccessful or no model is available to load.
    if (!loaded_model_ && !LoadModelFile(status_recorder.mutable_status())) {
      reply_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback_on_complete), absl::nullopt));
      // Some error status is expected, and derived classes should have set the
      // status.
      DCHECK_NE(status_recorder.status(), ExecutionStatus::kUnknown);
      DCHECK_NE(status_recorder.status(), ExecutionStatus::kSuccess);
      return;
    }

    if (last_execution_time_) {
      // The max of this histogram is 3m since only the distribution and count
      // of smaller values is important.
      base::UmaHistogramMediumTimes(
          "OptimizationGuide.ModelExecutor.TimeSincePreviousRun." +
              GetStringNameForOptimizationTarget(optimization_target_),
          base::TimeTicks::Now() - *last_execution_time_);
    }
    last_execution_time_ = base::TimeTicks::Now();

    DCHECK(loaded_model_);
    absl::optional<OutputType> output;

    // IMPORTANT: Once the arm method is called, disarm must be called when the
    // model execution finishes. Do NOT early-return in this next block.
    if (watchdog_) {
      watchdog_->ArmWithTask(loaded_model_.get());
    }
    {
      TRACE_EVENT1("browser", "OptGuideModelExecutor::Execute",
                   "OptimizationTarget",
                   optimization_guide::GetStringNameForOptimizationTarget(
                       optimization_target_));
      base::ElapsedThreadTimer execution_timer;
      base::TimeTicks execute_start_time = base::TimeTicks::Now();
      output = Execute(loaded_model_.get(), status_recorder.mutable_status(),
                       args...);
      DCHECK_NE(status_recorder.status(), ExecutionStatus::kUnknown);

      // The max of this histogram is 1 hour because we want to understand
      // tail behavior and catch long running model executions.
      base::UmaHistogramLongTimes(
          "OptimizationGuide.ModelExecutor.ExecutionLatency." +
              GetStringNameForOptimizationTarget(optimization_target_),
          base::TimeTicks::Now() - execute_start_time);
      base::UmaHistogramLongTimes(
          "OptimizationGuide.ModelExecutor.ExecutionThreadTime." +
              GetStringNameForOptimizationTarget(optimization_target_),
          execution_timer.Elapsed());
    }
    if (watchdog_) {
      watchdog_->DisarmOnExecutionComplete();
    }

    DCHECK(callback_on_complete);
    reply_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_on_complete), output));

    OnExecutionComplete();
  }

  TFLiteModelExecutor(const TFLiteModelExecutor&) = delete;
  TFLiteModelExecutor& operator=(const TFLiteModelExecutor&) = delete;

 protected:
  using ModelExecutionTask =
      tflite::task::core::BaseTaskApi<OutputType, InputTypes...>;

  // Executes the model using |execution_task| on |args|, returning the model
  // output and setting |out_status| with the status of the execution attempt.
  virtual absl::optional<OutputType> Execute(ModelExecutionTask* execution_task,
                                             ExecutionStatus* out_status,
                                             InputTypes... args) = 0;

  // Builds a model execution task using |model_file|.
  virtual std::unique_ptr<ModelExecutionTask> BuildModelExecutionTask(
      base::MemoryMappedFile* model_file,
      ExecutionStatus* out_status) = 0;

 private:
  // A true return value indicates the model was loaded successfully, false
  // otherwise.
  bool LoadModelFile(ExecutionStatus* out_status) {
    TRACE_EVENT1("browser", "OptGuideModelExecutor::LoadModelFile",
                 "OptimizationTarget",
                 optimization_guide::GetStringNameForOptimizationTarget(
                     optimization_target_));
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    UnloadModel();

    base::UmaHistogramBoolean(
        "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
            GetStringNameForOptimizationTarget(optimization_target_),
        !!model_file_path_);

    if (!model_file_path_) {
      *out_status = ExecutionStatus::kErrorModelFileNotAvailable;
      return false;
    }

    base::TimeTicks loading_start_time = base::TimeTicks::Now();

    std::unique_ptr<base::MemoryMappedFile> model_fb =
        std::make_unique<base::MemoryMappedFile>();
    if (!model_fb->Initialize(*model_file_path_)) {
      *out_status = ExecutionStatus::kErrorModelFileNotValid;
      return false;
    }
    model_fb_ = std::move(model_fb);

    loaded_model_ = BuildModelExecutionTask(model_fb_.get(), out_status);

    if (!!loaded_model_) {
      // We only want to record successful loading times.
      base::UmaHistogramTimes(
          "OptimizationGuide.ModelExecutor.ModelLoadingDuration2." +
              optimization_guide::GetStringNameForOptimizationTarget(
                  optimization_target_),
          base::TimeTicks::Now() - loading_start_time);
    }

    // Local histogram used in integration testing.
    base::BooleanHistogram::FactoryGet(
        "OptimizationGuide.ModelExecutor.ModelLoadedSuccessfully." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        base::Histogram::kNoFlags)
        ->Add(!!loaded_model_);

    return !!loaded_model_;
  }

  void OnExecutionComplete() {
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (should_unload_model_on_complete_) {
      UnloadModel();
    }
  }

  proto::OptimizationTarget optimization_target_ =
      proto::OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN;

  bool should_unload_model_on_complete_ = true;

  std::unique_ptr<ModelExecutionTimeoutWatchdog<OutputType, InputTypes...>,
                  base::OnTaskRunnerDeleter>
      watchdog_;

  scoped_refptr<base::SequencedTaskRunner> execution_task_runner_;

  scoped_refptr<base::SequencedTaskRunner> reply_task_runner_;

  // The time that the model was last executed. Logged in metrics for the second
  // and following runs.
  absl::optional<base::TimeTicks> last_execution_time_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The model file path to be loaded. May be nullopt if no model has been
  // downloaded yet.
  absl::optional<base::FilePath> model_file_path_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Note on lifetimes: |loaded_model_| and |model_fb_| both share the same
  // lifetime, being set in |LoadModelFile()| and being destroyed in
  // |ResetModelFile()|.

  std::unique_ptr<ModelExecutionTask> loaded_model_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // This will only be non-null when |model_file_path_| is set, and while the
  // model is loaded which is managed by a feature flag.
  std::unique_ptr<base::MemoryMappedFile> model_fb_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TFLITE_MODEL_EXECUTOR_H_
