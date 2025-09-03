// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_state.h"

#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

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

}  // namespace optimization_guide
