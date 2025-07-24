// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"

#include <memory>

#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace optimization_guide {

FakeModelBroker::FakeModelBroker(const FakeAdaptationAsset& asset) {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kOptimizationGuideModelExecution, {}},
       {features::internal::kOnDeviceModelTestFeature, {}},
       {features::kOptimizationGuideOnDeviceModel, {}},
       {features::kOnDeviceModelPerformanceParams,
        {{"compatible_on_device_performance_classes", "*"},
         {"compatible_low_tier_on_device_performance_classes", "3"}}},
       {features::kTextSafetyClassifier, {}},
       {features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "0"}}}},
      {});
  model_execution::prefs::RegisterLocalStatePrefs(local_state_.registry());
  UpdatePerformanceClassPref(&local_state_,
                             OnDeviceModelPerformanceClass::kHigh);
  model_broker_state_.Init();
  base_model_.SetReadyIn(model_broker_state_.component_state_manager());
  controller().MaybeUpdateModelAdaptation(asset.feature(), asset.metadata());
}
FakeModelBroker::~FakeModelBroker() = default;

mojo::PendingRemote<mojom::ModelBroker> FakeModelBroker::BindAndPassRemote() {
  mojo::PendingRemote<mojom::ModelBroker> remote;
  controller().BindBroker(remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeModelBroker::UpdateModelAdaptation(const FakeAdaptationAsset& asset) {
  // First clear the current adaptation, then add the new asset to force an
  // update.
  controller().MaybeUpdateModelAdaptation(
      asset.feature(),
      base::unexpected(AdaptationUnavailability::kUpdatePending));
  controller().MaybeUpdateModelAdaptation(asset.feature(), asset.metadata());
}

std::unique_ptr<OnDeviceAssetManager> FakeModelBroker::CreateAssetManager(
    OptimizationGuideModelProvider* provider) {
  return model_broker_state_.CreateAssetManager(provider);
}

}  // namespace optimization_guide
