// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"

#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"

namespace optimization_guide {

namespace {

std::map<ModelBasedCapabilityKey, OnDeviceModelAdaptationLoader>
GetRequiredModelAdaptationLoaders(
    OptimizationGuideModelProvider* model_provider,
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager,
    PrefService* local_state,
    base::WeakPtr<OnDeviceModelServiceController>
        on_device_model_service_controller) {
  std::map<ModelBasedCapabilityKey, OnDeviceModelAdaptationLoader> loaders;
  for (const auto feature : kAllModelBasedCapabilityKeys) {
    if (!features::internal::GetOptimizationTargetForCapability(feature)) {
      continue;
    }
    loaders.emplace(
        std::piecewise_construct, std::forward_as_tuple(feature),
        std::forward_as_tuple(
            feature, model_provider, on_device_component_state_manager,
            local_state,
            base::BindRepeating(
                &OnDeviceModelServiceController::MaybeUpdateModelAdaptation,
                on_device_model_service_controller, feature)));
  }
  return loaders;
}

}  // namespace

OnDeviceAssetManager::OnDeviceAssetManager(
    PrefService* local_state,
    base::WeakPtr<OnDeviceModelServiceController> service_controller,
    base::WeakPtr<OnDeviceModelComponentStateManager> component_state_manager,
    raw_ptr<OptimizationGuideModelProvider> model_provider)
    : on_device_model_service_controller_(service_controller),
      on_device_component_state_manager_(component_state_manager),
      model_provider_(model_provider),
      model_adaptation_loaders_(
          GetRequiredModelAdaptationLoaders(model_provider,
                                            on_device_component_state_manager_,
                                            local_state,
                                            service_controller)) {
  if (!features::ShouldUseTextSafetyClassifierModel()) {
    return;
  }
  if (GetGenAILocalFoundationalModelEnterprisePolicySettings(local_state) !=
      model_execution::prefs::
          GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed) {
    return;
  }

  if (on_device_component_state_manager_) {
    on_device_component_state_manager_->AddObserver(this);
    if (on_device_component_state_manager_->IsInstallerRegistered()) {
      RegisterTextSafetyAndLanguageModels();
    }
  }
}
OnDeviceAssetManager::~OnDeviceAssetManager() {
  if (on_device_component_state_manager_) {
    on_device_component_state_manager_->RemoveObserver(this);
  }
  if (did_register_for_supplementary_on_device_models_) {
    model_provider_->RemoveObserverForOptimizationTargetModel(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_TEXT_SAFETY, this);
    model_provider_->RemoveObserverForOptimizationTargetModel(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
        this);
  }
}

// Whether the supplementary on-device models are registered.
bool OnDeviceAssetManager::IsSupplementaryModelRegistered() {
  return did_register_for_supplementary_on_device_models_;
}

void OnDeviceAssetManager::RegisterTextSafetyAndLanguageModels() {
  if (!did_register_for_supplementary_on_device_models_) {
    did_register_for_supplementary_on_device_models_ = true;
    model_provider_->AddObserverForOptimizationTargetModel(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_TEXT_SAFETY,
        /*model_metadata=*/std::nullopt, this);
    model_provider_->AddObserverForOptimizationTargetModel(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
        /*model_metadata=*/std::nullopt, this);
  }
}

void OnDeviceAssetManager::OnModelUpdated(
    proto::OptimizationTarget optimization_target,
    base::optional_ref<const ModelInfo> model_info) {
  switch (optimization_target) {
    case proto::OPTIMIZATION_TARGET_TEXT_SAFETY:
      if (on_device_model_service_controller_) {
        on_device_model_service_controller_->MaybeUpdateSafetyModel(model_info);
      }
      break;

    case proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION:
      if (on_device_model_service_controller_) {
        on_device_model_service_controller_->SetLanguageDetectionModel(
            model_info);
      }
      break;

    default:
      break;
  }
}

void OnDeviceAssetManager::StateChanged(
    const OnDeviceModelComponentState* state) {
  if (state) {
    RegisterTextSafetyAndLanguageModels();
  }
}

}  // namespace optimization_guide
