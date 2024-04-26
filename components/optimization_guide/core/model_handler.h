// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_HANDLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_HANDLER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

namespace {

void RecordTaskExecutionLatency(proto::OptimizationTarget optimization_target,
                                base::TimeDelta execution_time) {
  base::UmaHistogramMediumTimes(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              optimization_target),
      execution_time);
}

}  // namespace

// This class owns and handles the execution of models on the UI thread.
// Derived classes must provide an implementation of `ModelExecutor`
// which is then owned by `this`. The passed executor will be called
// and destroyed on the thread specified by `model_executor_task_runner`,
// which is all handled by this class.
//
// Derived classes that override `OnModelUpdated` must call the parent
// `OnModelUpdated` as the first step, for the internal state to be updated.
template <class OutputType, class InputType>
class ModelHandler : public OptimizationTargetModelObserver {
 public:
  ModelHandler(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner,
      std::unique_ptr<ModelExecutor<OutputType, InputType>> model_executor,
      // Passing nullopt will use a default value.
      std::optional<base::TimeDelta> model_inference_timeout,
      proto::OptimizationTarget optimization_target,
      const std::optional<proto::Any>& model_metadata)
      : model_provider_(model_provider),
        optimization_target_(optimization_target),
        model_executor_(std::move(model_executor)),
        model_executor_task_runner_(model_executor_task_runner) {
    DCHECK(model_provider_);
    DCHECK(model_executor_);
    DCHECK_NE(optimization_target_,
              proto::OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN);

    base::UmaHistogramBoolean(
        "OptimizationGuide.ModelHandler.HandlerCreated." +
            GetStringNameForOptimizationTarget(optimization_target_),
        true);

    handler_created_time_ = base::TimeTicks::Now();

    model_executor_->InitializeAndMoveToExecutionThread(
        model_inference_timeout, optimization_target_,
        model_executor_task_runner_,
        base::SequencedTaskRunner::GetCurrentDefault());

    // Run this after the executor is initialized in case the model is already
    // available.
    model_provider_->AddObserverForOptimizationTargetModel(
        optimization_target_, model_metadata, this);
  }
  ~ModelHandler() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    model_provider_->RemoveObserverForOptimizationTargetModel(
        optimization_target_, this);

