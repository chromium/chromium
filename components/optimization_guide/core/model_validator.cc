// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_validator.h"

#include <memory>
#include <vector>

#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/bert_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "third_party/tflite-support/src/tensorflow_lite_support/cc/task/core/task_utils.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"

namespace optimization_guide {

ModelValidatorHandler::ModelValidatorHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    optimization_guide::proto::OptimizationTarget optimization_target)
    : optimization_guide::ModelHandler<float, const std::vector<float>&>(
          model_provider,
          background_task_runner,
          std::make_unique<ModelValidatorExecutor>(),
          optimization_target,
          absl::nullopt /*model_metadata=*/) {}

ModelValidatorHandler::~ModelValidatorHandler() = default;

void ModelValidatorHandler::OnModelExecutionTaskLoaded() {
  // Delete |this| since the model load completed successfully or failed.
  delete this;
}

ModelValidatorExecutor::ModelValidatorExecutor() = default;

ModelValidatorExecutor::~ModelValidatorExecutor() = default;

void ModelValidatorExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const std::vector<float>& input) {}

float ModelValidatorExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  std::vector<float> data;
  tflite::task::core::PopulateVector<float>(output_tensors[0], &data);
  return data[0];
}

void DoValidateModel(
    OptimizationGuideModelProvider* optimization_guide_model_provider) {
  if (!switches::ShouldValidateModel())
    return;

  // Create the validator object which will get destroyed when the model load is
  // complete.
  new ModelValidatorHandler(
      optimization_guide_model_provider,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      proto::OPTIMIZATION_TARGET_MODEL_VALIDATION);
}

}  // namespace optimization_guide
