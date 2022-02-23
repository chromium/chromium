// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_H_

#include "components/optimization_guide/core/base_model_executor_helpers.h"
#include "components/optimization_guide/core/execution_status.h"
#include "components/optimization_guide/core/tflite_model_executor.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"

namespace optimization_guide {

// An ModelExecutor that executes models with arbitrary
// input and output types. Note that callers will need to give an implementation
// of this class to a |ModelHandler|, whereas the
// handle is the actual class that calling code would own and call into.
template <class OutputType, class... InputTypes>
class BaseModelExecutor : public TFLiteModelExecutor<OutputType, InputTypes...>,
                          public InferenceDelegate<OutputType, InputTypes...> {
 public:
  using ModelExecutionTask =
      tflite::task::core::BaseTaskApi<OutputType, InputTypes...>;

  BaseModelExecutor() = default;
  ~BaseModelExecutor() override = default;
  BaseModelExecutor(const BaseModelExecutor&) = delete;
  BaseModelExecutor& operator=(const BaseModelExecutor&) = delete;

 protected:
  absl::optional<OutputType> Execute(ModelExecutionTask* execution_task,
                                     ExecutionStatus* out_status,
                                     InputTypes... args) override {
    return static_cast<GenericModelExecutionTask<OutputType, InputTypes...>*>(
               execution_task)
        ->Execute(out_status, args...);
  }

  std::unique_ptr<ModelExecutionTask> BuildModelExecutionTask(
      base::MemoryMappedFile* model_file,
      ExecutionStatus* out_status) override {
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine =
        std::make_unique<tflite::task::core::TfLiteEngine>(
            std::make_unique<TFLiteOpResolver>());
    absl::Status model_load_status = tflite_engine->BuildModelFromFlatBuffer(
        reinterpret_cast<const char*>(model_file->data()),
        model_file->length());
    if (!model_load_status.ok()) {
      DLOG(ERROR) << "Failed to load model: " << model_load_status.ToString();
      *out_status = ExecutionStatus::kErrorModelFileNotValid;
      return nullptr;
    }

    absl::Status interpreter_status =
        tflite_engine->InitInterpreter(tflite::proto::ComputeSettings(),
                                       /*num_threads=*/1);
    if (!interpreter_status.ok()) {
      DLOG(ERROR) << "Failed to initialize model interpreter: "
                  << interpreter_status.ToString();
      *out_status = ExecutionStatus::kErrorUnknown;
      return nullptr;
    }

    return std::make_unique<
        GenericModelExecutionTask<OutputType, InputTypes...>>(
        std::move(tflite_engine), this);
  }

  // InferenceDelegate:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  InputTypes... input) override = 0;
  absl::optional<OutputType> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_H_
