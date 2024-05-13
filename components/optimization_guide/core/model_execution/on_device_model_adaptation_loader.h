// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_ADAPTATION_MODEL_LOADER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_ADAPTATION_MODEL_LOADER_H_

#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "services/on_device_model/public/cpp/model_assets.h"

namespace optimization_guide {

class OnDeviceModelMetadata;
class OptimizationGuideModelProvider;
enum class OnDeviceModelAdaptationAvailability;

// Loads model adaptation assets for a particular feature. Performs adaptation
// model compatibility checks with the base model and reloads the assets if the
// base model changes.
class OnDeviceModelAdaptationLoader
    : public OptimizationTargetModelObserver,
      public OnDeviceModelComponentStateManager::Observer {
 public:
  using OnLoadFn = base::RepeatingCallback<void(
      std::unique_ptr<on_device_model::AdaptationAssetPaths>)>;

  OnDeviceModelAdaptationLoader(
      ModelBasedCapabilityKey feature,
      OptimizationGuideModelProvider* model_provider,
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager,
      OnLoadFn on_load_fn);
  ~OnDeviceModelAdaptationLoader() override;

  OnDeviceModelAdaptationLoader(const OnDeviceModelAdaptationLoader&) = delete;
  OnDeviceModelAdaptationLoader& operator=(
      const OnDeviceModelAdaptationLoader&) = delete;

 private:
  friend class OnDeviceModelAdaptationLoaderTest;

  // OptimizationTargetModelObserver:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info) final;

  // OnDeviceModelComponentStateManager::Observer.
  void StateChanged(const OnDeviceModelComponentState* state) final;

  base::expected<std::unique_ptr<on_device_model::AdaptationAssetPaths>,
                 OnDeviceModelAdaptationAvailability>
  ProcessModelUpdate(
      base::optional_ref<const optimization_guide::ModelInfo> model_info);

  ModelBasedCapabilityKey feature_;

  // The model spec of the latest base model, received from the component
  // state manager.
  std::optional<OnDeviceBaseModelSpec> base_model_spec_;

  OnLoadFn on_load_fn_;

  base::ScopedObservation<OnDeviceModelComponentStateManager,
                          OnDeviceModelComponentStateManager::Observer>
      component_state_manager_observation_{this};

  // The model provider to observe for updates to model adaptations.
  raw_ptr<OptimizationGuideModelProvider> model_provider_;
  bool registered_with_model_provider_ = false;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_ADAPTATION_MODEL_LOADER_H_
