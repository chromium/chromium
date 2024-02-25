// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_VALIDATOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_VALIDATOR_H_

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/base_model_executor.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// Handler for loading and validating a model.
class ModelValidatorHandler
    : public ModelHandler<float, const std::vector<float>&> {
 public:
  ModelValidatorHandler(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~ModelValidatorHandler() override;

  // Disallow copy/assign.
  ModelValidatorHandler(const ModelValidatorHandler&) = delete;
  ModelValidatorHandler& operator=(const ModelValidatorHandler&) = delete;

 private:
  // ModelValidatorHandler:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const ModelInfo> model_info) override;

  // Invoked when the model has finished executing.
  void OnModelExecutionComplete(const std::optional<float>& output);

  base::WeakPtrFactory<ModelValidatorHandler> weak_ptr_factory_{this};
};

// Executor where the model loading and validation happens in the background
// thread. This is owned by ModelValidatorHandler.
class ModelValidatorExecutor
    : public BaseModelExecutor<float, const std::vector<float>&> {
 public:
  ModelValidatorExecutor();
  ~ModelValidatorExecutor() override;

  // Disallow copy/assign.
  ModelValidatorExecutor(const ModelValidatorExecutor&) = delete;
  ModelValidatorExecutor& operator=(const ModelValidatorExecutor&) = delete;

 protected:
  // BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const std::vector<float>& input) override;
  std::optional<float> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_VALIDATOR_H_
