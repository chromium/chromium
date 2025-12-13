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
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"

namespace optimization_guide {

// This holds the state for the on-device model broker. This is an abstraction
// to allow chrome and other embedders to share the same broker logic while
// owning the state separately.
class ModelBrokerState final : public OnDeviceCapability {
 public:
  ModelBrokerState(
      PrefService& local_state,
      OptimizationGuideModelProvider& model_provider,
      std::unique_ptr<OnDeviceModelComponentStateManager::Delegate> delegate,
      on_device_model::ServiceClient::LaunchFn launch_fn);
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

  OnDeviceModelServiceController& service_controller() {
    return service_controller_;
  }

  // OnDeviceCapability
  void BindModelBroker(
      mojo::PendingReceiver<mojom::ModelBroker> receiver) override;
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
  std::optional<SamplingParamsConfig> GetSamplingParamsConfig(
      mojom::OnDeviceFeature feature) override;
  std::optional<const optimization_guide::proto::Any> GetFeatureMetadata(
      mojom::OnDeviceFeature feature) override;
  void AddOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature feature,
      OnDeviceModelAvailabilityObserver* observer) override;
  void RemoveOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature feature,
      OnDeviceModelAvailabilityObserver* observer) override;
  on_device_model::Capabilities GetOnDeviceCapabilities() override;

 private:
  void FinishGetOnDeviceModelEligibility(
      mojom::OnDeviceFeature feature,
      const on_device_model::Capabilities& capabilities,
      base::OnceCallback<
          void(optimization_guide::OnDeviceModelEligibilityReason)> callback);

  on_device_model::ServiceClient service_client_;
  UsageTracker usage_tracker_;
  PerformanceClassifier performance_classifier_;
  OnDeviceModelComponentStateManager component_state_manager_;
  OnDeviceModelServiceController service_controller_;
  OnDeviceAssetManager asset_manager_;
  base::WeakPtrFactory<ModelBrokerState> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_STATE_H_
