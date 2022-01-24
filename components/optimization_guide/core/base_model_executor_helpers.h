// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_HELPERS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_HELPERS_H_

#include <memory>
#include <vector>

#include "base/check.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"

namespace optimization_guide {

template <class OutputType, class... InputTypes>
class InferenceDelegate {
 public:
  // Preprocesses |args| into |input_tensors|.
  virtual absl::Status Preprocess(
      const std::vector<TfLiteTensor*>& input_tensors,
      InputTypes... args) = 0;

  // Postprocesses |output_tensors| into the desired |OutputType|.
  virtual OutputType Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) = 0;
};

template <class OutputType, class... InputTypes>
class GenericModelExecutionTask
    : public tflite::task::core::BaseTaskApi<OutputType, InputTypes...> {
 public:
  GenericModelExecutionTask(
      std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine,
      InferenceDelegate<OutputType, InputTypes...>* delegate)
      : tflite::task::core::BaseTaskApi<OutputType, InputTypes...>(
            std::move(tflite_engine)),
        delegate_(delegate) {
    DCHECK(delegate_);
  }
  ~GenericModelExecutionTask() override = default;

  // Executes the model using |args| and returns the output if the model was
  // executed successfully.
  absl::optional<OutputType> Execute(InputTypes... args) {
    tflite::support::StatusOr<OutputType> maybe_output = this->Infer(args...);
    if (maybe_output.ok())
      return maybe_output.value();
    return absl::nullopt;
  }

 protected:
  // BaseTaskApi:
  absl::Status Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                          InputTypes... args) override {
    return delegate_->Preprocess(input_tensors, args...);
  }
  tflite::support::StatusOr<OutputType> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors,
      InputTypes... api_inputs) override {
    return delegate_->Postprocess(output_tensors);
  }

 private:
  // Guaranteed to outlive this.
  InferenceDelegate<OutputType, InputTypes...>* delegate_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_HELPERS_H_
