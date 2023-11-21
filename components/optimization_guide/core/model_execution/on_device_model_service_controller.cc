// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/model_execution/on_device_session.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

OnDeviceModelServiceController::OnDeviceModelServiceController(
    std::unique_ptr<OnDeviceModelAccessController> access_controller)
    : access_controller_(std::move(access_controller)) {}

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

void OnDeviceModelServiceController::Init() {
  auto model_path_override_switch =
      switches::GetOnDeviceModelExecutionOverride();
  if (model_path_override_switch) {
    auto file_path = StringToFilePath(*model_path_override_switch);
    if (file_path) {
      Init(*file_path,
           std::make_unique<OnDeviceModelExecutionConfigInterpreter>());
    }
  }
}

std::unique_ptr<OptimizationGuideModelExecutor::Session>
OnDeviceModelServiceController::StartSession(
    proto::ModelExecutionFeature feature) {
  if (!base::FeatureList::IsEnabled(
          features::kOptimizationGuideOnDeviceModel) ||
      !config_interpreter_->HasConfigForFeature(feature) ||
      !access_controller_->ShouldStartNewSession()) {
    return nullptr;
  }
  return std::make_unique<OnDeviceSession>(
      base::BindRepeating(&OnDeviceModelServiceController::StartMojoSession,
                          weak_ptr_factory_.GetWeakPtr()),
      feature, config_interpreter_.get(), weak_ptr_factory_.GetWeakPtr());
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
    model_remote_.set_disconnect_handler(
        base::BindOnce(&OnDeviceModelServiceController::OnDisconnected,
                       base::Unretained(this)));
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

void OnDeviceModelServiceController::OnResponseCompleted(
    base::PassKey<OnDeviceSession>,
    OnDeviceSession& session) {
  access_controller_->OnResponseCompleted();
}

void OnDeviceModelServiceController::OnLoadModelResult(
    on_device_model::mojom::LoadModelResult result) {
  switch (result) {
    case on_device_model::mojom::LoadModelResult::kGpuBlocked:
      access_controller_->OnGpuBlocked();
      model_remote_.reset();
      break;
    case on_device_model::mojom::LoadModelResult::kSuccess:
      break;
    case on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary:
      break;
  }
}

void OnDeviceModelServiceController::OnDisconnected() {
  model_remote_.reset();
  access_controller_->OnDisconnectedFromRemote();
}

}  // namespace optimization_guide