    // |model_executor_|'s  WeakPtrs are used on the model thread, so
    // that is also where the class must be destroyed.
    model_executor_task_runner_->DeleteSoon(FROM_HERE,
                                            std::move(model_executor_));
  }
  ModelHandler(const ModelHandler&) = delete;
  ModelHandler& operator=(const ModelHandler&) = delete;

  // Executes the model using |input| and invokes |callback| on the UI thread
  // when completed. Virtual for testing.
  // TODO(crbug.com/40167079): Add a way to surface errors.
  using ExecutionCallback =
      base::OnceCallback<void(const std::optional<OutputType>&)>;
  virtual void ExecuteModelWithInput(ExecutionCallback callback,
                                     InputType input) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    model_executor_task_runner_->PostTask(
        FROM_HERE, GetExecutionTask(std::move(callback), input));
  }

  // Same as the method above. But also receives a `base::CancelableTaskTracker`
  // for cancelling the execution. Keep in mind that CancelableTaskTracker
  // cannot cancel tasks that have already started to run. Virtual for testing.
  // TODO(crbug.com/40167079): Add a way to surface errors.
  virtual void ExecuteModelWithInput(base::CancelableTaskTracker* tracker,
                                     ExecutionCallback callback,
                                     InputType input) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    tracker->PostTask(model_executor_task_runner_.get(), FROM_HERE,
                      GetExecutionTask(std::move(callback), input));
  }

  // Variants of the above |ExecuteModelWithInput| but which support running
  // multiple model executions in the same call stack. It is guaranteed that the
  // output passed to |BatchExecutionCallback| will always be in the same order
  // as the input vector.
  //
  // IMPORTANT: Running the model multiple times in the same PostTask means that
  // it will take longer for Chrome's threadpool to reuse these CPU cycles for
  // other work, especially for high priority tasks. This method should only be
  // used in time-sensitive applications, for example when the model output is
  // used on UI surfaces. Otherwise use multiple calls to
  // |ExecuteModelWithInput| with a |base::BarrierClosure| (see
  // page_content_annotation_job_executor.cc for an example and explanation).
  using BatchExecutionCallback =
      base::OnceCallback<void(const std::vector<std::optional<OutputType>>&)>;
  virtual void BatchExecuteModelWithInput(
      BatchExecutionCallback callback,
      typename ModelExecutor<OutputType, InputType>::ConstRefInputVector
          batch_input) {
    model_executor_task_runner_->PostTask(
        FROM_HERE, GetBatchExecutionTask(std::move(callback), batch_input));
  }

  // See above comment.
  virtual void BatchExecuteModelWithInput(
      base::CancelableTaskTracker* tracker,
      BatchExecutionCallback callback,
      typename ModelExecutor<OutputType, InputType>::ConstRefInputVector
          batch_input) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    tracker->PostTask(model_executor_task_runner_.get(), FROM_HERE,
                      GetBatchExecutionTask(std::move(callback), batch_input));
  }

  // Runs synchronous batch model execution.
  // Returns batch model outputs.
  std::vector<std::optional<OutputType>> BatchExecuteModelWithInputSync(
      typename ModelExecutor<OutputType, InputType>::ConstRefInputVector
          inputs) {
    base::ElapsedTimer timer;
    auto batch_model_outputs =
        model_executor_->SendForBatchExecutionSync(inputs);
    RecordTaskExecutionLatency(optimization_target_,
                               /*execution_time=*/timer.Elapsed());
    return batch_model_outputs;
  }

  // Note that keeping the model in memory for a long duration may be detected
  // as a memory leak in Chrome, and will always increase the private or shared
  // memory used by the browser by the size of the model file and the
  // constructed TFLite graph.
  void SetShouldUnloadModelOnComplete(bool should_auto_unload) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    model_executor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ModelExecutor<OutputType,
                           InputType>::SetShouldUnloadModelOnComplete,
            model_executor_->GetWeakPtrForExecutionThread(),
            should_auto_unload));
  }

  // Note that keeping the model in memory for a long duration may be detected
  // as a memory leak in Chrome, and will always increase the private or shared
  // memory used by the browser by the size of the model file and the
  // constructed TFLite graph.
  void SetShouldPreloadModel(bool should_preload_model) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    model_executor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ModelExecutor<OutputType, InputType>::SetShouldPreloadModel,
            model_executor_->GetWeakPtrForExecutionThread(),
            should_preload_model));
  }

  // Requests that the model executor unload the model from memory, if it is
  // currently loaded. Virtual to allow derived classes to also observe this
  // signal.
  virtual void UnloadModel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    model_executor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ModelExecutor<OutputType, InputType>::UnloadModel,
                       model_executor_->GetWeakPtrForExecutionThread()));
  }

  // OptimizationTargetModelObserver:
  void OnModelUpdated(proto::OptimizationTarget optimization_target,
                      base::optional_ref<const ModelInfo> model_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::optional<base::FilePath> model_file_path;

    if (optimization_target_ != optimization_target)
      return;

    if (handler_created_time_) {
      base::UmaHistogramMediumTimes(
          "OptimizationGuide.ModelHandler.HandlerCreatedToModelAvailable." +
              GetStringNameForOptimizationTarget(optimization_target_),
          base::TimeTicks::Now() - *handler_created_time_);
      handler_created_time_ = std::nullopt;
    }

    model_available_ = model_info.has_value();
    if (model_info.has_value()) {
      model_info_ = *model_info;
      model_file_path = model_info->GetModelFilePath();
    } else {
      model_info_ = std::nullopt;
    }

    model_executor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ModelExecutor<OutputType, InputType>::UpdateModelFile,
                       model_executor_->GetWeakPtrForExecutionThread(),
                       model_file_path));

    // Run any observing callbacks after the model file is posted to the
    // model executor thread so that any model execution requests are posted to
    // the model executor thread after the model update.
    on_model_updated_callbacks_.Notify();
  }

  // Returns whether a model is available to be executed. Virtual for testing.
  virtual bool ModelAvailable() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return model_available_;
  }

  // Runs |callback| now if |ModelAvailable()| or the next time |OnModelUpdated|
  // is called.
  void AddOnModelUpdatedCallback(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (ModelAvailable()) {
      std::move(callback).Run();
      return;
    }
    // callbacks are not bound locally are are safe to be destroyed at any time.
    on_model_updated_callbacks_.AddUnsafe(std::move(callback));
  }

  std::optional<ModelInfo> GetModelInfo() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return model_info_;
  }

  // Validates that the model info's metadata is of the same type and is
  // parseable as |T|. Will return metadata if all checks pass.
  template <
      class T,
      class = typename std::enable_if<
          std::is_convertible<T*, google::protobuf::MessageLite*>{}>::type>
  std::optional<T> ParsedSupportedFeaturesForLoadedModel() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!model_info_ || !model_info_->GetModelMetadata())
      return std::nullopt;
    return ParsedAnyMetadata<T>(*model_info_->GetModelMetadata());
  }

 private:
  // Returns a closure supplied with |callback| and |input| for model execution.
  base::OnceClosure GetExecutionTask(ExecutionCallback callback,
                                     InputType input) {
    base::TimeTicks now = base::TimeTicks::Now();

    ExecutionCallback on_complete_callback =
        base::BindOnce(&ModelHandler::OnExecutionCompleted, std::move(callback),
                       optimization_target_, now);
    return base::BindOnce(
        &ModelExecutor<OutputType, InputType>::SendForExecution,
        model_executor_->GetWeakPtrForExecutionThread(),
        std::move(on_complete_callback), now, input);
  }

  // Returns a closure supplied with |callback| and |inputs| for model
  // execution.
  base::OnceClosure GetBatchExecutionTask(
      BatchExecutionCallback callback,
      typename ModelExecutor<OutputType, InputType>::ConstRefInputVector
          inputs) {
    base::TimeTicks now = base::TimeTicks::Now();

    BatchExecutionCallback on_complete_callback =
        base::BindOnce(&ModelHandler::OnBatchExecutionCompleted,
                       std::move(callback), optimization_target_, now);
    return base::BindOnce(
        &ModelExecutor<OutputType, InputType>::SendForBatchExecution,
        model_executor_->GetWeakPtrForExecutionThread(),
        std::move(on_complete_callback), now, inputs);
  }

  // This is called by |model_executor_|. This method does not have to be
  // static, but because it is stateless we've made it static so that we don't
  // have to have this class support WeakPointers on the calling thread.
  static void OnExecutionCompleted(
      ExecutionCallback callback,
      proto::OptimizationTarget optimization_target,
      base::TimeTicks model_execute_start_time,
      const std::optional<OutputType>& output) {
    RecordTaskExecutionLatency(
        optimization_target,
        /*execution_time=*/base::TimeTicks::Now() - model_execute_start_time);

    std::move(callback).Run(output);
  }

  static void OnBatchExecutionCompleted(
      BatchExecutionCallback callback,
      proto::OptimizationTarget optimization_target,
      base::TimeTicks model_execute_start_time,
      const std::vector<std::optional<OutputType>>& output) {
    RecordTaskExecutionLatency(
        optimization_target,
        /*execution_time=*/base::TimeTicks::Now() - model_execute_start_time);

    std::move(callback).Run(output);
  }

  // Not owned. Guaranteed to outlive |this|.
  raw_ptr<OptimizationGuideModelProvider> model_provider_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const proto::OptimizationTarget optimization_target_;

  // The time that |optimization_target_| was registered wih |model_provider_|
  // when |this| is created.
  //
  // Will only be non-nullopt if a model has not been received yet after the
  // target was registered.
  std::optional<base::TimeTicks> handler_created_time_;

  // The owned model executor.
  std::unique_ptr<ModelExecutor<OutputType, InputType>> model_executor_;

  // The model executor task runner. Note that whenever a task is posted here,
  // the task takes a reference to the TaskRunner (in a cyclic dependency) so
  // |base::Unretained| is not safe anywhere in this class or the
  // |model_executor_|.
  scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner_;

  // Set in |OnModelUpdated|.
  std::optional<ModelInfo> model_info_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Populated with callbacks if |AddOnModelUpdatedCallback| is called before a
  // model file is available, then is notified when |OnModelUpdated| is called.
  base::OnceClosureList on_model_updated_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Set in |OnModelUpdated|.
  bool model_available_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_HANDLER_H_
