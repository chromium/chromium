// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_model_broker_android.h"

#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/optimization_guide/core/model_execution/test/fake_android_manifest_asset.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "services/on_device_model/android/model_downloader_android.h"

namespace optimization_guide {

ScopedModelBrokerAndroidFeatureList::ScopedModelBrokerAndroidFeatureList() {
  feature_list_.InitWithFeaturesAndParameters(
      {
          {features::kOptimizationGuideModelExecution, {}},
          {features::kAICorePrompt, {}},
          {features::kAICoreScamDetection, {}},
          {features::kAICoreTest, {}},
      },
      {features::kRequirePersistentModeForScamDetection});
}

ScopedModelBrokerAndroidFeatureList::~ScopedModelBrokerAndroidFeatureList() =
    default;

FakeModelBrokerAndroid::FakeModelBrokerAndroid(const Options& options)
    : options_(options) {
  model_execution::prefs::RegisterLocalStatePrefs(local_state_.registry());
  java_helper_.SetMockAiCoreFactory();
  java_helper_.settings().SetDefaultStatusCheckResult(
      on_device_model::ModelDownloaderAndroid::ModelStatus::kAvailable);

  if (options.preinstall_base_model) {
    InstallBaseModel();
  }
}

FakeModelBrokerAndroid::~FakeModelBrokerAndroid() = default;

mojo::PendingRemote<mojom::ModelBroker>
FakeModelBrokerAndroid::BindAndPassRemote() {
  mojo::PendingRemote<mojom::ModelBroker> remote;
  EnsureBroker().BindModelBroker(remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeModelBrokerAndroid::InstallBaseModel() {
  java_helper_.TriggerDownloaderOnAvailable(options_.base_model_spec.name,
                                            options_.base_model_spec.version);
}

void FakeModelBrokerAndroid::UnInstallBaseModel() {
  java_helper_.UnInstallModel();
}

void FakeModelBrokerAndroid::UpdateManifest(
    std::unique_ptr<FakeAndroidManifestAsset> asset) {
  EnsureBroker().OnManifestLoaded(asset->CreateManifest());
}

ModelBrokerAndroid& FakeModelBrokerAndroid::EnsureBroker() {
  if (!broker_) {
    broker_.emplace(local_state_);
  }
  return *broker_;
}

}  // namespace optimization_guide
