// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv3_handler.h"

#include <memory>

#include "base/feature_list.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
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
    scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner,
    std::unique_ptr<PermissionsAiv3Encoder> model_executor)
    : ModelHandler<ModelOutput, const ModelInput&>(
          model_provider,
          model_executor_task_runner,
          std::move(model_executor),
          /*model_inference_timeout=*/std::nullopt,
          optimization_target,
          /*model_metadata=*/std::nullopt) {}

PermissionsAiv3Handler::PermissionsAiv3Handler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target,
    RequestType request_type)
    : PermissionsAiv3Handler(
          model_provider,
          optimization_target,
          request_type,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
          std::make_unique<PermissionsAiv3Encoder>(request_type)) {}

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
    // TODO(crbug.com/405095664): Parse ModelMetadata as soon as we have it.
  }
}

void PermissionsAiv3Handler::ExecuteModel(
    ExecutionCallback callback,
    std::unique_ptr<ModelInput> snapshot) {
  if (snapshot.get()) {
    ExecuteModelWithInput(std::move(callback), *snapshot);
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

}  // namespace permissions
