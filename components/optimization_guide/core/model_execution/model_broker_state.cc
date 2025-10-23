// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_state.h"

#include <cstddef>

#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace optimization_guide {

ModelBrokerState::ModelBrokerState(
    PrefService* local_state,
    std::unique_ptr<OnDeviceModelComponentStateManager::Delegate> delegate,
    on_device_model::ServiceClient::LaunchFn launch_fn)
    : local_state_(local_state),
      service_client_(std::move(launch_fn)),
      usage_tracker_(local_state),
      performance_classifier_(local_state, service_client_.GetSafeRef()),
      component_state_manager_(local_state,
                               performance_classifier_.GetSafeRef(),
                               usage_tracker_,
                               std::move(delegate)) {}
ModelBrokerState::~ModelBrokerState() = default;

void ModelBrokerState::Init() {
  CHECK(!service_controller_);
  performance_classifier_.Init();
  component_state_manager_.OnStartup();
  service_controller_ = std::make_unique<OnDeviceModelServiceController>(
      std::make_unique<OnDeviceModelAccessController>(*local_state_),
      performance_classifier_.GetSafeRef(),
      component_state_manager_.GetWeakPtr(), usage_tracker_,
      service_client_.GetSafeRef());
  service_controller_->Init();
}

std::unique_ptr<OnDeviceAssetManager> ModelBrokerState::CreateAssetManager(
    OptimizationGuideModelProvider* provider) {
  return std::make_unique<OnDeviceAssetManager>(
      *local_state_, usage_tracker_, component_state_manager_,
      *service_controller_, *provider);
}

void ModelBrokerState::BindModelBroker(
    mojo::PendingReceiver<mojom::ModelBroker> receiver) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return;
  }
  service_controller_->BindBroker(std::move(receiver));
}

std::unique_ptr<OnDeviceSession> ModelBrokerState::StartSession(
    ModelBasedCapabilityKey feature,
    const SessionConfigParams& config_params) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return nullptr;
  }
  return service_controller_->CreateSession(feature, config_params);
}

OnDeviceModelEligibilityReason ModelBrokerState::GetOnDeviceModelEligibility(
    ModelBasedCapabilityKey feature) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return OnDeviceModelEligibilityReason::kFeatureNotEnabled;
  }
  return service_controller_->CanCreateSession(feature);
}

void ModelBrokerState::GetOnDeviceModelEligibilityAsync(
    ModelBasedCapabilityKey feature,
    const on_device_model::Capabilities& capabilities,
    base::OnceCallback<void(OnDeviceModelEligibilityReason)> callback) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    std::move(callback).Run(OnDeviceModelEligibilityReason::kFeatureNotEnabled);
    return;
  }
  performance_classifier_.EnsurePerformanceClassAvailable(
      base::BindOnce(&ModelBrokerState::FinishGetOnDeviceModelEligibility,
                     weak_ptr_factory_.GetWeakPtr(), feature, capabilities,
                     std::move(callback)));
}

std::optional<optimization_guide::SamplingParamsConfig>
ModelBrokerState::GetSamplingParamsConfig(
    optimization_guide::ModelBasedCapabilityKey feature) {
  MaybeAdaptationMetadata metadata =
      service_controller_->GetFeatureMetadata(feature);
  if (!features::IsOnDeviceExecutionEnabled() || !metadata.has_value()) {
    return std::nullopt;
  }
  return metadata->adapter()->GetSamplingParamsConfig();
}

std::optional<const proto::Any> ModelBrokerState::GetFeatureMetadata(
    optimization_guide::ModelBasedCapabilityKey feature) {
  MaybeAdaptationMetadata metadata =
      service_controller_->GetFeatureMetadata(feature);
  if (!features::IsOnDeviceExecutionEnabled() || !metadata.has_value()) {
    return std::nullopt;
  }
  return metadata->adapter()->GetFeatureMetadata();
}

void ModelBrokerState::FinishGetOnDeviceModelEligibility(
    optimization_guide::ModelBasedCapabilityKey feature,
    const on_device_model::Capabilities& capabilities,
    base::OnceCallback<void(optimization_guide::OnDeviceModelEligibilityReason)>
        callback) {
  // If this device will never support the requested capabilities, return not
  // available.
  if (!performance_classifier_.GetPossibleOnDeviceCapabilities().HasAll(
          capabilities)) {
    std::move(callback).Run(optimization_guide::OnDeviceModelEligibilityReason::
                                kModelAdaptationNotAvailable);
    return;
  }
  std::move(callback).Run(GetOnDeviceModelEligibility(feature));
}

void ModelBrokerState::AddOnDeviceModelAvailabilityChangeObserver(
    ModelBasedCapabilityKey feature,
    OnDeviceModelAvailabilityObserver* observer) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return;
  }
  service_controller_->AddOnDeviceModelAvailabilityChangeObserver(feature,
                                                                  observer);
}

void ModelBrokerState::RemoveOnDeviceModelAvailabilityChangeObserver(
    ModelBasedCapabilityKey feature,
    OnDeviceModelAvailabilityObserver* observer) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return;
  }
  service_controller_->RemoveOnDeviceModelAvailabilityChangeObserver(feature,
                                                                     observer);
}

on_device_model::Capabilities ModelBrokerState::GetOnDeviceCapabilities() {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return {};
  }
  auto capabilities = service_controller_->GetCapabilities();
  capabilities.RetainAll(
      performance_classifier_.GetPossibleOnDeviceCapabilities());
  return capabilities;
}

}  // namespace optimization_guide
