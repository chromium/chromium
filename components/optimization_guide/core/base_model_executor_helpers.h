// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_HELPERS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_HELPERS_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/execution_status.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"

namespace optimization_guide {

template <class OutputType, class... InputTypes>
class InferenceDelegate {
 public:
  // Preprocesses |args| into |input_tensors|. Returns true on success.
  virtual bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                          InputTypes... args) = 0;

  // Postprocesses |output_tensors| into the desired |OutputType|, returning
  // std::nullopt on error.
  virtual std::optional<OutputType> Postprocess(
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
  std::optional<OutputType> Execute(ExecutionStatus* out_status,
                                    InputTypes... args) {
    tflite::support::StatusOr<OutputType> maybe_output = this->Infer(args...);
    if (absl::IsCancelled(maybe_output.status())) {
      *out_status = ExecutionStatus::kErrorCancelled;
      return std::nullopt;
    }
    if (!maybe_output.ok()) {
      *out_status = ExecutionStatus::kErrorUnknown;
      return std::nullopt;
    }
    *out_status = ExecutionStatus::kSuccess;
    return maybe_output.value();
  }

  tflite::impl::SignatureRunner* GetSignatureRunner(const char* signature_key) {
    return this->GetTfLiteEngine()
        ->interpreter_wrapper()
        ->get()
        ->GetSignatureRunner(signature_key);
  }

 protected:
  // BaseTaskApi:
  absl::Status Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                          InputTypes... args) override {
    bool success = delegate_->Preprocess(input_tensors, args...);
    if (success) {
      return absl::OkStatus();
    }
    return absl::InternalError(
        "error during preprocessing. See stderr for more information if "
        "available");
  }
  tflite::support::StatusOr<OutputType> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors,
      InputTypes... api_inputs) override {
    std::optional<OutputType> output = delegate_->Postprocess(output_tensors);
    if (!output) {
      return absl::InternalError(
          "error during postprocessing. See stderr for more infomation if "
          "available");
    }
    return *output;
  }

 private:
  // Guaranteed to outlive this.
  raw_ptr<InferenceDelegate<OutputType, InputTypes...>> delegate_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_BASE_MODEL_EXECUTOR_HELPERS_H_
