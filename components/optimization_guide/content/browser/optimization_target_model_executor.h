// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_TARGET_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_TARGET_MODEL_EXECUTOR_H_

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/tflite-support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"

namespace optimization_guide {

namespace {

// Util class for recording the result of loading the detection model. The
// result is recorded when it goes out of scope and its destructor is called.
class ScopedModelExecutorLoadingResultRecorder {
 public:
  ScopedModelExecutorLoadingResultRecorder(
      proto::OptimizationTarget optimization_target,
      ModelExecutorLoadingState model_loading_state)
      : optimization_target_(optimization_target),
        model_loading_state_(model_loading_state),
        start_time_(base::TimeTicks::Now()) {}

  ~ScopedModelExecutorLoadingResultRecorder() {
    base::UmaHistogramEnumeration(
        "OptimizationGuide.ModelExecutor.ModelLoadingResult." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        model_loading_state_);

    base::UmaHistogramTimes(
        "OptimizationGuide.ModelExecutor.ModelLoadingDuration." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        base::TimeTicks::Now() - start_time_);
  }

  void set_model_loading_state(ModelExecutorLoadingState model_executor_state) {
    model_loading_state_ = model_executor_state;
  }

 private:
  proto::OptimizationTarget optimization_target_;
  ModelExecutorLoadingState model_loading_state_;

