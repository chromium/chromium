// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_BASE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_BASE_MODEL_EXECUTOR_H_

#include "components/optimization_guide/content/browser/base_model_executor_helpers.h"
#include "components/optimization_guide/content/browser/optimization_target_model_executor.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/tflite-support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"

namespace optimization_guide {

// An OptimizationTargetModelExecutor that executes models with arbitrary input
// and output types.
template <class OutputType, class... InputTypes>
class BaseModelExecutor
    : public OptimizationTargetModelExecutor<OutputType, InputTypes...>,
      public InferenceDelegate<OutputType, InputTypes...> {
 public:
  using ModelExecutionTask =
      tflite::task::core::BaseTaskApi<OutputType, InputTypes...>;

  BaseModelExecutor(OptimizationGuideDecider* decider,
                    proto::OptimizationTarget optimization_target,
                    const base::Optional<proto::Any>& model_metadata,
                    const scoped_refptr<base::SequencedTaskRunner>&
                        model_execution_task_runner)
      : OptimizationTargetModelExecutor<OutputType, InputTypes...>(
            decider,
            optimization_target,
            model_metadata,
            model_execution_task_runner) {}
  ~BaseModelExecutor() override = default;
  BaseModelExecutor(const BaseModelExecutor&) = delete;
  BaseModelExecutor& operator=(const BaseModelExecutor&) = delete;

 protected:
  base::Optional<OutputType> Execute(ModelExecutionTask* execution_task,
                                     InputTypes... args) override {
    return static_cast<GenericModelExecutionTask<OutputType, InputTypes...>*>(
               execution_task)
        ->Execute(args...);
  }

  std::unique_ptr<ModelExecutionTask> BuildModelExecutionTask(
      base::MemoryMappedFile* model_file) override {
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine =
        std::make_unique<tflite::task::core::TfLiteEngine>(
            std::make_unique<TFLiteOpResolver>());
    absl::Status model_load_status = tflite_engine->BuildModelFromFlatBuffer(
        reinterpret_cast<const char*>(model_file->data()),
        model_file->length());
    if (!model_load_status.ok()) {
      DLOG(ERROR) << "Failed to load model: " << model_load_status.ToString();
      return nullptr;
    }

    absl::Status interpreter_status =
        tflite_engine->InitInterpreter(tflite::proto::ComputeSettings(),
                                       /*num_threads=*/1);
    if (!interpreter_status.ok()) {
      DLOG(ERROR) << "Failed to initialize model interpreter: "
                  << interpreter_status.ToString();
      return nullptr;
    }

    return std::make_unique<
        GenericModelExecutionTask<OutputType, InputTypes...>>(
        std::move(tflite_engine), this);
  }

  // InferenceDelegate:
  void Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  InputTypes... input) override = 0;
  OutputType Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_BASE_MODEL_EXECUTOR_H_
