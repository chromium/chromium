// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_STATE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_STATE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"

namespace optimization_guide {

// This holds the state for the on-device model broker. This is an abstraction
// to allow chrome and other embedders to share the same broker logic while
// owning the state separately.
class ModelBrokerState {
 public:
  ModelBrokerState(
      PrefService* local_state,
      std::unique_ptr<OnDeviceModelComponentStateManager::Delegate> delegate,
      on_device_model::ServiceClient::LaunchFn launch_fn);
  ~ModelBrokerState();

  ModelBrokerState(const ModelBrokerState&) = delete;
  ModelBrokerState& operator=(const ModelBrokerState&) = delete;

  PerformanceClassifier& performance_classifier() {
    return performance_classifier_;
  }

  OnDeviceModelComponentStateManager& component_state_manager() {
    return component_state_manager_;
  }

  OnDeviceModelServiceController& service_controller() {
    return *service_controller_;
  }

  on_device_model::Capabilities GetPossibleOnDeviceCapabilities() const {
    return performance_classifier_.GetPossibleOnDeviceCapabilities();
  }

  // Executes initialization steps. This is normally called immediately on
  // construction, but can be called later to allow tests to register
  // preferences and other state.
  void Init();

  // Create a new asset manager to provide extra models/configs to the broker.
  std::unique_ptr<OnDeviceAssetManager> CreateAssetManager(
      OptimizationGuideModelProvider* provider);

 private:
  raw_ptr<PrefService> local_state_;
  on_device_model::ServiceClient service_client_;
  PerformanceClassifier performance_classifier_;
  OnDeviceModelComponentStateManager component_state_manager_;
  std::unique_ptr<OnDeviceModelServiceController> service_controller_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_STATE_H_
