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
    model_remote_->Execute(std::string(input), std::move(streaming_responder));
    return;
  }
  LaunchService();
  service_remote_->LoadModel(
      on_device_model::LoadModelAssets(model_path_),
      base::BindOnce(&OnDeviceModelServiceController::OnLoadModelResult,
                     weak_ptr_factory_.GetWeakPtr(), input,
                     std::move(streaming_responder)));
}

void OnDeviceModelServiceController::OnLoadModelResult(
    std::string_view input,
    mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
        streaming_responder,
    on_device_model::mojom::LoadModelResultPtr result) {
  if (result->is_model()) {
    model_remote_ = mojo::Remote<on_device_model::mojom::OnDeviceModel>(
        std::move(result->get_model()));
    Execute(input, std::move(streaming_responder));
  }
}

}  // namespace optimization_guide
