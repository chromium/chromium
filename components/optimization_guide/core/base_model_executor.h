// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_H_

#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/base_model_executor_helpers.h"
#include "components/optimization_guide/core/execution_status.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/tflite_model_executor.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"

namespace optimization_guide {

// An ModelExecutor that executes models with arbitrary input and output types.
// Note that callers will need to give an implementation of this class to a
// |ModelHandler|, whereas the handle is the actual class that calling code
// would own and call into.
template <class OutputType, class InputType>
class BaseModelExecutor : public TFLiteModelExecutor<OutputType, InputType>,
                          public InferenceDelegate<OutputType, InputType> {
 public:
  using ModelExecutionTask =
      tflite::task::core::BaseTaskApi<OutputType, InputType>;

  BaseModelExecutor() = default;
  ~BaseModelExecutor() override = default;
  BaseModelExecutor(const BaseModelExecutor&) = delete;
  BaseModelExecutor& operator=(const BaseModelExecutor&) = delete;

 public:
  // TFLiteModelExecutor:
  void InitializeAndMoveToExecutionThread(
      std::optional<base::TimeDelta> model_inference_timeout,
      proto::OptimizationTarget optimization_target,
      scoped_refptr<base::SequencedTaskRunner> execution_task_runner,
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner) override {
    num_threads_ = features::OverrideNumThreadsForOptTarget(optimization_target)
                       .value_or(-1);
    TFLiteModelExecutor<OutputType, InputType>::
        InitializeAndMoveToExecutionThread(
            model_inference_timeout, optimization_target, execution_task_runner,
            reply_task_runner);
  }

 protected:
  std::optional<OutputType> Execute(ModelExecutionTask* execution_task,
                                    ExecutionStatus* out_status,
                                    InputType input) override {
    return static_cast<GenericModelExecutionTask<OutputType, InputType>*>(
               execution_task)
        ->Execute(out_status, input);
  }

  base::expected<std::unique_ptr<ModelExecutionTask>, ExecutionStatus>
  BuildModelExecutionTask(base::MemoryMappedFile* model_file) override {
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine =
        std::make_unique<tflite::task::core::TfLiteEngine>(
            std::make_unique<TFLiteOpResolver>());
    absl::Status model_load_status = tflite_engine->BuildModelFromFlatBuffer(
        reinterpret_cast<const char*>(model_file->data()),
        model_file->length());
    if (!model_load_status.ok()) {
      DLOG(ERROR) << "Failed to load model: " << model_load_status.ToString();
      return base::unexpected(ExecutionStatus::kErrorModelFileNotValid);
    }

    auto compute_settings = tflite::proto::ComputeSettings();
    compute_settings.mutable_tflite_settings()
        ->mutable_cpu_settings()
        ->set_num_threads(num_threads_);
    absl::Status interpreter_status =
        tflite_engine->InitInterpreter(compute_settings);
    if (!interpreter_status.ok()) {
      DLOG(ERROR) << "Failed to initialize model interpreter: "
                  << interpreter_status.ToString();
      return base::unexpected(ExecutionStatus::kErrorUnknown);
    }

    return std::make_unique<GenericModelExecutionTask<OutputType, InputType>>(
        std::move(tflite_engine), this);
  }

  // InferenceDelegate:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  InputType input) override = 0;
  std::optional<OutputType> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override = 0;

 private:
  // -1 tells TFLite to use its own default number of threads.
  int num_threads_ = -1;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_H_
