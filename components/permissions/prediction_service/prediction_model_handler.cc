// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_model_handler.h"

#include <memory>

#include "components/optimization_guide/core/optimization_guide_model_provider.h"

namespace permissions {

PredictionModelHandler::PredictionModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target)
    : ModelHandler<GeneratePredictionsResponse,
                   const GeneratePredictionsRequest&>(
          model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
          std::make_unique<PredictionModelExecutor>(),
          /*model_inference_timeout=*/absl::nullopt,
          optimization_target,
          absl::nullopt) {}

void PredictionModelHandler::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const optimization_guide::ModelInfo& model_info) {
  // First invoke parent to update internal status.
  optimization_guide::ModelHandler<
      GeneratePredictionsResponse,
      const GeneratePredictionsRequest&>::OnModelUpdated(optimization_target,
                                                         model_info);
  model_load_run_loop_.Quit();
}

void PredictionModelHandler::WaitForModelLoadForTesting() {
  model_load_run_loop_.Run();
}

}  // namespace permissions