  // The time at which this instance was constructed.
  const base::TimeTicks start_time_;
};

}  // namespace

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
    base::UmaHistogramCounts100(
        "OptimizationGuide.ModelExecutor.RunCount." +
            GetStringNameForOptimizationTarget(optimization_target_),
        run_count_);
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
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       base::TimeTicks::Now()));
  }

  // OptimizationTargetModelObserver:
  void OnModelFileUpdated(proto::OptimizationTarget optimization_target,
                          const base::Optional<proto::Any>& model_metadata,
                          const base::FilePath& file_path) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (optimization_target_ != optimization_target)
      return;

    model_metadata_to_load_ = model_metadata;
    file_path_to_load_ = file_path;

    if (features::LoadModelFileForEachExecution()) {
      // Wait for an actual execution before the model gets loaded.
      return;
    }

    // base::Unretained is safe here since model loading will not run if
    // |model_execution_task_runner_| gets destructed.
    model_execution_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&OptimizationTargetModelExecutor::LoadModelFile),
            base::Unretained(this)));
  }

  // Returns whether a model is currently loaded.
  bool HasLoadedModel() const { return loaded_model_ != nullptr; }

  // Returns the supported features for the loaded model, if the server provided
  // any.
  base::Optional<proto::Any> supported_features_for_loaded_model() const {
    return supported_features_for_loaded_model_;
  }
  // Validates that |supported_features_for_loaded_model_| is of the same type
  // and is parseable as |T|. Will return metadata if all checks pass.
  template <
      class T,
      class = typename std::enable_if<
          std::is_convertible<T*, google::protobuf::MessageLite*>{}>::type>
  base::Optional<T> ParsedSupportedFeaturesForLoadedModel() const {
    if (!supported_features_for_loaded_model_)
      return base::nullopt;
    return ParsedAnyMetadata<T>(*supported_features_for_loaded_model_);
  }

 protected:
  // Executes the model using |execution_task| on |args|.
  virtual base::Optional<OutputType> Execute(ModelExecutionTask* execution_task,
                                             InputTypes... args) = 0;

  // Builds a model execution task using |model_file|.
  virtual std::unique_ptr<ModelExecutionTask> BuildModelExecutionTask(
      base::MemoryMappedFile* model_file) = 0;

 private:
  void ResetLoadedModel() {
    DCHECK(model_execution_task_runner_->RunsTasksInCurrentSequence());
    loaded_model_.reset();
    model_fb_.reset();
    supported_features_for_loaded_model_ = base::nullopt;
  }

  // Callback invoked when a model file for |optimization_target| has been
  // loaded. A true return value indicates the model was loaded successfully,
  // false otherwise.
  bool LoadModelFile() {
    DCHECK(model_execution_task_runner_->RunsTasksInCurrentSequence());
    ScopedModelExecutorLoadingResultRecorder scoped_model_loading_recorder(
        optimization_target_, ModelExecutorLoadingState::kModelFileInvalid);

    // We received a new model file. Reset any loaded models.
    ResetLoadedModel();

    if (!file_path_to_load_)
      return false;

    std::unique_ptr<base::MemoryMappedFile> model_fb =
        std::make_unique<base::MemoryMappedFile>();
    if (!model_fb->Initialize(*file_path_to_load_))
      return false;
    model_fb_ = std::move(model_fb);

    supported_features_for_loaded_model_ = model_metadata_to_load_;

    loaded_model_ = BuildModelExecutionTask(model_fb_.get());
    if (loaded_model_) {
      scoped_model_loading_recorder.set_model_loading_state(
          ModelExecutorLoadingState::kModelFileValidAndMemoryMapped);
    }

    return !!loaded_model_;
  }

  base::Optional<OutputType> SendForExecution(InputTypes... args) {
    DCHECK(model_execution_task_runner_->RunsTasksInCurrentSequence());

    // If there's no model to be loaded, no model can be loaded.
    if (!file_path_to_load_) {
      DCHECK(!loaded_model_);
      return base::nullopt;
    }

    // Attempt to load the model file if it isn't loaded yet, fail if loading is
    // unsuccessful.
    if (!loaded_model_ && !LoadModelFile())
      return base::nullopt;

    run_count_++;
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
    return Execute(loaded_model_.get(), args...);
  }

  void OnExecutionCompleted(ExecutionCallback callback,
                            base::TimeTicks model_execute_start_time,
                            base::Optional<OutputType> output) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // If the model file should only be loaded for execution, then unload it
    // from memory. This should be done in a PostTask since it may take a while
    // for big models and the metrics below shouldn't be skewed, and
    // |loaded_model_| should only be accessed by
    // |model_execution_task_runner_|.
    if (features::LoadModelFileForEachExecution()) {
      model_execution_task_runner_->PostTask(
          FROM_HERE,
          // base::Unretained is safe here since |model_execution_task_runner_|
          // is owned by |this|.
          base::BindOnce(&OptimizationTargetModelExecutor::ResetLoadedModel,
                         base::Unretained(this)));
    }

    if (!output) {
      std::move(callback).Run(output);
      return;
    }

    base::TimeDelta execution_time =
        base::TimeTicks::Now() - model_execute_start_time;

    base::UmaHistogramMediumTimes(
        "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        execution_time);
    std::move(callback).Run(output);
  }

  // Not owned. Guaranteed to outlive |this|.
  OptimizationGuideDecider* decider_;

  proto::OptimizationTarget optimization_target_;

  // Incremented every time the model is run and logged in metrics on
  // destruction.
  size_t run_count_ = 0;

  // The time that the model was last executed. Logged in metrics for the second
  // and following runs.
  base::Optional<base::TimeTicks> last_execution_time_;

  scoped_refptr<base::SequencedTaskRunner> model_execution_task_runner_;

  // When the model file is updated, the server may pass this metadata that
  // accompanies the model. This can be nullopt even after a model file update
  // occurs since.
  base::Optional<proto::Any> model_metadata_to_load_;

  // The model file path to be loaded. May be nullopt if no model has been
  // downloaded yet.
  base::Optional<base::FilePath> file_path_to_load_;

  // Note on lifetimes: |model_fb_|, |loaded_model_|, and
  // |supported_features_for_loaded_model_| all share the same lifetime, being
  // set in |LoadModelFile()| and being destroyed in |ResetModelFile()|.

  // This will only be non-null when |file_path_to_load_| is set, and while the
  // model is loaded which is manged by a feature flag. See also the above note
  // regarding lifetime.
  std::unique_ptr<base::MemoryMappedFile> model_fb_;

  // |loaded_model_| should only be accessed on |model_execution_task_runner_|.
  // See also the above note regarding lifetime.
  std::unique_ptr<ModelExecutionTask> loaded_model_;

  // See the above note regarding lifetime.
  base::Optional<proto::Any> supported_features_for_loaded_model_;

  base::WeakPtrFactory<OptimizationTargetModelExecutor> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_TARGET_MODEL_EXECUTOR_H_
