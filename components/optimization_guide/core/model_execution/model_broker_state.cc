// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_state.h"

#include <cstddef>
#include <memory>

#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"

namespace optimization_guide {

ModelBrokerState::ModelBrokerState(
    PrefService& local_state,
    OptimizationGuideModelProvider& model_provider,
    std::unique_ptr<OnDeviceModelComponentStateManager::Delegate> base_delegate,
    std::unique_ptr<OnDeviceModelComponentStateManager::Delegate>
        classifier_delegate,
    on_device_model::ServiceClient::LaunchFn launch_fn,
    component_updater::ComponentUpdateService* component_update_service)
    : service_client_(std::move(launch_fn)),
      usage_tracker_(&local_state),
      performance_classifier_(&local_state, service_client_.GetSafeRef()),
      download_progress_manager_(component_update_service,
                                 {base_delegate->GetComponentId()}),
      component_state_manager_(&local_state,
                               performance_classifier_.GetSafeRef(),
                               usage_tracker_,
                               std::move(base_delegate)),
      service_controller_(
          std::make_unique<OnDeviceModelAccessController>(local_state),
          performance_classifier_.GetSafeRef(),
          component_state_manager_.GetWeakPtr(),
          usage_tracker_,
          service_client_.GetSafeRef(),
          download_progress_manager_.GetAddObserverCallback()),
      asset_manager_(local_state,
                     usage_tracker_,
                     component_state_manager_,
                     service_controller_,
                     model_provider) {}
ModelBrokerState::~ModelBrokerState() = default;

void ModelBrokerState::BindModelBroker(
    mojo::PendingReceiver<mojom::ModelBroker> receiver) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return;
  }
  service_controller_.BindBroker(std::move(receiver));
}

std::unique_ptr<OnDeviceSession> ModelBrokerState::StartSession(
    mojom::OnDeviceFeature feature,
    const SessionConfigParams& config_params,
    base::WeakPtr<OptimizationGuideLogger> logger) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return nullptr;
  }
  return service_controller_.CreateSession(feature, logger, config_params);
}

OnDeviceModelEligibilityReason ModelBrokerState::GetOnDeviceModelEligibility(
    mojom::OnDeviceFeature feature) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return OnDeviceModelEligibilityReason::kFeatureNotEnabled;
  }
  return service_controller_.CanCreateSession(feature);
}

void ModelBrokerState::GetOnDeviceModelEligibilityAsync(
    mojom::OnDeviceFeature feature,
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
ModelBrokerState::GetSamplingParamsConfig(mojom::OnDeviceFeature feature) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return std::nullopt;
  }

  const auto* adapter = service_controller_.GetAdapter(feature);
  if (!adapter) {
    return std::nullopt;
  }

  return adapter->GetSamplingParamsConfig();
}

std::optional<const proto::Any> ModelBrokerState::GetFeatureMetadata(
    mojom::OnDeviceFeature feature) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return std::nullopt;
  }

  const auto* adapter = service_controller_.GetAdapter(feature);
  if (!adapter) {
    return std::nullopt;
  }

  return adapter->GetFeatureMetadata();
}

void ModelBrokerState::FinishGetOnDeviceModelEligibility(
    mojom::OnDeviceFeature feature,
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
    mojom::OnDeviceFeature feature,
    OnDeviceModelAvailabilityObserver* observer) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return;
  }
  service_controller_.AddOnDeviceModelAvailabilityChangeObserver(feature,
                                                                 observer);
}

void ModelBrokerState::RemoveOnDeviceModelAvailabilityChangeObserver(
    mojom::OnDeviceFeature feature,
    OnDeviceModelAvailabilityObserver* observer) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return;
  }
  service_controller_.RemoveOnDeviceModelAvailabilityChangeObserver(feature,
                                                                    observer);
}

on_device_model::Capabilities ModelBrokerState::GetOnDeviceCapabilities() {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return {};
  }
  auto capabilities = service_controller_.GetCapabilities();
  capabilities.RetainAll(
      performance_classifier_.GetPossibleOnDeviceCapabilities());
  return capabilities;
}

}  // namespace optimization_guide
