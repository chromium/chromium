// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_model_broker_android.h"

#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "services/on_device_model/android/model_downloader_android.h"

namespace optimization_guide {

ScopedModelBrokerAndroidFeatureList::ScopedModelBrokerAndroidFeatureList() {
  feature_list_.InitWithFeaturesAndParameters(
      {
          {features::kOptimizationGuideModelExecution, {}},
          {features::kOptimizationGuideOnDeviceModel, {}},
          {features::kAICorePrompt, {}},
          {features::kAICoreScamDetection, {}},
          {features::kAICoreTest, {}},
      },
      {features::kRequirePersistentModeForScamDetection});
}

ScopedModelBrokerAndroidFeatureList::~ScopedModelBrokerAndroidFeatureList() =
    default;

FakeModelBrokerAndroid::FakeModelBrokerAndroid() {
  java_helper_.SetMockAiCoreFactory();
  java_helper_.settings().SetDefaultStatusCheckResult(
      on_device_model::ModelDownloaderAndroid::ModelStatus::kAvailable);
}

FakeModelBrokerAndroid::~FakeModelBrokerAndroid() = default;

mojo::PendingRemote<mojom::ModelBroker>
FakeModelBrokerAndroid::BindAndPassRemote() {
  mojo::PendingRemote<mojom::ModelBroker> remote;
  EnsureBroker().BindModelBroker(remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeModelBrokerAndroid::UpdateModelAdaptation(
    const FakeAdaptationAsset& asset) {
  UpdateTarget(GetOptimizationTargetForFeature(asset.feature()),
               asset.model_info());
}

ModelBrokerAndroid& FakeModelBrokerAndroid::EnsureBroker() {
  if (!broker_) {
    broker_.emplace(local_state_.local_state(), model_provider_);
  }
  return *broker_;
}

void FakeModelBrokerAndroid::UpdateTarget(proto::OptimizationTarget target,
                                          const ModelInfo& model_info) {
  model_provider_.UpdateModelImmediatelyForTesting(
      target, std::make_unique<ModelInfo>(model_info));
}

}  // namespace optimization_guide
