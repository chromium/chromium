// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TFLITE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TFLITE_MODEL_EXECUTOR_H_

#include <optional>

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
#include "base/types/expected.h"
#include "components/optimization_guide/core/execution_status.h"
#include "components/optimization_guide/core/model_enums.h"
#include "components/optimization_guide/core/model_execution_timeout_watchdog.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
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
//
// Note that when built with the MediaPipe backend (non-default), task
// cancellation is not supported.
template <class OutputType,
          class InputType,
          // TODO(b/283522287): Remove this once all usage of TFLite Task
          // Support are replaced by MediaPipe.
          class ModelExecutionTaskType =
              tflite::task::core::BaseTaskApi<OutputType, InputType>>
class TFLiteModelExecutor : public ModelExecutor<OutputType, InputType> {
 public:
  TFLiteModelExecutor()
      : watchdog_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

  ~TFLiteModelExecutor() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Unload the model. Do not use `UnloadModel` since it may be overridden by
    // a subclass and hence not available from this destructor.
    model_fb_.reset();
  }

  // Should be called on the same sequence as the ctor, but once called |this|
  // must only be used from the |execution_task_runner| thread/sequence.
  void InitializeAndMoveToExecutionThread(
      std::optional<base::TimeDelta> model_inference_timeout,
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
    model_loading_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

    if (features::IsModelExecutionWatchdogEnabled()) {
      // The sequence |watchdog_sequence| is used to run watchdog's task. The
      // watchdog must be deleted on that sequence to guarantee that pending
      // tasks can safely be executed.
      scoped_refptr<base::SequencedTaskRunner> watchdog_sequence =
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
      watchdog_ = std::unique_ptr<ModelExecutionTimeoutWatchdog,
                                  base::OnTaskRunnerDeleter>(
          new ModelExecutionTimeoutWatchdog(
              watchdog_sequence, optimization_target_,
              model_inference_timeout.value_or(
                  features::ModelExecutionWatchdogDefaultTimeout())),
          base::OnTaskRunnerDeleter(watchdog_sequence));
    }
  }

  // Called when a model file is available to load. Immediately loads model into
  // memory when `should_preload_model_` is set.
  void UpdateModelFile(
      base::optional_ref<const base::FilePath> file_path) override {
    DCHECK(execution_task_runner_ &&
           execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    UnloadModel();
    DCHECK(!loaded_model_);
    DCHECK(!model_fb_);

    // The model has been removed.
    if (!file_path.has_value()) {
      model_file_path_.reset();
      return;
    }
    model_file_path_ = *file_path;

    // crbug/1257189: Histogram enums can't use dynamically created histogram
    // names, so factory create the local histogram (used in testing).
    base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
        "OptimizationGuide.ModelExecutor.ModelFileUpdated." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        base::Histogram::kNoFlags);
    histogram->Add(true);

    if (should_preload_model_) {
      LoadModelFile(base::DoNothing());
    }
  }

  // Calling this method allows the default model loading/unloading behavior to
  // be overridden. Setting this to false will cause the model to remain loaded
  // afterwards a model execution (e.g.: "OnComplete"), until |UnloadModel| is
  // called. False is the default behavior (see class comment).
  //
  // Note that keeping the model in memory for a long duration may be detected
  // as a memory leak in Chrome, and will always increase the private or shared
  // memory used by the browser by the size of the model file and the
  // constructed TFLite graph.
  void SetShouldUnloadModelOnComplete(
      bool should_unload_model_on_complete) override {
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    should_unload_model_on_complete_ = should_unload_model_on_complete;
  }

  // Calling this method allows the default model preloading behavior to
  // be overridden. Setting this to true will cause the model to be loaded as
  // soon as its file path is available. Callers may also need to call
  // `SetShouldUnloadModelOnComplete(true)` to keep the model in memory for the
  // lifetime of the entire browsing session.
  //
  // Note that keeping the model in memory for a long duration may be detected
  // as a memory leak in Chrome, and will always increase the private or shared
  // memory used by the browser by the size of the model file and the
  // constructed TFLite graph.
  void SetShouldPreloadModel(bool should_preload_model) override {
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    should_preload_model_ = should_preload_model;
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

  using ExecutionCallback =
      base::OnceCallback<void(const std::optional<OutputType>&)>;
  using BatchExecutionCallback =
      base::OnceCallback<void(const std::vector<std::optional<OutputType>>&)>;

  // When complete, |callback_on_complete| will be run via |reply_task_runner_|
  // with the outputs of the model.
  void SendForExecution(ExecutionCallback callback_on_complete,
                        base::TimeTicks start_time,
                        InputType input) override {
    BatchExecutionCallback adapted_callback = base::BindOnce(
        [](ExecutionCallback callback,
           const std::vector<std::optional<OutputType>>& output) {
          CHECK_EQ(output.size(), 1U);
          std::move(callback).Run(output[0]);
        },
        std::move(callback_on_complete));
    SendForBatchExecution(std::move(adapted_callback), start_time, {input});
  }

  // Starts the batch execution of the model. When complete,
  // |callback_on_complete| will be run via |reply_task_runner_| with the
  // outputs of the model.
  void SendForBatchExecution(
      BatchExecutionCallback callback_on_complete,
      base::TimeTicks start_time,
      ModelExecutor<OutputType, InputType>::ConstRefInputVector inputs)
      override {
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

    // Load the model file in the background thread if not loaded yet, and
    // then batch execute the loaded model on the execution thread.
    LoadModelFileAndBatchExecute(std::move(callback_on_complete), inputs);
  }

  // Starts the synchronous execution of the model. Returns model outputs.
  // Model needs to be loaded. Synchronous calls do not load or unload model.
  std::vector<std::optional<OutputType>> SendForBatchExecutionSync(
      ModelExecutor<OutputType, InputType>::ConstRefInputVector inputs)
      override {
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    std::vector<std::optional<OutputType>> outputs;
    outputs.reserve(inputs.size());
    // If the model isn't loaded yet, return null results.
    if (!loaded_model_) {
      for (size_t i = 0; i < inputs.size(); i++) {
        outputs.push_back(std::nullopt);
        // If the model is not loaded in a batch context, this status would not
        // get recorded the same number of times as it would in success. Thus,
        // increment the bucket |inputs.size()| number of times to keep metrics
        // sane.
        ScopedExecutionStatusResultRecorder status_recorder(
            optimization_target_);
        status_recorder.set_status(
            ExecutionStatus::kErrorModelFileNotAvailable);
      }
      return outputs;
    }

    BatchExecuteLoadedModel(inputs, &outputs);
    OnExecutionComplete();
    return outputs;
  }

  // IMPORTANT: These WeakPointers must only be dereferenced on the
  // |execution_task_runner| thread.
  base::WeakPtr<TFLiteModelExecutor> GetWeakPtrForExecutionThread() {
    return execution_sequence_weak_ptr_factory_.GetWeakPtr();
  }

  TFLiteModelExecutor(const TFLiteModelExecutor&) = delete;
  TFLiteModelExecutor& operator=(const TFLiteModelExecutor&) = delete;

 protected:
  using ModelExecutionTask =
      tflite::task::core::BaseTaskApi<OutputType, InputType>;

  // Executes the model using |execution_task| on |args|, returning the model
  // output and setting |out_status| with the status of the execution attempt.
  virtual std::optional<OutputType> Execute(
      ModelExecutionTaskType* execution_task,
      ExecutionStatus* out_status,
      InputType args) = 0;

  // Builds a model execution task using |model_file|. On error, the returned
  // `ExecutionStatus` will never be `ExecutionStatus::kSuccess`.
  virtual base::expected<std::unique_ptr<ModelExecutionTaskType>,
                         ExecutionStatus>
  BuildModelExecutionTask(base::MemoryMappedFile* model_file) = 0;

 private:
  using MemoryMappedFileDeleteOnTaskRunner =
      std::unique_ptr<base::MemoryMappedFile, base::OnTaskRunnerDeleter>;

  static MemoryMappedFileDeleteOnTaskRunner
  NullMemoryMappedFileDeleteOnTaskRunner() {
    return {nullptr, base::OnTaskRunnerDeleter(nullptr)};
  }

  // Loads the model file in the background thread, and calls a callback on
  // model file loaded in memory on the model execution thread.
  void LoadModelFile(
      base::OnceCallback<void(ExecutionStatus)> model_loaded_callback) {
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

    // TODO(b/298673103): Multiple calls to LoadModelFile may trigger this
    // PostTask multiple times.

    // Run the slower model loading file I/O task on the background thread to
    // avoid blocking the main thread, e.g., the UI thread.
    model_loading_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        // Anomynous model file loading function to be called on the background
        // thread, which returns the memory-mapped model file or nullptr if
        // failed to load.
        base::BindOnce(
            [](const std::optional<base::FilePath> model_file_path,
               proto::OptimizationTarget optimization_target,
               scoped_refptr<base::SequencedTaskRunner>
                   model_loading_task_runner)
                -> std::pair<ExecutionStatus,
                             MemoryMappedFileDeleteOnTaskRunner> {
              base::TimeTicks loading_start_time = base::TimeTicks::Now();
              if (!model_file_path) {
                return std::make_pair(
                    ExecutionStatus::kErrorModelFileNotAvailable,
                    NullMemoryMappedFileDeleteOnTaskRunner());
              }

              MemoryMappedFileDeleteOnTaskRunner model_fb(
                  new base::MemoryMappedFile(),
                  base::OnTaskRunnerDeleter(
                      std::move(model_loading_task_runner)));
              if (!model_fb->Initialize(*model_file_path)) {
                return std::make_pair(ExecutionStatus::kErrorModelFileNotValid,
                                      NullMemoryMappedFileDeleteOnTaskRunner());
              }

              // We only want to record successful loading times.
              base::UmaHistogramTimes(
                  "OptimizationGuide.ModelExecutor.ModelLoadingDuration2." +
                      optimization_guide::GetStringNameForOptimizationTarget(
                          optimization_target),
                  base::TimeTicks::Now() - loading_start_time);

              return std::make_pair(ExecutionStatus::kSuccess,
                                    std::move(model_fb));
            },
            model_file_path_, optimization_target_, model_loading_task_runner_),
        base::BindOnce(&TFLiteModelExecutor::OnModelFileLoadedInMemory,
                       GetWeakPtrForExecutionThread(),
                       std::move(model_loaded_callback)));
  }

  // Called on model file loaded in memory. Builds the model execution task from
  // the memory-mapped file, and calls `model_loaded_callback`.
  void OnModelFileLoadedInMemory(
      base::OnceCallback<void(ExecutionStatus)> model_loaded_callback,
      std::pair<ExecutionStatus, MemoryMappedFileDeleteOnTaskRunner>
          status_and_model_fb) {
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // If |model_fb_| is going to be replaced below, it needs to be deleted on a
    // blockable thread.
    UnloadModel();

    ExecutionStatus file_load_status = status_and_model_fb.first;
    model_fb_ = std::move(status_and_model_fb.second);
    if (!model_fb_) {
      std::move(model_loaded_callback).Run(file_load_status);
      return;
    }

    auto build_result = BuildModelExecutionTask(model_fb_.get());
    if (build_result.has_value()) {
      loaded_model_ = std::move(build_result.value());
    }

    // Local histogram used in integration testing.
    base::BooleanHistogram::FactoryGet(
        "OptimizationGuide.ModelExecutor.ModelLoadedSuccessfully." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        base::Histogram::kNoFlags)
        ->Add(!!loaded_model_);

    std::move(model_loaded_callback)
        .Run(build_result.error_or(ExecutionStatus::kSuccess));
  }

  // Loads the model file if not loaded yet on the background thread, and batch
  // executes it on the model execution thread.
  void LoadModelFileAndBatchExecute(
      BatchExecutionCallback callback_on_complete,
      ModelExecutor<OutputType, InputType>::ConstRefInputVector inputs) {
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!loaded_model_) {
      LoadModelFile(base::BindOnce(
          &TFLiteModelExecutor::BatchExecuteLoadedModelAndRunCallback,
          GetWeakPtrForExecutionThread(), std::move(callback_on_complete),
          inputs));
    } else {
      BatchExecuteLoadedModelAndRunCallback(std::move(callback_on_complete),
                                            inputs, ExecutionStatus::kSuccess);
    }
  }

  // Batch executes the loaded model for inputs.
  void BatchExecuteLoadedModel(
      ModelExecutor<OutputType, InputType>::ConstRefInputVector inputs,
      std::vector<std::optional<OutputType>>* outputs) {
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(loaded_model_);

    if (last_execution_time_) {
      // The max of this histogram is 3m since only the distribution and count
      // of smaller values is important.
      base::UmaHistogramMediumTimes(
          "OptimizationGuide.ModelExecutor.TimeSincePreviousRun." +
              GetStringNameForOptimizationTarget(optimization_target_),
          base::TimeTicks::Now() - *last_execution_time_);
    }
    last_execution_time_ = base::TimeTicks::Now();

    for (const InputType& input : inputs) {
      ScopedExecutionStatusResultRecorder status_recorder(optimization_target_);
      // IMPORTANT: Once the arm method is called, disarm must be called when
      // the model execution finishes. Do NOT early-return in this next block.
      if (watchdog_) {
        watchdog_->ArmWithTask(MakeCancelClosure());
      }
      {
        TRACE_EVENT1("browser", "OptGuideModelExecutor::Execute",
                     "OptimizationTarget",
                     optimization_guide::GetStringNameForOptimizationTarget(
                         optimization_target_));
        base::ElapsedThreadTimer execution_timer;
        base::ElapsedTimer elapsed_timer;
        std::optional<OutputType> output = Execute(
            loaded_model_.get(), status_recorder.mutable_status(), input);
        DCHECK_NE(status_recorder.status(), ExecutionStatus::kUnknown);
        outputs->push_back(output);

        // The max of this histogram is 1 hour because we want to understand
        // tail behavior and catch long running model executions.
        base::UmaHistogramLongTimes(
            "OptimizationGuide.ModelExecutor.ExecutionLatency." +
                GetStringNameForOptimizationTarget(optimization_target_),
            elapsed_timer.Elapsed());
        base::UmaHistogramLongTimes(
            "OptimizationGuide.ModelExecutor.ExecutionThreadTime." +
                GetStringNameForOptimizationTarget(optimization_target_),
            execution_timer.Elapsed());
        base::UmaHistogramMicrosecondsTimes(
            "OptimizationGuide.ModelExecutor.ExecutionThreadTimeMicroseconds." +
                GetStringNameForOptimizationTarget(optimization_target_),
            execution_timer.Elapsed());
      }
      if (watchdog_) {
        watchdog_->DisarmOnExecutionComplete();
      }
    }
  }

  // Batch executes the loaded model and runs callback on the reply thread.
  // Unloads the model if needed.
  void BatchExecuteLoadedModelAndRunCallback(
      BatchExecutionCallback callback_on_complete,
      ModelExecutor<OutputType, InputType>::ConstRefInputVector inputs,
      ExecutionStatus execution_status) {
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    std::vector<std::optional<OutputType>> outputs;
    outputs.reserve(inputs.size());
    if (!loaded_model_) {
      for (size_t i = 0; i < inputs.size(); i++) {
        outputs.push_back(std::nullopt);
        // If the model fails to load in a batch context, this status would not
        // get recorded the same number of times as it would in success. Thus,
        // increment the bucket |inputs.size()| number of times to keep metrics
        // sane.
        ScopedExecutionStatusResultRecorder status_recorder(
            optimization_target_);
        status_recorder.set_status(execution_status);
      }

      reply_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_on_complete), outputs));
      return;
    }

    BatchExecuteLoadedModel(inputs, &outputs);
    DCHECK(callback_on_complete);
    reply_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_on_complete), outputs));

    OnExecutionComplete();
  }

  void OnExecutionComplete() {
    DCHECK(execution_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (should_unload_model_on_complete_) {
      UnloadModel();
    }
  }

  base::OnceClosure MakeCancelClosure() {
#if BUILDFLAG(BUILD_WITH_MEDIAPIPE_LIB)
    return base::DoNothing();
#else
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // |base::Unretained| is safe here since the watchdog itself guarantees the
    // lifetime of the stored pointer will not extend beyond when it is
    // disarmed.
    return base::BindOnce(&ModelExecutionTask::Cancel,
                          base::Unretained(loaded_model_.get()));
#endif
  }

  proto::OptimizationTarget optimization_target_ =
      proto::OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN;

  bool should_unload_model_on_complete_ = true;

  bool should_preload_model_ = false;

  std::unique_ptr<ModelExecutionTimeoutWatchdog, base::OnTaskRunnerDeleter>
      watchdog_;

  // Main thread for model execution. For synchronous model execution, this
  // needs to be the same caller thread.
  scoped_refptr<base::SequencedTaskRunner> execution_task_runner_;

  // Arbitrary thread for running reply tasks.
  scoped_refptr<base::SequencedTaskRunner> reply_task_runner_;

  // Background thread for model loading file I/O.
  scoped_refptr<base::SequencedTaskRunner> model_loading_task_runner_;

  // The time that the model was last executed. Logged in metrics for the second
  // and following runs.
  std::optional<base::TimeTicks> last_execution_time_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The model file path to be loaded. May be nullopt if no model has been
  // downloaded yet.
  std::optional<base::FilePath> model_file_path_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Note on lifetimes: |loaded_model_| and |model_fb_| both share the same
  // lifetime, being set in |LoadModelFile()| and being destroyed in
  // |UnloadModel()|.

  std::unique_ptr<ModelExecutionTaskType> loaded_model_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // This will only be non-null when |model_file_path_| is set, and while the
  // model is loaded which is managed by a feature flag. `OnTaskRunnerDeleter`
  // is used to ensure that destruction occurs on a sequence that allows
  // blocking, since it involves closing a file handle.
  MemoryMappedFileDeleteOnTaskRunner model_fb_ GUARDED_BY_CONTEXT(
      sequence_checker_) = NullMemoryMappedFileDeleteOnTaskRunner();

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TFLiteModelExecutor>
      execution_sequence_weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TFLITE_MODEL_EXECUTOR_H_
