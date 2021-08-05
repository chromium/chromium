// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTOR_H_

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

// This class handles the execution, loading, unloading, and associated metrics
// of machine learning models in Optimization Guide on a background thread. This
// class is meant to be used and owned by an instance of |ModelHandler|. A
// ModelExecutor must be passed to a ModelHandler's constructor, this design
// allows the implementer of a ModelExecutor to define how the model is built
// and executed.. See also base_model_executor.h and
// base_model_executor_helpers.h in this directory for helpful derived classes.
//
// Lifetime: This class can be constructed on any thread but cannot do anything
// useful until |InitializeAndMoveToBackgroundThread| is called. After that
// method is called, all subsequent calls to this class must be made through the
// |background_task_runner| that was passed to initialize. Furthermore, all
// WeakPointers of this class must only be dereferenced on the background thread
// as well. This in turn means that this class must be destroyed on the
// background thread as well.
template <class OutputType, class... InputTypes>
class ModelExecutor {
 public:
  ModelExecutor() = default;
  virtual ~ModelExecutor() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // Should be called on the same sequence as the ctor, but once called |this|
  // must only be used from a background thread/sequence.
  void InitializeAndMoveToBackgroundThread(
      proto::OptimizationTarget optimization_target,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner) {
    DCHECK(!background_task_runner_);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_NE(optimization_target,
              proto::OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN);

    DETACH_FROM_SEQUENCE(sequence_checker_);
    optimization_target_ = optimization_target;
    background_task_runner_ = background_task_runner;
    reply_task_runner_ = reply_task_runner;
  }

  // Called when a model file is available to load. Depending on feature flags,
  // the model may or may not be immediately loaded.
  void UpdateModelFile(const base::FilePath& file_path) {
    DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    ResetLoadedModel();

    model_file_path_ = file_path;

    if (!features::LoadModelFileForEachExecution()) {
      LoadModelFile();
    }
  }

