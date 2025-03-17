// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_ASSET_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_ASSET_MANAGER_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

namespace optimization_guide {

class OnDeviceModelAdaptationLoader;
class OnDeviceModelServiceController;
class OptimizationGuideModelProvider;

// Registers for on-device asset downloads and notifies about updates.
class OnDeviceAssetManager final
    : public OptimizationTargetModelObserver,
      public OnDeviceModelComponentStateManager::Observer {
 public:
  OnDeviceAssetManager(
      PrefService* local_state,
      base::WeakPtr<OnDeviceModelServiceController> service_controller,
      base::WeakPtr<OnDeviceModelComponentStateManager> component_state_manager,
      raw_ptr<OptimizationGuideModelProvider> model_provider);
  ~OnDeviceAssetManager() final;

  // OptimizationTargetModelObserver:
  void OnModelUpdated(proto::OptimizationTarget target,
                      base::optional_ref<const ModelInfo> model_info) override;

 private:
  // Registers text safety and language detection models. Does nothing if
  // already registered.
  void RegisterTextSafetyAndLanguageModels();

  // Whether the supplementary on-device models are registered.
  bool IsSupplementaryModelRegistered();

  // OnDeviceModelComponentStateManager::Observer:
  void StateChanged(const OnDeviceModelComponentState* state) override;

  // Controller for the on-device service.
  base::WeakPtr<OnDeviceModelServiceController>
      on_device_model_service_controller_;

  base::WeakPtr<OnDeviceModelComponentStateManager>
      on_device_component_state_manager_;

  // The model provider to observe for updates to auxiliary models.
  raw_ptr<OptimizationGuideModelProvider> model_provider_;

  // Map from feature to its model adaptation loader. Present only for features
  // that require model adaptation.
  const std::map<ModelBasedCapabilityKey, OnDeviceModelAdaptationLoader>
      model_adaptation_loaders_;

  // Whether the user registered for supplementary on-device models.
  bool did_register_for_supplementary_on_device_models_ = false;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_ASSET_MANAGER_H_
