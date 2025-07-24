// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv3_handler.h"

#include <memory>

#include "base/feature_list.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/prediction_service/permissions_aiv3_encoder.h"
#include "components/version_info/version_info.h"

namespace permissions {

namespace {
using ModelInput = PermissionsAiv3Encoder::ModelInput;
using ModelOutput = PermissionsAiv3Encoder::ModelOutput;
}  // namespace

PermissionsAiv3Handler::PermissionsAiv3Handler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target,
    RequestType request_type,
    std::unique_ptr<PermissionsAiv3Encoder> model_executor,
    scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner,
    scoped_refptr<base::SequencedTaskRunner> reply_task_runner)
    : ModelHandler<ModelOutput, const ModelInput&>(
          model_provider,
          model_executor_task_runner,
          std::move(model_executor),
          /*model_inference_timeout=*/std::nullopt,
          optimization_target,
          /*model_metadata=*/std::nullopt,
          reply_task_runner) {}

PermissionsAiv3Handler::PermissionsAiv3Handler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target,
    RequestType request_type)
    : PermissionsAiv3Handler(
          model_provider,
          optimization_target,
          request_type,
          /*model_executor=*/
          std::make_unique<PermissionsAiv3Encoder>(request_type)) {}

PermissionsAiv3Handler::~PermissionsAiv3Handler() = default;

void PermissionsAiv3Handler::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  // First invoke parent to update internal status.
  optimization_guide::ModelHandler<
      ModelOutput, const ModelInput&>::OnModelUpdated(optimization_target,
                                                      model_info);
  if (model_info.has_value()) {
    // The parent class should always set the model availability to true after
    // having received an updated model.
    DCHECK(ModelAvailable());
    model_metadata_ =
        ParsedSupportedFeaturesForLoadedModel<PermissionsAiv3ModelMetadata>();
  }
}

void PermissionsAiv3Handler::ExecuteModel(ExecutionCallback callback,
                                          std::unique_ptr<SkBitmap> snapshot) {
  if (snapshot.get()) {
    base::UmaHistogramBoolean(
        "Permissions.AIv3.ModelExecutionAlreadyInProgress",
        is_execution_in_progress_);
    // If an execution is already in progress, there is no way to cancel it and
    // we cannot wait until it is done because this will add extra latency, so
    // we will return an empty response to the callback.
    if (is_execution_in_progress_) {
      VLOG(1) << "[PermissionsAIv3] ExecuteModel: Execution already in "
                 "progress. Returning empty response.";
      // The callback is no longer valid because a new execution was requested
      // while the previous one was still in progress.
      is_callback_valid_ = false;
      std::move(callback).Run(std::nullopt);
      return;
    }
    is_execution_in_progress_ = true;
    is_callback_valid_ = true;

    ModelInput input;
    input.snapshot = *snapshot;
    input.metadata = model_metadata_;

    ExecutionCallback on_complete_callback =
        base::BindOnce(&PermissionsAiv3Handler::OnModelExecutionComplete,
                       weak_factory_.GetWeakPtr(), std::move(callback));

    ExecuteModelWithInput(std::move(on_complete_callback), input);
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void PermissionsAiv3Handler::OnModelExecutionComplete(
    ExecutionCallback original_callback,
    const std::optional<PermissionRequestRelevance>& relevance) {
  is_execution_in_progress_ = false;

  if (is_callback_valid_) {
    std::move(original_callback).Run(relevance);
  } else {
    VLOG(1) << "[PermissionsAIv3] OnModelExecutionComplete: Callback is no "
               "longer valid. Ignoring the result.";
    // The callback is no longer valid because a new execution was requested
    // while the previous one was still in progress. We will return an empty
    // response to the callback because there is no UI to which the relevance
    // can be applied.
    std::move(original_callback).Run(std::nullopt);
  }
}

}  // namespace permissions