  // Starts the execution of the model. When complete, |ui_callback_on_complete|
  // will be run on the UI thread with the output of the model.
  using ExecutionCallback =
      base::OnceCallback<void(const absl::optional<OutputType>&)>;
  void SendForExecution(ExecutionCallback ui_callback_on_complete,
                        base::TimeTicks start_time,
                        InputTypes... args) {
    DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(reply_task_runner_);

    base::TimeDelta task_scheduling_latency =
        base::TimeTicks::Now() - start_time;
    base::UmaHistogramMediumTimes(
        "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        task_scheduling_latency);

    // Attempt to load the model file if it isn't loaded yet, fail if loading is
    // unsuccessful or no model is available to load.
    if (!loaded_model_ && !LoadModelFile()) {
      reply_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(ui_callback_on_complete), absl::nullopt));
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
    {
      TRACE_EVENT1("browser", "OptGuideModelExecutor::Execute",
                   "OptimizationTarget",
                   optimization_guide::GetStringNameForOptimizationTarget(
                       optimization_target_));
      base::TimeTicks execute_start_time = base::TimeTicks::Now();
      output = Execute(loaded_model_.get(), args...);
      // The max of this histogram is 1 hour because we want to understand
      // tail behavior and catch long running model executions.
      base::UmaHistogramLongTimes(
          "OptimizationGuide.ModelExecutor.ExecutionLatency." +
              GetStringNameForOptimizationTarget(optimization_target_),
          base::TimeTicks::Now() - execute_start_time);
    }

    DCHECK(ui_callback_on_complete);
    reply_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(ui_callback_on_complete), output));

    // If the model file should only be loaded for execution, then unload it
    // from memory. This can be done in a PostTask since it may take a while
    // for big models and isn't very important.
    if (features::LoadModelFileForEachExecution()) {
      background_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ModelExecutor::ResetLoadedModel,
                                    GetBackgroundWeakPtr()));
    }
  }

  // IMPORTANT: These WeakPointers must only be dereferenced on the background
  // thread.
  base::WeakPtr<ModelExecutor> GetBackgroundWeakPtr() {
    return background_weak_ptr_factory_.GetWeakPtr();
  }

  ModelExecutor(const ModelExecutor&) = delete;
  ModelExecutor& operator=(const ModelExecutor&) = delete;

 protected:
  using ModelExecutionTask =
      tflite::task::core::BaseTaskApi<OutputType, InputTypes...>;

  // Executes the model using |execution_task| on |args|.
  virtual absl::optional<OutputType> Execute(ModelExecutionTask* execution_task,
                                             InputTypes... args) = 0;

  // Builds a model execution task using |model_file|.
  virtual std::unique_ptr<ModelExecutionTask> BuildModelExecutionTask(
      base::MemoryMappedFile* model_file) = 0;

 private:
  void ResetLoadedModel() {
    TRACE_EVENT1("browser", "OptGuideModelExecutor::ResetLoadedModel",
                 "OptimizationTarget",
                 optimization_guide::GetStringNameForOptimizationTarget(
                     optimization_target_));
    DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    loaded_model_.reset();
    model_fb_.reset();
  }

  // A true return value indicates the model was loaded successfully, false
  // otherwise.
  bool LoadModelFile() {
    TRACE_EVENT1("browser", "OptGuideModelExecutor::LoadModelFile",
                 "OptimizationTarget",
                 optimization_guide::GetStringNameForOptimizationTarget(
                     optimization_target_));
    DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    ScopedModelExecutorLoadingResultRecorder scoped_model_loading_recorder(
        optimization_target_, ModelExecutorLoadingState::kModelFileInvalid);

    ResetLoadedModel();

    base::UmaHistogramBoolean(
        "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
            GetStringNameForOptimizationTarget(optimization_target_),
        !!model_file_path_);

    if (!model_file_path_)
      return false;

    std::unique_ptr<base::MemoryMappedFile> model_fb =
        std::make_unique<base::MemoryMappedFile>();
    if (!model_fb->Initialize(*model_file_path_))
      return false;
    model_fb_ = std::move(model_fb);

    loaded_model_ = BuildModelExecutionTask(model_fb_.get());
    if (loaded_model_) {
      scoped_model_loading_recorder.set_model_loading_state(
          ModelExecutorLoadingState::kModelFileValidAndMemoryMapped);
    }

    return !!loaded_model_;
  }

  proto::OptimizationTarget optimization_target_ =
      proto::OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

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

  base::WeakPtrFactory<ModelExecutor> background_weak_ptr_factory_{this};
};

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

  // OptimizationTargetModelObserver:
  void OnModelUpdated(proto::OptimizationTarget optimization_target,
                      const ModelInfo& model_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (optimization_target_ != optimization_target)
      return;

    supported_features_for_loaded_model_ = model_info.GetModelMetadata();
    model_available_ = true;

    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ModelExecutor<OutputType, InputTypes...>::UpdateModelFile,
            background_executor_->GetBackgroundWeakPtr(),
            model_info.GetModelFilePath()));
  }

  // Returns whether a model is available to be executed. Virtual for testing.
  virtual bool ModelAvailable() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return model_available_;
  }

  // Validates that |supported_features_for_loaded_model_| is of the same type
  // and is parseable as |T|. Will return metadata if all checks pass.
  template <
      class T,
      class = typename std::enable_if<
          std::is_convertible<T*, google::protobuf::MessageLite*>{}>::type>
  absl::optional<T> ParsedSupportedFeaturesForLoadedModel() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!supported_features_for_loaded_model_)
      return absl::nullopt;
    return ParsedAnyMetadata<T>(*supported_features_for_loaded_model_);
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
  OptimizationGuideModelProvider* model_provider_
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
  absl::optional<proto::Any> supported_features_for_loaded_model_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Set in |OnModelUpdated|.
  bool model_available_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTOR_H_
