// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_validator.h"

#include <memory>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace optimization_guide {

ModelValidatorHandler::ModelValidatorHandler(
    OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : ModelHandler<float, const std::vector<float>&>(
          model_provider,
          background_task_runner,
          std::make_unique<ModelValidatorExecutor>(),
          /*model_inference_timeout=*/std::nullopt,
          proto::OPTIMIZATION_TARGET_MODEL_VALIDATION,
          /*model_metadata=*/std::nullopt) {}

ModelValidatorHandler::~ModelValidatorHandler() = default;

void ModelValidatorHandler::OnModelExecutionComplete(
    const std::optional<float>& output) {
  // Delete |this| since the model load completed successfully or failed.
  delete this;
}

void ModelValidatorHandler::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  // First invoke parent to update internal status.
  optimization_guide::ModelHandler<
      float, const std::vector<float>&>::OnModelUpdated(optimization_target,
                                                        model_info);
  // The parent class should always set the model availability to true after
  // having received an updated model.
  DCHECK(ModelAvailable());

  // Try executing the model which will wait until the model is loaded, execute
  // it, and callback when execution is finished.
  ExecuteModelWithInput(
      base::BindOnce(&ModelValidatorHandler::OnModelExecutionComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      std::vector<float>());
}

ModelValidatorExecutor::ModelValidatorExecutor() = default;

ModelValidatorExecutor::~ModelValidatorExecutor() = default;

bool ModelValidatorExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const std::vector<float>& input) {
  // Return error so that actual model execution does not happen.
  return false;
}

std::optional<float> ModelValidatorExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  std::vector<float> data;
  absl::Status status =
      tflite::task::core::PopulateVector<float>(output_tensors[0], &data);
  if (!status.ok()) {
    NOTREACHED_IN_MIGRATION();
    return std::nullopt;
  }
  return data[0];
}

}  // namespace optimization_guide
