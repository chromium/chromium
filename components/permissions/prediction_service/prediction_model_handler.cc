// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_model_handler.h"

#include <memory>

#include "base/feature_list.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/prediction_service/prediction_signature_model_executor.h"

namespace permissions {

PredictionModelHandler::PredictionModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target)
    : ModelHandler<GeneratePredictionsResponse,
                   const PredictionModelExecutorInput&>(
          model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
          GetExecutor(),
          /*model_inference_timeout=*/std::nullopt,
          optimization_target,
          std::nullopt) {}

void PredictionModelHandler::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  // First invoke parent to update internal status.
  optimization_guide::ModelHandler<
      GeneratePredictionsResponse,
      const PredictionModelExecutorInput&>::OnModelUpdated(optimization_target,
                                                           model_info);
  model_load_run_loop_.Quit();
}

std::optional<WebPermissionPredictionsModelMetadata>
PredictionModelHandler::GetModelMetaData() {
  std::optional<WebPermissionPredictionsModelMetadata> metadata =
      ParsedSupportedFeaturesForLoadedModel<
          WebPermissionPredictionsModelMetadata>();
  return metadata;
}

void PredictionModelHandler::ExecuteModelWithMetadata(
    ExecutionCallback callback,
    std::unique_ptr<GeneratePredictionsRequest> proto_request) {
  PredictionModelExecutorInput input;
  input.request = *proto_request;
  input.metadata = GetModelMetaData();
  ExecuteModelWithInput(std::move(callback), input);
}

void PredictionModelHandler::WaitForModelLoadForTesting() {
  model_load_run_loop_.Run();
}

std::unique_ptr<
    optimization_guide::ModelExecutor<GeneratePredictionsResponse,
                                      const PredictionModelExecutorInput&>>
PredictionModelHandler::GetExecutor() {
  if (base::FeatureList::IsEnabled(features::kCpssUseTfliteSignatureRunner)) {
    return std::make_unique<PredictionSignatureModelExecutor>();
  }
  return std::make_unique<PredictionModelExecutor>();
}

}  // namespace permissions
