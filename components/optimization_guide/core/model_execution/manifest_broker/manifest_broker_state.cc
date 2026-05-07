// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"

#include <cstddef>
#include <memory>

#include "base/barrier_closure.h"
#include "base/containers/extend.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_solution_factory.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_validation.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
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

base::flat_map<mojom::OnDeviceFeature, proto::Any> GetFeatureConfigs(
    const Manifest& manifest) {
  base::flat_map<mojom::OnDeviceFeature, proto::Any> feature_configs;
  for (const auto& [name, config] :
       manifest.GetDeviceCategoryConfig().feature_configs()) {
    if (auto feature = GetFeatureForUseCase(name)) {
      feature_configs[*feature] = config;
    }
  }
  return feature_configs;
}

}  // namespace

ManifestBrokerState::ManifestBrokerState(
    PrefService& local_state,
    std::unique_ptr<ManifestAssetManager::Delegate> delegate,
    on_device_model::ServiceClient::LaunchFn launch_fn)
    : local_state_(local_state),
      delegate_(std::move(delegate)),
      service_client_(std::move(launch_fn)),
      usage_tracker_(&local_state),
      model_broker_impl_(
          usage_tracker_,
          base::BindRepeating(&ManifestBrokerState::EnsureInitialization,
                              base::Unretained(this)),
          base::DoNothing()),
      performance_classifier_(&local_state, service_client_.GetSafeRef()),
      manifest_monitor_(local_state, performance_classifier_, *delegate_),
      manifest_validator_(access_controller_, model_broker_impl_) {
  service_client_.set_on_disconnect_fn(
      base::BindRepeating(&ManifestBrokerState::OnServiceDisconnected,
                          weak_ptr_factory_.GetWeakPtr()));
  manifest_monitor_.SetCallback(base::BindRepeating(
      &ManifestBrokerState::OnManifestUpdated, weak_ptr_factory_.GetWeakPtr()));
  base::UmaHistogramBoolean(
      "OptimizationGuide.OnDeviceModel.ManifestBrokerInstantiated", true);
}

ManifestBrokerState::~ManifestBrokerState() = default;

void ManifestBrokerState::BindModelBroker(
    mojo::PendingReceiver<mojom::ModelBroker> receiver) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return;
  }
  model_broker_impl_.BindBroker(std::move(receiver));
}

void ManifestBrokerState::BindModelBrokerDebug(
    base::PassKey<on_device_internals::PageHandler> key,
    mojo::PendingReceiver<mojom::ModelBrokerDebug> receiver) {
  receivers_.Add(this, std::move(receiver));
}

std::unique_ptr<OnDeviceSession> ManifestBrokerState::StartSession(
    mojom::OnDeviceFeature feature,
    const SessionConfigParams& config_params,
    base::WeakPtr<OptimizationGuideLogger> logger) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return nullptr;
  }
  TRACE_EVENT("optimization_guide", "ManifestBrokerState::StartSession",
              "feature", base::ToString(feature));
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

void ManifestBrokerState::AddOnDeviceModelAvailabilityChangeObserver(
    mojom::OnDeviceFeature feature,
    OnDeviceModelAvailabilityObserver* observer) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return;
  }
  model_broker_impl_.GetSolutionProvider(feature).AddObserver(observer);
}

void ManifestBrokerState::RemoveOnDeviceModelAvailabilityChangeObserver(
    mojom::OnDeviceFeature feature,
    OnDeviceModelAvailabilityObserver* observer) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return;
  }
  model_broker_impl_.GetSolutionProvider(feature).RemoveObserver(observer);
}

OnDeviceModelEligibilityReason ManifestBrokerState::GetOnDeviceModelEligibility(
    mojom::OnDeviceFeature feature) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return OnDeviceModelEligibilityReason::kFeatureNotEnabled;
  }
  TRACE_EVENT("optimization_guide",
              "ManifestBrokerState::GetOnDeviceModelEligibility", "feature",
              base::ToString(feature));
  // TODO(holte): We many want to flush factory to avoid kUnknown responses,
  // but we might not actually need to.  Things should be moving away from
  // this interface already.
  // if (asset_manager_) {
  //   asset_manager_->factory_->UpdateSolutions();
  // }

  return model_broker_impl_.GetSolutionProvider(feature).solution().error_or(
      OnDeviceModelEligibilityReason::kSuccess);
}

void ManifestBrokerState::GetOnDeviceModelEligibilityAsync(
    mojom::OnDeviceFeature feature,
    const on_device_model::Capabilities& capabilities,
    base::OnceCallback<void(OnDeviceModelEligibilityReason)> callback) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    std::move(callback).Run(OnDeviceModelEligibilityReason::kFeatureNotEnabled);
    return;
  }
  EnsureInitialization(
      base::BindOnce(&ManifestBrokerState::FinishGetOnDeviceModelEligibility,
                     weak_ptr_factory_.GetWeakPtr(), feature, capabilities,
                     std::move(callback)));
}

