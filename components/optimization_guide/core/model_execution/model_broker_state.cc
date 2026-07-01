// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_state.h"

#include <cstddef>
#include <memory>

#include "base/containers/extend.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_classifier_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "components/optimization_guide/public/mojom/model_broker_debug.mojom.h"
#include "components/prefs/pref_service.h"

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
    : local_state_(local_state),
      service_client_(std::move(launch_fn)),
      download_progress_manager_(
          component_update_service,
          std::vector<std::string>{base_delegate->GetComponentId()}),
      usage_tracker_(&local_state),
      model_broker_impl_(
          usage_tracker_,
          base::BindRepeating(&ModelBrokerState::EnsureInitialization,
                              base::Unretained(this)),
          base::BindRepeating(
              [](base::RepeatingCallback<void(
                     mojo::PendingRemote<
                         on_device_model::mojom::DownloadObserver>)> callback,
                 const std::string& /* use_case */,
                 mojo::PendingRemote<on_device_model::mojom::DownloadObserver>
                     observer_remote) {
                callback.Run(std::move(observer_remote));
              },
              download_progress_manager_.GetAddObserverCallback())),
      performance_classifier_(&local_state, service_client_.GetSafeRef()),
      component_state_manager_(&local_state,
                               performance_classifier_.GetSafeRef(),
                               usage_tracker_,
                               std::move(base_delegate),
                               OnDeviceModelServiceController::kModelType),
      base_model_controller_(
          service_client_,
          usage_tracker_,
          model_broker_impl_,
          std::make_unique<OnDeviceModelAccessController>(local_state),
          component_state_manager_.GetWeakPtr()),
      asset_manager_(local_state,
                     usage_tracker_,
                     component_state_manager_,
                     base_model_controller_,
                     model_provider) {
  if (classifier_delegate) {
    classifier_controller_.emplace(
        local_state, performance_classifier_.GetSafeRef(), usage_tracker_,
        service_client_.GetSafeRef(), model_broker_impl_,
        std::move(classifier_delegate));
  }
  component_state_manager_.AddObserver(this);
}

ModelBrokerState::~ModelBrokerState() {
  component_state_manager_.RemoveObserver(this);
}

void ModelBrokerState::BindModelBroker(
    mojo::PendingReceiver<mojom::ModelBroker> receiver) {
  if (!features::IsOnDeviceExecutionEnabled()) {
    return;
  }
  model_broker_impl_.BindBroker(std::move(receiver));
}

void ModelBrokerState::BindModelBrokerDebug(
    base::PassKey<on_device_internals::PageHandler> key,
    mojo::PendingReceiver<mojom::ModelBrokerDebug> receiver) {
  receivers_.Add(this, std::move(receiver));
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
  base_model_controller_.UpdateSolutionProvider(feature);
  if (classifier_controller_) {
    classifier_controller_->UpdateSolution();
  }

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

void ModelBrokerState::EnsureInitialization(
    ModelBrokerImpl::InitCallback callback) {
  performance_classifier_.EnsurePerformanceClassAvailable(
      base::BindOnce(&ModelBrokerState::EnsureInitializationComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ModelBrokerState::EnsureInitializationComplete(
    ModelBrokerImpl::InitCallback callback) {
  std::move(callback).Run(
      performance_classifier_.GetPossibleOnDeviceCapabilities());
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

void ModelBrokerState::GetStateInfo(
    mojom::ModelBrokerDebug::GetStateInfoCallback callback) {
  auto result = mojom::BrokerStateInfo::New();
  result->properties = performance_classifier_.GetBrokerProperties();
  base::Extend(result->properties,
               component_state_manager_.GetBrokerProperties());
  result->assets = component_state_manager_.GetBrokerAssets();
  result->use_cases = model_broker_impl_.GetBrokerUseCaseInfo();

  result->model_crash_count = local_state_->GetInteger(
      model_execution::prefs::localstate::kOnDeviceModelCrashCount);
  result->max_model_crash_count =
      optimization_guide::features::GetOnDeviceModelCrashCountBeforeDisable();

  auto models_with_paths = base_model_controller_.GetBrokerModels();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](mojom::BrokerStateInfoPtr result,
             std::vector<std::pair<mojom::BrokerModelInfoPtr, base::FilePath>>
                 models_with_paths) {
            for (auto& [model_info, path] : models_with_paths) {
              if (!path.empty()) {
                model_info->folder_size = static_cast<uint64_t>(
                    base::ComputeDirectorySize(path));
              }
              result->models.push_back(std::move(model_info));
            }
            return result;
          },
          std::move(result), std::move(models_with_paths)),
      std::move(callback));
}

void ModelBrokerState::SetUseCaseRequested(const std::string& use_case,
                                           bool requested) {
  usage_tracker_.SetUseCaseRequested(use_case, requested);
}

void ModelBrokerState::UninstallModels() {
  component_state_manager_.ForceUninstall();
}

void ModelBrokerState::ResetModelCrashCount() {
  local_state_->SetInteger(
      model_execution::prefs::localstate::kOnDeviceModelCrashCount, 0);
}

void ModelBrokerState::AddObserver(
    mojo::PendingRemote<mojom::ModelBrokerDebugObserver> observer) {
  debug_observers_.Add(std::move(observer));
}

void ModelBrokerState::StateChanged(MaybeOnDeviceModelComponentState) {
  for (auto& observer : debug_observers_) {
    observer->OnBrokerStateChanged();
  }
}

}  // namespace optimization_guide
