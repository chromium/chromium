// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ADAPTATION_CONTROLLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ADAPTATION_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

namespace optimization_guide {

// Controls the on-device model adaptations per feature.
class OnDeviceModelAdaptationController final : public ModelController {
 public:
  OnDeviceModelAdaptationController(
      ModelBasedCapabilityKey feature,
      base::WeakPtr<ModelController> controller,
      const on_device_model::AdaptationAssetPaths& asset_paths);
  ~OnDeviceModelAdaptationController() override;

  OnDeviceModelAdaptationController(const OnDeviceModelAdaptationController&) =
      delete;
  OnDeviceModelAdaptationController& operator=(
      const OnDeviceModelAdaptationController&) = delete;

  // Loads the adaptation model from the loaded assets.
  void LoadAdaptationModelFromAssets(
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      on_device_model::AdaptationAssets assets);

  mojo::Remote<on_device_model::mojom::OnDeviceModel>& GetOrCreateRemote()
      override;

  base::WeakPtr<OnDeviceModelAdaptationController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  ModelBasedCapabilityKey feature_;

  base::WeakPtr<ModelController> controller_;

  on_device_model::AdaptationAssetPaths asset_paths_;

  mojo::Remote<on_device_model::mojom::OnDeviceModel> model_remote_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<OnDeviceModelAdaptationController> weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ADAPTATION_CONTROLLER_H_
