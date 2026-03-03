// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_state.h"

#include <cstddef>
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"

namespace optimization_guide {

namespace {

void LogEligibilityReason(mojom::OnDeviceFeature feature,
                          OnDeviceModelEligibilityReason reason) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.",
           GetVariantName(feature)}),
      reason);
}

}  // namespace

ModelBrokerState::ModelBrokerState(
    PrefService& local_state,
    OptimizationGuideModelProvider& model_provider,
    std::unique_ptr<OnDeviceModelComponentStateManager::Delegate> base_delegate,
    std::unique_ptr<OnDeviceModelComponentStateManager::Delegate>
        classifier_delegate,
    on_device_model::ServiceClient::LaunchFn launch_fn,
    component_updater::ComponentUpdateService* component_update_service)
    : service_client_(std::move(launch_fn)),
      download_progress_manager_(component_update_service,
                                 {base_delegate->GetComponentId()}),
      usage_tracker_(&local_state),
      model_broker_impl_(
          usage_tracker_,
          base::BindRepeating(&ModelBrokerState::EnsureInitialization,
                              base::Unretained(this)),
          download_progress_manager_.GetAddObserverCallback()),
      performance_classifier_(&local_state, service_client_.GetSafeRef()),
      component_state_manager_(&local_state,
                               performance_classifier_.GetSafeRef(),
                               usage_tracker_,
                               std::move(base_delegate)),
      service_controller_(
          service_client_,
          usage_tracker_,
          model_broker_impl_,
          std::make_unique<OnDeviceModelAccessController>(local_state),
          component_state_manager_.GetWeakPtr()),
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
  model_broker_impl_.BindBroker(std::move(receiver));
}

std::unique_ptr<OnDeviceSession> ModelBrokerState::StartSession(
    mojom::OnDeviceFeature feature,
    const SessionConfigParams& config_params,
    base::WeakPtr<OptimizationGuideLogger> logger) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return nullptr;
  }
  TRACE_EVENT("optimization_guide", "ModelBrokerState::StartSession", "feature",
              base::ToString(feature));
  // TODO: holte - This should be simplified if we remove integration test
  // dependencies on the EligibilityReason histogram being logged.
  OnDeviceModelEligibilityReason reason = GetOnDeviceModelEligibility(feature);
  LogEligibilityReason(feature, reason);
  usage_tracker_.OnDeviceEligibleFeatureUsed(feature);

  // Return if we cannot do anything more for right now.
  if (reason != OnDeviceModelEligibilityReason::kSuccess) {
    VLOG(1) << "Failed to create Session:" << reason;
    return nullptr;
  }
  // Client should be non-null because GetOnDeviceModelEligibility above
  // succeeded.
  return model_broker_impl_.GetSolutionProvider(feature)
      .local_subscriber()
      .client()
      ->CreateSession(config_params, logger);
}

OnDeviceModelEligibilityReason ModelBrokerState::GetOnDeviceModelEligibility(
    mojom::OnDeviceFeature feature) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return OnDeviceModelEligibilityReason::kFeatureNotEnabled;
  }
  TRACE_EVENT("optimization_guide",
              "ModelBrokerState::GetOnDeviceModelEligibility", "feature",
              base::ToString(feature));
  // Ensure a solution is constructed for this feature, to avoid returning
  // kUnknown when this is called too early.
  service_controller_.UpdateSolutionProvider(feature);
  return model_broker_impl_.GetSolutionProvider(feature).solution().error_or(
      OnDeviceModelEligibilityReason::kSuccess);
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

  const auto& solution =
      model_broker_impl_.GetSolutionProvider(feature).solution();
  if (!solution.has_value()) {
    return std::nullopt;
  }

  // Solution owns the scoped_refptr to the adapter, so the return pointer of
  // GetAdapter() is always safe to use.
  return solution.value()->GetAdapter()->GetSamplingParamsConfig();
}

std::optional<const proto::Any> ModelBrokerState::GetFeatureMetadata(
    mojom::OnDeviceFeature feature) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return std::nullopt;
  }

  const auto& solution =
      model_broker_impl_.GetSolutionProvider(feature).solution();
  if (!solution.has_value()) {
    return std::nullopt;
  }

  // Solution owns the scoped_refptr to the adapter, so the return pointer of
  // GetAdapter() is always safe to use.
  return solution.value()->GetAdapter()->GetFeatureMetadata();
}

void ModelBrokerState::EnsureInitialization(base::OnceClosure callback) {
  performance_classifier_.EnsurePerformanceClassAvailable(std::move(callback));
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
  model_broker_impl_.GetSolutionProvider(feature).AddObserver(observer);
}

void ModelBrokerState::RemoveOnDeviceModelAvailabilityChangeObserver(
    mojom::OnDeviceFeature feature,
    OnDeviceModelAvailabilityObserver* observer) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return;
  }
  model_broker_impl_.GetSolutionProvider(feature).RemoveObserver(observer);
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
