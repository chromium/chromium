// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

OnDeviceModelServiceController::OnDeviceModelServiceController() = default;
OnDeviceModelServiceController::~OnDeviceModelServiceController() = default;

void OnDeviceModelServiceController::Init(const base::FilePath& model_path) {
  CHECK(model_path_.empty());
  model_path_ = model_path;
}

void OnDeviceModelServiceController::Execute(
    std::string_view input,
    mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
        streaming_responder) {
  if (model_remote_) {
    model_remote_->StartSession(session_remote_.BindNewPipeAndPassReceiver());
    session_remote_->Execute(
        on_device_model::mojom::InputOptions::New(std::string(input),
                                                  std::nullopt, std::nullopt),
        std::move(streaming_responder));
    return;
  }
  LaunchService();
  // TODO(b/302402959): Choose max_tokens based on device.
  service_remote_->LoadModel(
      on_device_model::mojom::LoadModelParams::New(
          on_device_model::LoadModelAssets(model_path_), 4096),
      model_remote_.BindNewPipeAndPassReceiver(),
      base::BindOnce(&OnDeviceModelServiceController::OnLoadModelResult,
                     weak_ptr_factory_.GetWeakPtr()));
  Execute(input, std::move(streaming_responder));
}

void OnDeviceModelServiceController::OnLoadModelResult(
    const std::optional<std::string>& error) {
  if (error.has_value()) {
    // TODO(b/302402576): Add error handling.
    LOG(ERROR) << *error;
  }
}

}  // namespace optimization_guide
