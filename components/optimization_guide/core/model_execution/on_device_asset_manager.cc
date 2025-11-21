// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"

#include <memory>

#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

namespace {

proto::OptimizationTarget GetOptimizationTargetForSafetyModel() {
  return features::ShouldUseGeneralizedSafetyModel()
             ? proto::OptimizationTarget::OPTIMIZATION_TARGET_GENERALIZED_SAFETY
             : proto::OptimizationTarget::OPTIMIZATION_TARGET_TEXT_SAFETY;
}

SafetyModelInfo::SafetyModelType GetSafetyModelTypeFromOptimizationTarget(
    proto::OptimizationTarget target) {
  return target == proto::OptimizationTarget::
                       OPTIMIZATION_TARGET_GENERALIZED_SAFETY
             ? SafetyModelInfo::SafetyModelType::kGeneralizedSafetyModel
             : SafetyModelInfo::SafetyModelType::kTextSafetyModel;
}

}  // namespace

OnDeviceAssetManager::OnDeviceAssetManager(
    PrefService& local_state,
    UsageTracker& usage_tracker,
    OnDeviceModelComponentStateManager& component_state_manager,
    OnDeviceModelServiceController& service_controller,
    OptimizationGuideModelProvider& model_provider)
    : local_state_(local_state),
      usage_tracker_(usage_tracker),
      on_device_component_state_manager_(component_state_manager),
      service_controller_(service_controller),
      adaptation_loaders_(
          model_provider,
          base::BindRepeating(
              &OnDeviceModelServiceController::MaybeUpdateModelAdaptation,
              service_controller.GetWeakPtr())),
      text_safety_model_observation_(
          &model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
          this),
      language_detection_model_observation_(
          &model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
          this) {
  usage_tracker_->AddObserver(this);
  on_device_component_state_manager_->AddObserver(this);

  if (auto* state = on_device_component_state_manager_->GetState()) {
    StateChanged(state);
  }
}

OnDeviceAssetManager::~OnDeviceAssetManager() {
  on_device_component_state_manager_->RemoveObserver(this);
  usage_tracker_->RemoveObserver(this);
}

void OnDeviceAssetManager::RegisterTextSafetyAndLanguageModels() {
  if (!features::ShouldUseTextSafetyClassifierModel()) {
    return;
  }
  if (GetGenAILocalFoundationalModelEnterprisePolicySettings(&*local_state_) !=
      model_execution::prefs::
          GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed) {
    return;
  }

  if (!text_safety_model_observation_.IsRegistered()) {
    text_safety_model_observation_.Observe(
        GetOptimizationTargetForSafetyModel(),
        /*model_metadata=*/std::nullopt);
  }
  if (!language_detection_model_observation_.IsRegistered()) {
    language_detection_model_observation_.Observe(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
        /*model_metadata=*/std::nullopt);
  }
}

void OnDeviceAssetManager::OnModelUpdated(
    proto::OptimizationTarget optimization_target,
    base::optional_ref<const ModelInfo> model_info) {
  switch (optimization_target) {
    case proto::OPTIMIZATION_TARGET_GENERALIZED_SAFETY:
    case proto::OPTIMIZATION_TARGET_TEXT_SAFETY: {
      std::unique_ptr<SafetyModelInfo> safety_model_info =
          SafetyModelInfo::Load(
              GetSafetyModelTypeFromOptimizationTarget(optimization_target),
              model_info);

      if (safety_model_info) {
        service_controller_->MaybeUpdateSafetyModel(
            std::move(safety_model_info));
      }
      break;
    }

    case proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION:
      service_controller_->SetLanguageDetectionModel(model_info);
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
  std::optional<OnDeviceBaseModelSpec> new_spec =
      state ? std::make_optional(state->GetBaseModelSpec()) : std::nullopt;
  for (auto feature : OnDeviceFeatureSet::All()) {
    adaptation_loaders_.MaybeRegisterModelDownload(
        feature, new_spec,
        usage_tracker_->WasOnDeviceEligibleFeatureRecentlyUsed(feature));
  }
}

void OnDeviceAssetManager::OnDeviceEligibleFeatureFirstUsed(
    mojom::OnDeviceFeature feature) {
  const OnDeviceModelComponentState* state =
      on_device_component_state_manager_->GetState();
  std::optional<OnDeviceBaseModelSpec> new_spec =
      state ? std::make_optional(state->GetBaseModelSpec()) : std::nullopt;
  adaptation_loaders_.MaybeRegisterModelDownload(
      feature, new_spec,
      usage_tracker_->WasOnDeviceEligibleFeatureRecentlyUsed(feature));
}

}  // namespace optimization_guide
