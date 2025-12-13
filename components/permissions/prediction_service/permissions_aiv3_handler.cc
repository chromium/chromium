// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv3_handler.h"

#include <memory>

#include "base/feature_list.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/prediction_service/permissions_aiv3_executor.h"
#include "components/version_info/version_info.h"

namespace permissions {

namespace {
using ModelInput = PermissionsAiv3Executor::ModelInput;
using ModelOutput = PermissionsAiv3Executor::ModelOutput;

// This is the timeout for the model execution. If the model execution takes
// longer than this timeout, the callback will be called with a nullopt result.
constexpr auto kModelExecutionTimeoutSeconds =
    base::Seconds(PermissionsAiv3Handler::kModelExecutionTimeout);
}  // namespace

PermissionsAiv3Handler::PermissionsAiv3Handler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target,
    RequestType request_type,
    std::unique_ptr<PermissionsAiv3Executor> model_executor,
    scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner,
    scoped_refptr<base::SequencedTaskRunner> reply_task_runner)
    : ModelHandler<ModelOutput, const ModelInput&>(
          model_provider,
          model_executor_task_runner,
          std::move(model_executor),
          /*model_inference_timeout=*/std::nullopt,
          optimization_target,
          /*model_metadata=*/std::nullopt,
          /*model_loading_task_runner=*/nullptr,
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
          std::make_unique<PermissionsAiv3Executor>(request_type)) {}

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
                                          ModelInput model_input) {
  DCHECK(!model_input.snapshot.drawsNothing());
  VLOG(1) << "PermissionsAiv3Handler::ExecuteModel";
  base::UmaHistogramBoolean("Permissions.AIv3.ModelExecutionAlreadyInProgress",
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
  } else {
    VLOG(1) << "[PermissionsAIv3] ExecuteModel: Execution not in "
               "progress. Starting execution.";
  }
  is_execution_in_progress_ = true;
  is_callback_valid_ = true;

  model_input.metadata = model_metadata_;

  current_callback_ = std::move(callback);
  ExecutionCallback on_complete_callback =
      base::BindOnce(&PermissionsAiv3Handler::OnModelExecutionComplete,
                     weak_factory_.GetWeakPtr());

  ExecuteModelWithInput(std::move(on_complete_callback), model_input);

  // In parallel with the model execution, we will start a timer that will
  // call `OnModelExecutionTimeout` with a nullopt result if the model
  // execution takes longer than the timeout.
  timeout_timer_.Start(
      FROM_HERE, kModelExecutionTimeoutSeconds,
      base::BindOnce(&PermissionsAiv3Handler::OnModelExecutionTimeout,
                     weak_factory_.GetWeakPtr(), std::nullopt));
}

void PermissionsAiv3Handler::OnModelExecutionTimeout(
    const std::optional<PermissionRequestRelevance>& relevance) {
  VLOG(1) << "[PermissionsAIv3] OnModelExecutionTimeout: Model execution took "
             "longer than the timeout. Returning empty response.";
  base::UmaHistogramBoolean("Permissions.AIv3.ModelExecutionTimeout", true);
  std::move(current_callback_).Run(std::nullopt);
}

void PermissionsAiv3Handler::OnModelExecutionComplete(
    const std::optional<PermissionRequestRelevance>& relevance) {
  VLOG(1) << "[PermissionsAIv3] OnModelExecutionComplete: Model execution "
             "completed. Returning relevance: "
          << (relevance.has_value() ? static_cast<int>(relevance.value()) : -1);
  timeout_timer_.Stop();
  is_execution_in_progress_ = false;

  if (!current_callback_) {
    VLOG(1) << "[PermissionsAIv3] OnModelExecutionComplete: Callback was "
               "replaced. Ignoring the result.";
    // The callback was executed in `OnModelExecutionTimeout` before the model
    // execution completed.
    // The timeout logic does not reset
    // `is_execution_in_progress_` flag, so in the case of a new request we will
    // not save a new callback to avoid delivering a stale model execution
    // result to a new permission prompt.
    return;
  }

  if (is_callback_valid_) {
    VLOG(1) << "[PermissionsAIv3] OnModelExecutionComplete: Callback is "
               "valid. Delivering relevance: "
            << (relevance.has_value() ? static_cast<int>(relevance.value())
                                      : -1);
    std::move(current_callback_).Run(relevance);
  } else {
    VLOG(1) << "[PermissionsAIv3] OnModelExecutionComplete: Callback is no "
               "longer valid. Ignoring the result.";
    // The callback is no longer valid because a new execution was requested
    // while the previous one was still in progress. We will return an empty
    // response to the callback because there is no UI to which the relevance
    // can be applied.
    std::move(current_callback_).Run(std::nullopt);
  }
}

}  // namespace permissions
