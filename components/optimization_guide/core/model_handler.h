// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_HANDLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_HANDLER_H_

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// This class owns and handles the execution of models on the UI thread. Derived
// classes must provide an implementation of |ModelExecutor|
// (see above) which is then owned by |this|. The passed executor will be called
// and destroyed on a background thread, which is all handled by this class.
template <class OutputType, class... InputTypes>
class ModelHandler : public OptimizationTargetModelObserver {
 public:
  ModelHandler(OptimizationGuideModelProvider* model_provider,
               scoped_refptr<base::SequencedTaskRunner> background_task_runner,
               std::unique_ptr<ModelExecutor<OutputType, InputTypes...>>
                   background_executor,
               proto::OptimizationTarget optimization_target,
               const absl::optional<proto::Any>& model_metadata)
      : model_provider_(model_provider),
        optimization_target_(optimization_target),
        background_executor_(std::move(background_executor)),
        background_task_runner_(background_task_runner) {
    DCHECK(model_provider_);
    DCHECK(background_executor_);
    DCHECK_NE(optimization_target_,
              proto::OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN);

    model_provider_->AddObserverForOptimizationTargetModel(
        optimization_target_, model_metadata, this);
    background_executor_->InitializeAndMoveToBackgroundThread(
        optimization_target_, background_task_runner_,
        base::SequencedTaskRunnerHandle::Get());
  }
  ~ModelHandler() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    model_provider_->RemoveObserverForOptimizationTargetModel(
        optimization_target_, this);

    // |background_executor_|'s  WeakPtrs are used on the background thread, so
    // that is also where the class must be destroyed.
    background_task_runner_->DeleteSoon(FROM_HERE,
                                        std::move(background_executor_));
  }
  ModelHandler(const ModelHandler&) = delete;
  ModelHandler& operator=(const ModelHandler&) = delete;

  // Executes the model using |input| and invokes |callback| on the UI thread
  // when completed. Virtual for testing.
  // TODO(crbug/1173328): Add a way to surface errors.
  using ExecutionCallback =
      base::OnceCallback<void(const absl::optional<OutputType>&)>;
  virtual void ExecuteModelWithInput(ExecutionCallback callback,
                                     InputTypes... input) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::TimeTicks now = base::TimeTicks::Now();

    ExecutionCallback on_complete_callback =
        base::BindOnce(&ModelHandler::OnExecutionCompleted, std::move(callback),
                       optimization_target_, now);
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ModelExecutor<OutputType, InputTypes...>::SendForExecution,
            background_executor_->GetBackgroundWeakPtr(),
            std::move(on_complete_callback), now, input...));
  }

  void SetShouldUnloadModelOnComplete(bool should_auto_unload) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ModelExecutor<OutputType,
                           InputTypes...>::SetShouldUnloadModelOnComplete,
            background_executor_->GetBackgroundWeakPtr(), should_auto_unload));
  }

  // Requests that the model executor unload the model from memory, if it is
  // currently loaded.
  void UnloadModel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ModelExecutor<OutputType, InputTypes...>::UnloadModel,
                       background_executor_->GetBackgroundWeakPtr()));
  }

  // OptimizationTargetModelObserver:
  void OnModelUpdated(proto::OptimizationTarget optimization_target,
                      const ModelInfo& model_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (optimization_target_ != optimization_target)
      return;

    model_info_ = model_info;
    model_available_ = true;

    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ModelExecutor<OutputType, InputTypes...>::UpdateModelFile,
            background_executor_->GetBackgroundWeakPtr(),
            model_info.GetModelFilePath()));

    // Run any observing callbacks after the model file is posted to the
    // background thread so that any model execution requests are posted to the
    // background thread after the model update.
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

  absl::optional<ModelInfo> GetModelInfo() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return model_info_;
  }

  // Validates that the model info's metadata is of the same type and is
  // parseable as |T|. Will return metadata if all checks pass.
  template <
      class T,
      class = typename std::enable_if<
          std::is_convertible<T*, google::protobuf::MessageLite*>{}>::type>
  absl::optional<T> ParsedSupportedFeaturesForLoadedModel() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!model_info_ || !model_info_->GetModelMetadata())
      return absl::nullopt;
    return ParsedAnyMetadata<T>(*model_info_->GetModelMetadata());
  }

 private:
  // This is called by |background_executor_|. This method does not have to be
  // static, but because it is stateless we've made it static so that we don't
  // have to have this class support WeakPointers.
  static void OnExecutionCompleted(
      ExecutionCallback callback,
      proto::OptimizationTarget optimization_target,
      base::TimeTicks model_execute_start_time,
      const absl::optional<OutputType>& output) {
    if (!output) {
      std::move(callback).Run(output);
      return;
    }

    base::TimeDelta execution_time =
        base::TimeTicks::Now() - model_execute_start_time;

    base::UmaHistogramMediumTimes(
        "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target),
        execution_time);
    std::move(callback).Run(output);
  }

  // Not owned. Guaranteed to outlive |this|.
  raw_ptr<OptimizationGuideModelProvider> model_provider_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const proto::OptimizationTarget optimization_target_;

  // The owned background executor.
  std::unique_ptr<ModelExecutor<OutputType, InputTypes...>>
      background_executor_;

  // The background task runner. Note that whenever a task is posted here, the
  // task takes a reference to the TaskRunner (in a cyclic dependency) so
  // |base::Unretained| is not safe anywhere in this class or the
  // |background_executor_|.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Set in |OnModelUpdated|.
  absl::optional<ModelInfo> model_info_ GUARDED_BY_CONTEXT(sequence_checker_);

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
