// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_SIGNATURE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_SIGNATURE_MODEL_EXECUTOR_H_

#include <optional>

#include "base/notreached.h"
#include "components/optimization_guide/core/base_model_executor.h"
#include "components/optimization_guide/core/execution_status.h"
#include "third_party/tflite/src/tensorflow/lite/signature_runner.h"

namespace optimization_guide {

// An ModelExecutor that uses TfLite Signatures for inference.
// This works exactly like the BaseModelExecutor except that the input and
// output tensors are available in the form of a map instead of a vector.
template <class OutputType, class InputType>
class SignatureModelExecutor : public BaseModelExecutor<OutputType, InputType> {
 public:
  using ModelExecutionTask =
      tflite::task::core::BaseTaskApi<OutputType, InputType>;

  SignatureModelExecutor() = default;
  ~SignatureModelExecutor() override = default;
  SignatureModelExecutor(const SignatureModelExecutor&) = delete;
  SignatureModelExecutor& operator=(const SignatureModelExecutor&) = delete;

 protected:
  virtual bool Preprocess(
      const std::map<std::string, TfLiteTensor*>& input_tensors_map,
      InputType input) = 0;
  virtual std::optional<OutputType> Postprocess(
      const std::map<std::string, const TfLiteTensor*>& output_tensors_map) = 0;
  virtual const char* GetSignature() = 0;

  // optimization_guide::BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const InputType& input) override {
    NOTREACHED();
    return false;
  }

  std::optional<OutputType> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override {
    NOTREACHED();
    return std::nullopt;
  }

  std::optional<OutputType> Execute(ModelExecutionTask* execution_task,
                                    ExecutionStatus* out_status,
                                    InputType input) override {
    tflite::SignatureRunner* signature_runner =
        static_cast<GenericModelExecutionTask<OutputType, InputType>*>(
            execution_task)
            ->GetSignatureRunner(GetSignature());
    Preprocess(GetTensorsMappedByInputNames(signature_runner), input);
    TfLiteStatus status = signature_runner->Invoke();
    if (status == kTfLiteCancelled) {
      *out_status = ExecutionStatus::kErrorCancelled;
      return std::nullopt;
    }
    if (status != kTfLiteOk) {
      *out_status = ExecutionStatus::kErrorUnknown;
      return std::nullopt;
    }
    *out_status = ExecutionStatus::kSuccess;
    return Postprocess(GetTensorsMappedByOutputNames(signature_runner));
  }

 private:
  std::map<std::string, TfLiteTensor*> GetTensorsMappedByInputNames(
      tflite::SignatureRunner* signature_runner) {
    auto input_keys = signature_runner->input_names();
    std::map<std::string, TfLiteTensor*> input_tensors;
    for (const char* input : input_keys) {
      input_tensors[input] = signature_runner->input_tensor(input);
    }
    return input_tensors;
  }

  std::map<std::string, const TfLiteTensor*> GetTensorsMappedByOutputNames(
      tflite::SignatureRunner* signature_runner) {
    auto output_keys = signature_runner->output_names();
    std::map<std::string, const TfLiteTensor*> output_tensors;
    for (const char* output : output_keys) {
      const TfLiteTensor* output_tensor =
          signature_runner->output_tensor(output);
      output_tensors[output] = output_tensor;
    }
    return output_tensors;
  }
};
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_SIGNATURE_MODEL_EXECUTOR_H_
