// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_ASSET_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_ASSET_MANAGER_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/delivery/optimization_target_model_observer.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

class UsageTracker;
class OnDeviceModelServiceController;
class OptimizationGuideModelProvider;

// Registers for on-device asset downloads and notifies about updates.
class OnDeviceAssetManager final
    : public OptimizationTargetModelObserver,
      public OnDeviceModelComponentStateManager::Observer,
      public UsageTracker::Observer {
 public:
  OnDeviceAssetManager(
      PrefService& local_state,
      UsageTracker& usage_tracker,
      OnDeviceModelComponentStateManager& component_state_manager,
      OnDeviceModelServiceController& service_controller,
      OptimizationGuideModelProvider& model_provider);
  ~OnDeviceAssetManager() final;

  // OptimizationTargetModelObserver:
  void OnModelUpdated(proto::OptimizationTarget target,
                      base::optional_ref<const ModelInfo> model_info) override;

 private:
  // Registers text safety and language detection models. Does nothing if
  // already registered.
  void RegisterTextSafetyAndLanguageModels();

  // OnDeviceModelComponentStateManager::Observer:
  void StateChanged(const OnDeviceModelComponentState* state) override;

  // UsageTracker::Observer:
  void OnDeviceEligibleFeatureFirstUsed(mojom::OnDeviceFeature feature) final;

  raw_ref<PrefService> local_state_;
  raw_ref<UsageTracker> usage_tracker_;
  raw_ref<OnDeviceModelComponentStateManager>
      on_device_component_state_manager_;

  // Controller for the on-device service.
  raw_ref<OnDeviceModelServiceController> service_controller_;

  // Map from feature to its model adaptation loader. Present only for features
  // that require model adaptation.
  AdaptationLoaderMap adaptation_loaders_;

  OptimizationGuideModelProviderObservation text_safety_model_observation_;
  OptimizationGuideModelProviderObservation
      language_detection_model_observation_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_ASSET_MANAGER_H_
