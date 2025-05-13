// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"

#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
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
  model_execution::prefs::RegisterProfilePrefs(pref_service_.registry());
  model_execution::prefs::RegisterLocalStatePrefs(pref_service_.registry());
  pref_service_.SetInteger(
      model_execution::prefs::localstate::kOnDevicePerformanceClass,
      base::to_underlying(OnDeviceModelPerformanceClass::kHigh));
  auto access_controller =
      std::make_unique<OnDeviceModelAccessController>(pref_service_);
  test_controller_ = base::MakeRefCounted<OnDeviceModelServiceController>(
      std::move(access_controller), component_manager_.get()->GetWeakPtr(),
      fake_launcher_.LaunchFn());
  test_controller_->Init();
  component_manager_.SetReady(base_model_);
  test_controller_->MaybeUpdateModelAdaptation(asset.feature(),
                                               asset.metadata());
}
FakeModelBroker::~FakeModelBroker() = default;

mojo::PendingRemote<mojom::ModelBroker> FakeModelBroker::BindAndPassRemote() {
  mojo::PendingRemote<mojom::ModelBroker> remote;
  test_controller_->BindBroker(remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeModelBroker::UpdateModelAdaptation(const FakeAdaptationAsset& asset) {
  // First clear the current adaptation, then add the new asset to force an
  // update.
  test_controller_->MaybeUpdateModelAdaptation(asset.feature(), nullptr);
  test_controller_->MaybeUpdateModelAdaptation(asset.feature(),
                                               asset.metadata());
}

}  // namespace optimization_guide
