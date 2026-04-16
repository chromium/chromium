// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_STATE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_STATE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_model_classifier_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace optimization_guide {

// This holds the state for the on-device model broker. This is an abstraction
// to allow chrome and other embedders to share the same broker logic while
// owning the state separately.
class ModelBrokerState final : public OnDeviceCapability,
                               mojom::ModelBrokerDebug {
 public:
  ModelBrokerState(
      PrefService& local_state,
      OptimizationGuideModelProvider& model_provider,
      std::unique_ptr<OnDeviceModelComponentStateManager::Delegate>
          base_delegate,
      std::unique_ptr<OnDeviceModelComponentStateManager::Delegate>
          classifier_delegate,
      on_device_model::ServiceClient::LaunchFn launch_fn,
      component_updater::ComponentUpdateService* component_update_service);
  ~ModelBrokerState() override;

  ModelBrokerState(const ModelBrokerState&) = delete;
  ModelBrokerState& operator=(const ModelBrokerState&) = delete;

  PerformanceClassifier& performance_classifier() {
    return performance_classifier_;
  }

  UsageTracker& usage_tracker() { return usage_tracker_; }

  OnDeviceModelComponentStateManager& component_state_manager() {
    return component_state_manager_;
  }

  OnDeviceModelServiceController& base_model_controller() {
    return base_model_controller_;
  }

  // OnDeviceCapability
  void BindModelBroker(
      mojo::PendingReceiver<mojom::ModelBroker> receiver) override;
  void BindModelBrokerDebug(
      base::PassKey<on_device_internals::PageHandler> key,
      mojo::PendingReceiver<mojom::ModelBrokerDebug> receiver) override;
  std::unique_ptr<OnDeviceSession> StartSession(
      mojom::OnDeviceFeature feature,
      const SessionConfigParams& config_params,
      base::WeakPtr<OptimizationGuideLogger> logger) override;
  OnDeviceModelEligibilityReason GetOnDeviceModelEligibility(
      mojom::OnDeviceFeature feature) override;
  void GetOnDeviceModelEligibilityAsync(
      mojom::OnDeviceFeature feature,
      const on_device_model::Capabilities& capabilities,
      base::OnceCallback<void(OnDeviceModelEligibilityReason)> callback)
      override;
  void AddOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature feature,
      OnDeviceModelAvailabilityObserver* observer) override;
  void RemoveOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature feature,
      OnDeviceModelAvailabilityObserver* observer) override;

  // mojom::ModelBrokerDebug
  void GetStateInfo(
      mojom::ModelBrokerDebug::GetStateInfoCallback callback) override;
  void SetUseCaseRequested(const std::string& use_case,
                           bool requested) override;
  void UninstallModels() override;

 private:
  // Ensure any delayed initialization tasks are complete, then call `callback`.
  void EnsureInitialization(ModelBrokerImpl::InitCallback callback);
  void EnsureInitializationComplete(ModelBrokerImpl::InitCallback callback);

  void FinishGetOnDeviceModelEligibility(
      mojom::OnDeviceFeature feature,
      const on_device_model::Capabilities& capabilities,
      base::OnceCallback<
          void(optimization_guide::OnDeviceModelEligibilityReason)> callback);

  on_device_model::ServiceClient service_client_;
  OnDeviceModelDownloadProgressManager download_progress_manager_;
  UsageTracker usage_tracker_;
  ModelBrokerImpl model_broker_impl_;
  PerformanceClassifier performance_classifier_;
  OnDeviceModelComponentStateManager component_state_manager_;
  OnDeviceModelServiceController base_model_controller_;
  std::optional<OnDeviceModelClassifierController> classifier_controller_;
  OnDeviceAssetManager asset_manager_;
  mojo::ReceiverSet<ModelBrokerDebug> receivers_;
  base::WeakPtrFactory<ModelBrokerState> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_STATE_H_
