// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"

#include <memory>

#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace optimization_guide {

ScopedModelBrokerFeatureList::ScopedModelBrokerFeatureList() {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kOptimizationGuideModelExecution, {}},
       {features::kOptimizationGuideOnDeviceModel, {}},
       {features::kOnDeviceModelPerformanceParams,
        {{"compatible_on_device_performance_classes", "3,4,5,6"},
         {"compatible_low_tier_on_device_performance_classes", "3"}}},
       {features::kTextSafetyClassifier, {}},
       {features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "0"}}}},
      {});
}
ScopedModelBrokerFeatureList::~ScopedModelBrokerFeatureList() = default;

ModelBrokerPrefService::ModelBrokerPrefService() {
  model_execution::prefs::RegisterLocalStatePrefs(local_state_.registry());
}
ModelBrokerPrefService::~ModelBrokerPrefService() = default;

FakeModelBroker::FakeModelBroker(const Options& options) {
  if (options.performance_class != OnDeviceModelPerformanceClass::kUnknown) {
    UpdatePerformanceClassPref(&local_state_.local_state(),
                               options.performance_class);
  }
  if (options.preinstall_base_model) {
    InstallBaseModel(std::make_unique<FakeBaseModelAsset>());
  }
}
FakeModelBroker::~FakeModelBroker() = default;

mojo::PendingRemote<mojom::ModelBroker> FakeModelBroker::BindAndPassRemote() {
  mojo::PendingRemote<mojom::ModelBroker> remote;
  GetOrCreateBrokerState().service_controller().BindBroker(
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeModelBroker::InstallBaseModel(FakeBaseModelAsset::Content content) {
  InstallBaseModel(std::make_unique<FakeBaseModelAsset>(std::move(content)));
}

void FakeModelBroker::InstallBaseModel(
    std::unique_ptr<FakeBaseModelAsset> asset) {
  component_state_.Install(std::move(asset));
}

void FakeModelBroker::UpdateTarget(proto::OptimizationTarget target,
                                   const ModelInfo& model_info) {
  model_provider_.UpdateModelImmediatelyForTesting(
      target, std::make_unique<ModelInfo>(model_info));
}

void FakeModelBroker::UpdateModelAdaptation(const FakeAdaptationAsset& asset) {
  UpdateTarget(GetOptimizationTargetForFeature(asset.feature()),
               asset.model_info());
}

void FakeModelBroker::UpdateSafetyModel(const FakeSafetyModelAsset& asset) {
  UpdateTarget(proto::OPTIMIZATION_TARGET_TEXT_SAFETY, asset.model_info());
}

void FakeModelBroker::UpdateLanguageDetectionModel(
    const FakeLanguageModelAsset& asset) {
  UpdateTarget(proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
               asset.model_info());
}

ModelBrokerState& FakeModelBroker::GetOrCreateBrokerState() {
  if (!model_broker_state_) {
    model_broker_state_.emplace(local_state_.local_state(), model_provider_,
                                component_state_.CreateDelegate(),
                                fake_launcher_.LaunchFn());
  }
  return *model_broker_state_;
}

}  // namespace optimization_guide
