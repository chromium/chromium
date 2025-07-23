// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv4_handler.h"

#include <memory>

#include "base/feature_list.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/prediction_service/permissions_aiv4_encoder.h"
#include "components/version_info/version_info.h"

namespace permissions {

namespace {
using ModelInput = PermissionsAiv4Encoder::ModelInput;
using ModelOutput = PermissionsAiv4Encoder::ModelOutput;
}  // namespace

PermissionsAiv4Handler::PermissionsAiv4Handler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target,
    RequestType request_type,
    scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner,
    scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
    std::unique_ptr<PermissionsAiv4Encoder> model_executor)
    : ModelHandler<ModelOutput, const ModelInput&>(
          model_provider,
          model_executor_task_runner,
          std::move(model_executor),
          /*model_inference_timeout=*/std::nullopt,
          optimization_target,
          /*model_metadata=*/std::nullopt,
          reply_task_runner) {}

PermissionsAiv4Handler::PermissionsAiv4Handler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target,
    RequestType request_type)
    : PermissionsAiv4Handler(
          model_provider,
          optimization_target,
          request_type,
          /*model_executor_task_runner=*/
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_BLOCKING}),
          /*reply_task_runner-=*/base::SequencedTaskRunner::GetCurrentDefault(),
          /*model_executor=*/
          std::make_unique<PermissionsAiv4Encoder>(request_type)) {}

PermissionsAiv4Handler::~PermissionsAiv4Handler() = default;

void PermissionsAiv4Handler::OnModelUpdated(
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
  }
}

void PermissionsAiv4Handler::ExecuteModel(ExecutionCallback callback,
                                          std::unique_ptr<SkBitmap> snapshot,
                                          std::string rendered_text) {
  if (snapshot.get()) {
    ModelInput input;
    input.snapshot = *snapshot;
    input.rendered_text = rendered_text;
    // TODO(crbug.com/382447738): Add timeout logic.
    ExecuteModelWithInput(std::move(callback), input);
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

}  // namespace permissions
