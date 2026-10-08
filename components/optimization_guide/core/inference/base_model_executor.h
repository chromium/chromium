// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_INFERENCE_BASE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_INFERENCE_BASE_MODEL_EXECUTOR_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/inference/execution_status.h"
#include "components/optimization_guide/core/inference/tflite_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/tflite_engine.h"

namespace optimization_guide {

// An ModelExecutor that executes models with arbitrary input and output types.
// Note that callers will need to give an implementation of this class to a
// |ModelHandler|, whereas the handle is the actual class that calling code
// would own and call into.
template <class OutputType, class InputType>
class BaseModelExecutor
    : public TFLiteModelExecutor<OutputType,
                                 InputType,
                                 tflite::task::core::TfLiteEngine> {
 public:
  using ModelExecutionTask = tflite::task::core::TfLiteEngine;

  BaseModelExecutor() = default;
  ~BaseModelExecutor() override = default;
  BaseModelExecutor(const BaseModelExecutor&) = delete;
  BaseModelExecutor& operator=(const BaseModelExecutor&) = delete;

  // TFLiteModelExecutor:
  void InitializeAndMoveToExecutionThread(
      std::optional<base::TimeDelta> model_inference_timeout,
      proto::OptimizationTarget optimization_target,
      scoped_refptr<base::SequencedTaskRunner> model_loading_task_runner,
      scoped_refptr<base::SequencedTaskRunner> execution_task_runner,
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner) override {
    num_threads_ = features::OverrideNumThreadsForOptTarget(optimization_target)
                       .value_or(-1);
    TFLiteModelExecutor<OutputType, InputType, ModelExecutionTask>::
        InitializeAndMoveToExecutionThread(
            model_inference_timeout, optimization_target,
            model_loading_task_runner, execution_task_runner,
            reply_task_runner);
  }

 protected:
  std::optional<OutputType> Execute(ModelExecutionTask* execution_task,
                                    ExecutionStatus* out_status,
                                    InputType input) override {
    if (!Preprocess(execution_task->GetInputs(), input)) {
      *out_status = ExecutionStatus::kErrorUnknown;
      return std::nullopt;
    }
    absl::Status status =
        execution_task->interpreter_wrapper()->InvokeWithoutFallback();
    if (absl::IsCancelled(status)) {
      *out_status = ExecutionStatus::kErrorCancelled;
      return std::nullopt;
    }
    if (!status.ok()) {
      *out_status = ExecutionStatus::kErrorUnknown;
      return std::nullopt;
    }
    std::optional<OutputType> output =
        Postprocess(execution_task->GetOutputs());
    *out_status =
        output ? ExecutionStatus::kSuccess : ExecutionStatus::kErrorUnknown;
    return output;
  }

  using BuildModelExecutionTaskCallback =
      typename TFLiteModelExecutor<OutputType, InputType, ModelExecutionTask>::
          BuildModelExecutionTaskCallback;

  BuildModelExecutionTaskCallback GetBuildModelExecutionTaskCallback()
      override {
    return base::BindRepeating(&BaseModelExecutor::BuildModelExecutionTask,
                               num_threads_);
  }

  // Preprocesses |input| into |input_tensors|. Returns true on success.
  virtual bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                          InputType input) = 0;

  // Postprocesses |output_tensors| into the desired |OutputType|, returning
  // std::nullopt on error.
  virtual std::optional<OutputType> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) = 0;

 private:
  static base::expected<std::unique_ptr<ModelExecutionTask>, ExecutionStatus>
  BuildModelExecutionTask(int num_threads, base::File& model_file) {
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine =
        std::make_unique<tflite::task::core::TfLiteEngine>(
            std::make_unique<TFLiteOpResolver>());
#if BUILDFLAG(IS_WIN)
    absl::Status model_load_status =
        tflite_engine->BuildModelFromFileHandle(model_file.GetPlatformFile());
#else
    absl::Status model_load_status =
        tflite_engine->BuildModelFromFileDescriptor(
            model_file.GetPlatformFile());
#endif
    if (!model_load_status.ok()) {
      DLOG(ERROR) << "Failed to load model: " << model_load_status.ToString();
      return base::unexpected(ExecutionStatus::kErrorModelFileNotValid);
    }

    auto compute_settings = tflite::proto::ComputeSettings();
    compute_settings.mutable_tflite_settings()
        ->mutable_cpu_settings()
        ->set_num_threads(num_threads);
    absl::Status interpreter_status =
        tflite_engine->InitInterpreter(compute_settings);
    if (!interpreter_status.ok()) {
      DLOG(ERROR) << "Failed to initialize model interpreter: "
                  << interpreter_status.ToString();
      return base::unexpected(ExecutionStatus::kErrorUnknown);
    }

    return std::move(tflite_engine);
  }

  // -1 tells TFLite to use its own default number of threads.
  int num_threads_ = -1;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_INFERENCE_BASE_MODEL_EXECUTOR_H_
