// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_BROKER_STATE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_BROKER_STATE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/component_updater/component_updater_service.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_monitor.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_solution_factory.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_validation.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_classifier_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"

class PrefService;

namespace optimization_guide {

// This holds the state for the manifest based on-device model broker.
class ManifestBrokerState final : public OnDeviceCapability,
                                  mojom::ModelBrokerDebug {
 public:
  ManifestBrokerState(
      PrefService& local_state,
      std::unique_ptr<ManifestAssetManager::Delegate> delegate,
      on_device_model::ServiceClient::LaunchFn launch_fn,
      component_updater::ComponentUpdateService* component_update_service);
  ~ManifestBrokerState() override;

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
  void AddOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature feature,
      OnDeviceModelAvailabilityObserver* observer) override;
  void RemoveOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature feature,
      OnDeviceModelAvailabilityObserver* observer) override;
  OnDeviceModelEligibilityReason GetOnDeviceModelEligibility(
      mojom::OnDeviceFeature feature) override;
  void GetOnDeviceModelEligibilityAsync(
      mojom::OnDeviceFeature feature,
      const on_device_model::Capabilities& capabilities,
      base::OnceCallback<void(OnDeviceModelEligibilityReason)> callback)
      override;

  // mojom::ModelBrokerDebug
  void GetStateInfo(
      mojom::ModelBrokerDebug::GetStateInfoCallback callback) override;
  void SetUseCaseRequested(const std::string& use_case,
                           bool requested) override;
  void UninstallModels() override;

  PerformanceClassifier& performance_classifier() {
    return performance_classifier_;
  }

 private:
  // Ensure any delayed initialization tasks are complete, then call `callback`.
  void EnsureInitialization(ModelBrokerImpl::InitCallback callback);
  // Called the first time EnsureInitialization is called.
  void StartInitialization();
  // Called by ManifestMonitor whenever the Manifest is updated.
  void OnManifestUpdated();
  // Fires all stored callbacks.
  void OnInitComplete();

  on_device_model::Capabilities GetPossibleOnDeviceCapabilities() const;

  void FinishGetOnDeviceModelEligibility(
      mojom::OnDeviceFeature feature,
      const on_device_model::Capabilities& capabilities,
      base::OnceCallback<
          void(optimization_guide::OnDeviceModelEligibilityReason)> callback,
      const on_device_model::Capabilities& possible_capabilities);

  // Handle unexpected service disconnects.
  void OnServiceDisconnected(on_device_model::ServiceDisconnectReason reason);

  void AddDownloadProgressObserver(
      const std::string& use_case,
      mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer);

  raw_ref<PrefService> local_state_;
  std::unique_ptr<ManifestAssetManager::Delegate> delegate_;
  on_device_model::ServiceClient service_client_;
  const raw_ptr<component_updater::ComponentUpdateService>
      component_update_service_;
  UsageTracker usage_tracker_;
  OnDeviceModelAccessController access_controller_{*local_state_};
  ModelBrokerImpl model_broker_impl_;
  PerformanceClassifier performance_classifier_;
  std::vector<ModelBrokerImpl::InitCallback> init_callbacks_;
  ManifestMonitor manifest_monitor_;
  ManifestValidator manifest_validator_;
  std::unique_ptr<ManifestAssetManager> asset_manager_;
  mojo::ReceiverSet<ModelBrokerDebug> receivers_;
  base::WeakPtrFactory<ManifestBrokerState> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_BROKER_STATE_H_
