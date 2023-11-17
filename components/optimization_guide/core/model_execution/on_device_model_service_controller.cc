// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/model_execution/on_device_session.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

OnDeviceModelServiceController::OnDeviceModelServiceController() = default;
OnDeviceModelServiceController::~OnDeviceModelServiceController() = default;

void OnDeviceModelServiceController::Init(
    const base::FilePath& model_path,
    std::unique_ptr<OnDeviceModelExecutionConfigInterpreter>
        config_interpreter) {
  CHECK(model_path_.empty());
  model_path_ = model_path;
  config_interpreter_ = std::move(config_interpreter);
  config_interpreter_->UpdateConfigWithFileDir(model_path_);
}

std::unique_ptr<OptimizationGuideModelExecutor::Session>
OnDeviceModelServiceController::StartSession(
    proto::ModelExecutionFeature feature) {
  if (!base::FeatureList::IsEnabled(
          features::kOptimizationGuideOnDeviceModel) ||
      !config_interpreter_->HasConfigForFeature(feature)) {
    return nullptr;
  }
  return std::make_unique<OnDeviceSession>(
      base::BindRepeating(&OnDeviceModelServiceController::StartMojoSession,
                          weak_ptr_factory_.GetWeakPtr()),
      feature, config_interpreter_.get());
}

void OnDeviceModelServiceController::StartMojoSession(
    mojo::PendingReceiver<on_device_model::mojom::Session> session) {
  if (!model_remote_) {
    LaunchService();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&on_device_model::LoadModelAssets, model_path_),
        base::BindOnce(&OnDeviceModelServiceController::OnModelAssetsLoaded,
                       weak_ptr_factory_.GetWeakPtr(),
                       model_remote_.BindNewPipeAndPassReceiver()));
    model_remote_.reset_on_disconnect();
  }
  model_remote_->StartSession(std::move(session));
}

void OnDeviceModelServiceController::OnModelAssetsLoaded(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    on_device_model::ModelAssets assets) {
  if (!service_remote_) {
    // Close the files on a background thread.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::DoNothingWithBoundArgs(std::move(assets)));
    return;
  }
  // TODO(b/302402959): Choose max_tokens based on device.
  service_remote_->LoadModel(
      on_device_model::mojom::LoadModelParams::New(std::move(assets),
                                                   /*max_tokens=*/4096),
      std::move(model),
      base::BindOnce(&OnDeviceModelServiceController::OnLoadModelResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OnDeviceModelServiceController::OnLoadModelResult(
    on_device_model::mojom::LoadModelResult result) {
  if (result != on_device_model::mojom::LoadModelResult::kSuccess) {
    // TODO(b/302402576): Add error handling.
    LOG(ERROR) << "Failed loading model, code=" << static_cast<int>(result);
  }
}

}  // namespace optimization_guide