void ManifestBrokerState::EnsureInitialization(
    ModelBrokerImpl::InitCallback callback) {
  // Hurry the performance classifier.
  performance_classifier_.EnsurePerformanceClassAvailable(base::DoNothing());
  if (manifest_monitor_.manifest().has_value()) {
    // Initialization is already complete.
    std::move(callback).Run(GetPossibleOnDeviceCapabilities());
    return;
  }
  init_callbacks_.push_back(std::move(callback));
}

void ManifestBrokerState::OnManifestUpdated() {
  TRACE_EVENT("optimization_guide", "ManifestBrokerState::OnManifestUpdated");
  CHECK(manifest_monitor_.manifest().has_value());

  model_broker_impl_.SetFeatureConfigs(
      GetFeatureConfigs(*manifest_monitor_.manifest()));

  // Init will complete the first time we finish loading all available assets
  // for a manifest.
  auto factory = std::make_unique<ManifestSolutionFactory>(
      *manifest_monitor_.manifest(), model_broker_impl_, usage_tracker_,
      service_client_, access_controller_,
      base::BindOnce(&ManifestBrokerState::OnInitComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  if (!asset_manager_) {
    asset_manager_ = std::make_unique<ManifestAssetManager>(
        *local_state_, usage_tracker_, *delegate_, std::move(factory));
  } else {
    asset_manager_->UpdateSolutionFactory(std::move(factory));
  }
}

void ManifestBrokerState::OnInitComplete() {
  TRACE_EVENT("optimization_guide", "ManifestBrokerState::OnInitComplete");
  auto callbacks_to_run = std::move(init_callbacks_);
  init_callbacks_.clear();
  for (auto& callback : callbacks_to_run) {
    std::move(callback).Run(GetPossibleOnDeviceCapabilities());
  }

  if (manifest_monitor_.manifest().has_value()) {
    const auto& manifest = *manifest_monitor_.manifest();
    const auto& category_config = manifest.GetDeviceCategoryConfig();
    if (category_config.has_validations()) {
      manifest_validator_.MaybeExecuteValidationTask(
          category_config.validations());
    }
  }
}

on_device_model::Capabilities
ManifestBrokerState::GetPossibleOnDeviceCapabilities() const {
  CHECK(manifest_monitor_.manifest().has_value());
  if (!performance_classifier_.IsPerformanceClassAvailable()) {
    // This should only happen if a policy is disabling on-device models.
    return {};
  }
  return performance_classifier_.GetPossibleOnDeviceCapabilities();
}

void ManifestBrokerState::FinishGetOnDeviceModelEligibility(
    mojom::OnDeviceFeature feature,
    const on_device_model::Capabilities& capabilities,
    base::OnceCallback<void(optimization_guide::OnDeviceModelEligibilityReason)>
        callback,
    const on_device_model::Capabilities& possible_capabilities) {
  // If this device will never support the requested capabilities, return not
  // available.
  if (!possible_capabilities.HasAll(capabilities)) {
    std::move(callback).Run(optimization_guide::OnDeviceModelEligibilityReason::
                                kModelAdaptationNotAvailable);
    return;
  }
  std::move(callback).Run(GetOnDeviceModelEligibility(feature));
}

void ManifestBrokerState::OnServiceDisconnected(
    on_device_model::ServiceDisconnectReason reason) {
  TRACE_EVENT("optimization_guide",
              "ManifestBrokerState::OnServiceDisconnected", "reason", reason);
  switch (reason) {
    case on_device_model::ServiceDisconnectReason::kGpuBlocked:
      access_controller_.OnGpuBlocked();
      if (asset_manager_) {
        asset_manager_->RefreshSolutions();
      }
      break;
    // Below errors will be tracked by the related model disconnects, so they
    // are not handled specifically here.
    case on_device_model::ServiceDisconnectReason::kFailedToLoadLibrary:
    case on_device_model::ServiceDisconnectReason::kUnspecified:
      break;
  }
}

void ManifestBrokerState::GetStateInfo(
    mojom::ModelBrokerDebug::GetStateInfoCallback callback) {
  auto result = mojom::BrokerStateInfo::New();
  result->properties.push_back(
      mojom::BrokerPropertyInfo::New("Broker Type", "ManifestBrokerState"));
  base::Extend(result->properties,
               performance_classifier_.GetBrokerProperties());
  base::Extend(result->properties, manifest_monitor_.GetBrokerProperties());
  result->use_cases = model_broker_impl_.GetBrokerUseCaseInfo();
  if (asset_manager_) {
    result->assets = asset_manager_->GetBrokerAssets();
    result->models = asset_manager_->GetBrokerModels();
  }
  std::move(callback).Run(std::move(result));
}

void ManifestBrokerState::SetUseCaseRequested(const std::string& use_case,
                                              bool requested) {
  usage_tracker_.SetUseCaseRequested(use_case, requested);
}

void ManifestBrokerState::UninstallModels() {
  // TODO: crbug.com/489511500 - Implement this.
  // component_state_manager_.ForceUninstall();
}

}  // namespace optimization_guide
