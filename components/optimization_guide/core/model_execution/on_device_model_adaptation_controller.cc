// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

namespace {

// Invoked when adaptation assets are loaded. Calls the controller to continue
// loading the model if its still alive. Otherwise the loaded assets are closed
// in background task.
void OnAdaptationAssetsLoaded(
    base::WeakPtr<OnDeviceModelAdaptationController> adaptation_controller,
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    on_device_model::AdaptationAssets assets) {
  if (!adaptation_controller) {
    // Close the files on a background thread.
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::DoNothingWithBoundArgs(std::move(assets)));
    return;
  }
  adaptation_controller->LoadAdaptationModelFromAssets(std::move(model),
                                                       std::move(assets));
}

}  // namespace

OnDeviceModelAdaptationController::OnDeviceModelAdaptationController(
    ModelBasedCapabilityKey feature,
    base::WeakPtr<OnDeviceModelServiceController> controller)
    : feature_(feature), controller_(controller) {
  CHECK(features::internal::IsOnDeviceModelEnabled(feature));
}

OnDeviceModelAdaptationController::~OnDeviceModelAdaptationController() =
    default;

mojo::Remote<on_device_model::mojom::OnDeviceModel>&
OnDeviceModelAdaptationController::GetOrCreateModelRemote(
    const on_device_model::AdaptationAssetPaths& adaptation_assets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(features::internal::IsOnDeviceModelAdaptationEnabled(feature_));
  if (!model_remote_) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&on_device_model::LoadAdaptationAssets,
                       adaptation_assets),
        base::BindOnce(OnAdaptationAssetsLoaded, weak_ptr_factory_.GetWeakPtr(),
                       model_remote_.BindNewPipeAndPassReceiver()));
    model_remote_.set_disconnect_handler(base::BindOnce(
        &OnDeviceModelServiceController::OnModelAdaptationRemoteDisconnected,
        controller_, feature_, ModelRemoteDisconnectReason::kDisconncted));
    model_remote_.set_idle_handler(
        features::GetOnDeviceModelIdleTimeout(),
        base::BindRepeating(&OnDeviceModelServiceController::
                                OnModelAdaptationRemoteDisconnected,
                            controller_, feature_,
                            ModelRemoteDisconnectReason::kRemoteIdle));
  }
  return model_remote_;
}

void OnDeviceModelAdaptationController::LoadAdaptationModelFromAssets(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    on_device_model::AdaptationAssets assets) {
  auto& base_model_remote = controller_->base_model_remote();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base_model_remote) {
    // Close the files on a background thread.
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::DoNothingWithBoundArgs(std::move(assets)));
    return;
  }
  auto params = on_device_model::mojom::LoadAdaptationParams::New();
  params->assets = std::move(assets);

  base_model_remote->LoadAdaptation(
      std::move(params), std::move(model),
      base::BindOnce(&OnDeviceModelAdaptationController::OnLoadModelResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OnDeviceModelAdaptationController::OnLoadModelResult(
    on_device_model::mojom::LoadModelResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelAdaptationLoadResult",
      ConvertToOnDeviceModelLoadResult(result));
  switch (result) {
    case on_device_model::mojom::LoadModelResult::kGpuBlocked:
      controller_->OnModelAdaptationRemoteDisconnected(
          feature_, ModelRemoteDisconnectReason::kGpuBlocked);
      break;
    case on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary:
      controller_->OnModelAdaptationRemoteDisconnected(
          feature_, ModelRemoteDisconnectReason::kModelLoadFailed);
      break;
    case on_device_model::mojom::LoadModelResult::kSuccess:
      break;
  }
}

}  // namespace optimization_guide
