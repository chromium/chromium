// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"

#include <memory>

#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

namespace {

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
        proto::OptimizationTarget::OPTIMIZATION_TARGET_GENERALIZED_SAFETY,
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
    case proto::OPTIMIZATION_TARGET_GENERALIZED_SAFETY: {
      std::unique_ptr<SafetyModelInfo> safety_model_info =
          SafetyModelInfo::Load(
              SafetyModelInfo::SafetyModelType::kGeneralizedSafetyModel,
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
    MaybeOnDeviceModelComponentState state) {
  std::optional<OnDeviceBaseModelSpec> new_spec;

  if (state.has_value()) {
    RegisterTextSafetyAndLanguageModels();
    new_spec = state.value().get().GetBaseModelSpec();
  }

  for (auto feature : OnDeviceFeatureSet::All()) {
    adaptation_loaders_.MaybeRegisterModelDownload(
        feature, new_spec,
        usage_tracker_->WasUseCaseRecentlyUsed(ToUseCaseName(feature)));
  }
}

void OnDeviceAssetManager::OnDeviceEligibleUseCaseUsed(
    const std::string& use_case_name,
    bool is_first_usage) {
  if (!is_first_usage) {
    return;
  }
  auto feature = GetFeatureForUseCase(use_case_name);
  if (!feature) {
    return;
  }

  const OnDeviceModelComponentState* state =
      on_device_component_state_manager_->GetState();
  std::optional<OnDeviceBaseModelSpec> new_spec =
      state ? std::make_optional(state->GetBaseModelSpec()) : std::nullopt;
  adaptation_loaders_.MaybeRegisterModelDownload(
      *feature, new_spec,
      usage_tracker_->WasUseCaseRecentlyUsed(use_case_name));
}

}  // namespace optimization_guide
