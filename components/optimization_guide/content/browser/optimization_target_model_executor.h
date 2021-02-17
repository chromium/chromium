// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_TARGET_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_TARGET_MODEL_EXECUTOR_H_

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/tflite-support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"

namespace optimization_guide {

template <class OutputType, class... InputTypes>
class OptimizationTargetModelExecutor : public OptimizationTargetModelObserver {
 public:
  using ModelExecutionTask =
      tflite::task::core::BaseTaskApi<OutputType, InputTypes...>;

  OptimizationTargetModelExecutor(
      OptimizationGuideDecider* decider,
      proto::OptimizationTarget optimization_target,
      const base::Optional<proto::Any>& model_metadata,
      const scoped_refptr<base::SequencedTaskRunner>&
          model_execution_task_runner)
      : decider_(decider),
        optimization_target_(optimization_target),
        model_execution_task_runner_(model_execution_task_runner) {
    DCHECK(decider_);
    DCHECK_NE(optimization_target_,
              proto::OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN);

    decider_->AddObserverForOptimizationTargetModel(optimization_target_,
                                                    model_metadata, this);
  }
  ~OptimizationTargetModelExecutor() override {
    decider_->RemoveObserverForOptimizationTargetModel(optimization_target_,
                                                       this);
  }
  OptimizationTargetModelExecutor(const OptimizationTargetModelExecutor&) =
      delete;
  OptimizationTargetModelExecutor& operator=(
      const OptimizationTargetModelExecutor&) = delete;

  // Executes the model using |input| and invokes |callback| when completed.
  // TODO(crbug/1173328): Add a way to surface errors.
  using ExecutionCallback =
      base::OnceCallback<void(const base::Optional<OutputType>&)>;
  void ExecuteModelWithInput(ExecutionCallback callback, InputTypes... input) {
    // base::Unretained is safe here since the execution will not run if
    // |model_execution_task_runner_| gets destructed.
    model_execution_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&OptimizationTargetModelExecutor::SendForExecution,
                       base::Unretained(this), input...),
        base::BindOnce(&OptimizationTargetModelExecutor::OnExecutionCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // OptimizationTargetModelObserver:
  void OnModelFileUpdated(proto::OptimizationTarget optimization_target,
                          const base::Optional<proto::Any>& model_metadata,
                          const base::FilePath& file_path) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (optimization_target_ != optimization_target)
      return;

    // base::Unretained is safe here since model loading will not run if
    // |model_execution_task_runner_| gets destructed.
    model_execution_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OptimizationTargetModelExecutor::LoadModelFile,
                       base::Unretained(this), model_metadata, file_path));
  }

  // Returns whether a model is currently loaded.
  bool HasLoadedModel() const { return loaded_model_ != nullptr; }

  // Returns the supported features for the loaded model, if the server provided
  // any.
  base::Optional<proto::Any> supported_features_for_loaded_model() const {
    return supported_features_for_loaded_model_;
  }

 protected:
  // Executes the model using |execution_task| on |args|.
  virtual base::Optional<OutputType> Execute(ModelExecutionTask* execution_task,
                                             InputTypes... args) = 0;

  // Builds a model execution task using |model_file|.
  virtual std::unique_ptr<ModelExecutionTask> BuildModelExecutionTask(
      base::MemoryMappedFile* model_file) = 0;

 private:
  // Callback invoked when a model file for |optimization_target| has been
  // loaded.
  void LoadModelFile(const base::Optional<proto::Any>& model_metadata,
                     const base::FilePath& file_path) {
    DCHECK(model_execution_task_runner_->RunsTasksInCurrentSequence());

    // We received a new model file. Reset any loaded models.
    loaded_model_.reset();
    model_fb_.reset();
    supported_features_for_loaded_model_ = base::nullopt;

    std::unique_ptr<base::MemoryMappedFile> model_fb =
        std::make_unique<base::MemoryMappedFile>();
    if (!model_fb->Initialize(file_path))
      return;
    model_fb_ = std::move(model_fb);

    supported_features_for_loaded_model_ = model_metadata;

    loaded_model_ = BuildModelExecutionTask(model_fb_.get());
  }

  base::Optional<OutputType> SendForExecution(InputTypes... args) {
    DCHECK(model_execution_task_runner_->RunsTasksInCurrentSequence());

    if (!loaded_model_)
      return base::nullopt;

    return Execute(loaded_model_.get(), args...);
  }

  void OnExecutionCompleted(ExecutionCallback callback,
                            base::Optional<OutputType> output) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::move(callback).Run(output);
  }

  // Not owned. Guaranteed to outlive |this|.
  OptimizationGuideDecider* decider_;

  proto::OptimizationTarget optimization_target_;

  scoped_refptr<base::SequencedTaskRunner> model_execution_task_runner_;

  std::unique_ptr<base::MemoryMappedFile> model_fb_;
  std::unique_ptr<ModelExecutionTask> loaded_model_;
  base::Optional<proto::Any> supported_features_for_loaded_model_;

  base::WeakPtrFactory<OptimizationTargetModelExecutor> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_TARGET_MODEL_EXECUTOR_H_
