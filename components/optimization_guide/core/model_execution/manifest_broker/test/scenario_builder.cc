// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/test/scenario_builder.h"

#include <memory>

#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"

namespace optimization_guide {

ScenarioBuilder::ScenarioBuilder(
    TestManifestAssetManagerComponentState& component_state)
    : state_(component_state) {
  manifest_directory_ =
      std::make_unique<ManifestComponentDirectory>(proto::Manifest{});
}

ScenarioBuilder::~ScenarioBuilder() = default;

ScenarioBuilder& ScenarioBuilder::AddBaseModel(const std::string& name) {
  AddBaseModel(
      name,
      BaseModelRecipeArgs(
          proto::BaseModelRecipe::BACKEND_TYPE_GPU,
          proto::BaseModelRecipe::PERFORMANCE_HINT_HIGHEST_QUALITY, {}, 100));
  return *this;
}

ScenarioBuilder& ScenarioBuilder::AddBaseModel(const std::string& name,
                                               BaseModelRecipeArgs args) {
  state_->UpdateBaseModel(name + "_key", []() {
    auto base_model_asset =
        std::make_unique<FakeBaseModelAsset>(FakeBaseModelAsset::Content{
            .cache_weight = 1015,
            .encoder_cache_weight = 1016,
            .adapter_cache_weight = 1017,
        });
    base_model_asset->set_version("1.0.0.0");
    return base_model_asset;
  }());
  builder.Add(name + "_asset", OnDemandComponent(name + "_key", "1.0.0.0"));
  builder.Add(
      name + "_recipe",
      BaseModelRecipe(FileReference(name + "_asset", "weights.bin"), args));
  return *this;
}

ScenarioBuilder& ScenarioBuilder::AddSafetyModel(const std::string& name) {
  state_->UpdateSafetyModel(
      name + "_key",
      std::make_unique<FakeSafetyModelAsset>(FakeSafetyModelAsset::Content{
          .model_info_version = 1,
      }));
  builder.Add(name + "_asset", OnDemandComponent(name + "_key", "1"));
  builder.Add(name + "_recipe",
              SafetyModelRecipe(FileReference(name + "_asset", "ts.bin"),
                                FileReference(name + "_asset", "lang.bin")));
  return *this;
}

ScenarioBuilder& ScenarioBuilder::AddAdaptation(const std::string& name,
                                                const std::string& base_model) {
  state_->UpdateModelAdaptation(
      name + "_key",
      std::make_unique<FakeAdaptationAsset>(FakeAdaptationAsset::Content{
          .config = SimpleComposeConfig(),
          .weight = 1,
      }));
  builder.Add(name + "_asset", OnDemandComponent(name + "_key", "12345"));
  builder.Add(name + "_recipe",
              AdaptationRecipe(
                  base_model + "_recipe",
                  FileReference(name + "_asset", "adaptation_weights.bin")));
  return *this;
}

ScenarioBuilder& ScenarioBuilder::AddUnsafeSolution(const std::string& use_case,
                                                    const std::string& model) {
  manifest_directory_->Add(use_case + "config.pb", []() {
    proto::SolutionConfig solution_config;
    *solution_config.mutable_feature() = SimpleTestFeatureConfig();
    return solution_config;
  }());
  builder.Add(
      use_case + "_solution",
      SolutionRecipe(model + "_recipe", "",
                     FileReference("manifest", use_case + "config.pb")));
  builder.Add(DeviceUseCase{DeviceCategory::kGpuHighTier, use_case},
              use_case + "_solution");
  return *this;
}

ScenarioBuilder& ScenarioBuilder::AddSafeSolution(
    const std::string& use_case,
    const std::string& model,
    const std::string& safety_model,
    proto::SolutionConfig config) {
  manifest_directory_->Add(use_case + "config.pb", std::move(config));
  builder.Add(
      use_case + "_solution",
      SolutionRecipe(model + "_recipe", safety_model + "_recipe",
                     FileReference("manifest", use_case + "config.pb")));
  builder.Add(DeviceUseCase{DeviceCategory::kGpuHighTier, use_case},
              use_case + "_solution");
  return *this;
}

ScenarioBuilder& ScenarioBuilder::SetFeatureConfig(DeviceCategory category,
                                                   const std::string& use_case,
                                                   const proto::Any& config) {
  builder.SetFeatureConfig(category, use_case, config);
  return *this;
}

void ScenarioBuilder::Finish() {
  manifest_directory_->Add(builder.Build());
  state_->UpdateManifest(std::move(manifest_directory_));
}

// static
void ScenarioBuilder::MinimalTestScenario(
    TestManifestAssetManagerComponentState& component_state) {
  ScenarioBuilder(component_state)
      .AddBaseModel("model_A")
      .AddUnsafeSolution("test", "model_A")
      .Finish();
}

}  // namespace optimization_guide
