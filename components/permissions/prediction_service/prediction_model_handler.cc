// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_model_handler.h"

#include <memory>

#include "base/feature_list.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/prediction_service/prediction_signature_model_executor.h"
#include "components/version_info/version_info.h"

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
          GetModelHandshakeProto()) {}

PredictionModelHandler::~PredictionModelHandler() = default;

void PredictionModelHandler::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  // First invoke parent to update internal status.
  optimization_guide::ModelHandler<
      GeneratePredictionsResponse,
      const PredictionModelExecutorInput&>::OnModelUpdated(optimization_target,
                                                           model_info);
  if (model_info.has_value()) {
    // The parent class should always set the model availability to true after
    // having received an updated model.
    DCHECK(ModelAvailable());
    prediction_model_metadata_ = ParsedSupportedFeaturesForLoadedModel<
        WebPermissionPredictionsModelMetadata>();
  }

  model_load_run_loop_.Quit();
}

void PredictionModelHandler::ExecuteModelWithMetadata(
    ExecutionCallback callback,
    std::unique_ptr<GeneratePredictionsRequest> proto_request) {
  // Check that the right model is served before execution. Only v2 models can
  // be used with the Signature runner
  const bool is_model_mismatch =
      base::FeatureList::IsEnabled(features::kCpssUseTfliteSignatureRunner) &&
      prediction_model_metadata_->version() != 2;
  base::UmaHistogramBoolean(
      "Permissions.PredictionService.SignatureModel.Mismatch",
      is_model_mismatch);
  if (is_model_mismatch) {
    return;
  }
  PredictionModelExecutorInput input;
  input.request = *proto_request;
  input.metadata = prediction_model_metadata_;
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

std::optional<optimization_guide::proto::Any>
PredictionModelHandler::GetModelHandshakeProto() {
  if (base::FeatureList::IsEnabled(features::kCpssUseTfliteSignatureRunner)) {
    const char url[] =
        "type.googleapis.com/"
        "google.privacy.webpermissionpredictions.v1."
        "WebPermissionPredictionsClientInfo";
    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(url);
    WebPermissionPredictionsClientInfo model_handshake_info;
    model_handshake_info.set_milestone(
        version_info::GetMajorVersionNumberAsInt());
    model_handshake_info.SerializeToString(any_metadata.mutable_value());
    return any_metadata;
  }
  return std::nullopt;
}

std::optional<float> PredictionModelHandler::HoldBackProbability() {
  if (!prediction_model_metadata_.has_value()) {
    return std::nullopt;
  }
  return prediction_model_metadata_->holdback_probability();
}

}  // namespace permissions
