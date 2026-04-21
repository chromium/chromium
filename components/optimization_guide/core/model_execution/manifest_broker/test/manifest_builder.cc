// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"

#include "base/files/file_util.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/proto/manifest.pb.h"

namespace optimization_guide {

proto::OnDemandComponent OnDemandComponent(const std::string& public_key,
                                           const std::string& target_version) {
  proto::OnDemandComponent component;
  component.set_public_key(public_key);
  component.set_target_version(target_version);
  return component;
}

proto::FileReference FileReference(const std::string& asset_id,
                                   const std::string& relative_path) {
  proto::FileReference file_ref;
  file_ref.set_asset_id(asset_id);
  file_ref.set_relative_path(relative_path);
  return file_ref;
}

BaseModelRecipeArgs::BaseModelRecipeArgs(
    proto::BaseModelRecipe::BackendType backend_type,
    proto::BaseModelRecipe::PerformanceHint performance_hint,
    std::vector<int32_t> supported_ranks,
    int32_t max_tokens)
    : backend_type(backend_type),
      performance_hint(performance_hint),
      supported_ranks(std::move(supported_ranks)),
      max_tokens(max_tokens) {}

BaseModelRecipeArgs::~BaseModelRecipeArgs() = default;

BaseModelRecipeArgs::BaseModelRecipeArgs(const BaseModelRecipeArgs&) = default;

BaseModelRecipeArgs& BaseModelRecipeArgs::operator=(
    const BaseModelRecipeArgs&) = default;

proto::BaseModelRecipe BaseModelRecipe(proto::FileReference weights_file,
                                       BaseModelRecipeArgs args) {
  proto::BaseModelRecipe recipe;
  *recipe.mutable_weights_file() = std::move(weights_file);
  recipe.set_backend_type(args.backend_type);
  recipe.set_performance_hint(args.performance_hint);
  for (int32_t rank : args.supported_ranks) {
    recipe.add_supported_adaptation_ranks(rank);
  }
  recipe.set_max_tokens(args.max_tokens);
  return recipe;
}

proto::AdaptationRecipe AdaptationRecipe(const std::string& base_model_id,
                                         proto::FileReference weights_file) {
  proto::AdaptationRecipe recipe;
  recipe.set_base_model_recipe_id(base_model_id);
  *recipe.mutable_weights_file() = std::move(weights_file);
  return recipe;
}

proto::SafetyModelRecipe SafetyModelRecipe(
    proto::FileReference weights_file,
    proto::FileReference language_detection_model_file) {
  proto::SafetyModelRecipe recipe;
  *recipe.mutable_weights_file() = std::move(weights_file);
  *recipe.mutable_language_detection_model_file() =
      std::move(language_detection_model_file);
  return recipe;
}

proto::SolutionRecipe SolutionRecipe(const std::string& model_recipe_id,
                                     const std::string& safety_model_recipe_id,
                                     proto::FileReference config_file) {
  proto::SolutionRecipe recipe;
  recipe.set_model_recipe_id(model_recipe_id);
  if (!safety_model_recipe_id.empty()) {
    recipe.set_safety_model_recipe_id(safety_model_recipe_id);
  }
  *recipe.mutable_config_file() = std::move(config_file);
  return recipe;
}

ManifestBuilder::ManifestBuilder() = default;
ManifestBuilder::~ManifestBuilder() = default;

ManifestBuilder& ManifestBuilder::Add(const std::string& name,
                                      proto::OnDemandComponent component) {
  (*manifest_.mutable_assets()->mutable_on_demand_components())[name] =
      std::move(component);
  return *this;
}

ManifestBuilder& ManifestBuilder::Add(const std::string& name,
                                      proto::BaseModelRecipe recipe) {
  (*manifest_.mutable_recipes()->mutable_base_models())[name] =
      std::move(recipe);
  return *this;
}

ManifestBuilder& ManifestBuilder::Add(const std::string& name,
                                      proto::AdaptationRecipe recipe) {
  (*manifest_.mutable_recipes()->mutable_adaptations())[name] =
      std::move(recipe);
  return *this;
}

ManifestBuilder& ManifestBuilder::Add(const std::string& name,
                                      proto::SafetyModelRecipe recipe) {
  (*manifest_.mutable_recipes()->mutable_safety_models())[name] =
      std::move(recipe);
  return *this;
}

ManifestBuilder& ManifestBuilder::Add(const std::string& name,
                                      proto::SolutionRecipe recipe) {
  (*manifest_.mutable_recipes()->mutable_solutions())[name] = std::move(recipe);
  return *this;
}

ManifestBuilder& ManifestBuilder::Add(const DeviceUseCase& device_use_case,
                                      const std::string& solution_recipe_id) {
  proto::DeviceCategoryConfig& config =
      (*manifest_.mutable_category_configs())[base::ToString(
          device_use_case.device)];
  (*config.mutable_use_cases())[device_use_case.use_case]
      .set_solution_recipe_id(solution_recipe_id);
  return *this;
}

ManifestBuilder& ManifestBuilder::Add(
    const std::string& name,
    proto::SolutionRecipe recipe,
    const std::vector<DeviceUseCase>& device_use_cases) {
  Add(name, std::move(recipe));
  for (const auto& device_use_case : device_use_cases) {
    Add(device_use_case, name);
  }
  return *this;
}

ManifestBuilder& ManifestBuilder::Add(const DeviceUseCase& use_case,
                                      proto::SolutionRecipe recipe) {
  std::string recipe_id =
      base::ToString(use_case.device) + "_" + use_case.use_case + "_solution";
  Add(recipe_id, std::move(recipe));
  Add(use_case, recipe_id);
  return *this;
}

ManifestBuilder& ManifestBuilder::SetFeatureConfig(DeviceCategory device,
                                                   const std::string& name,
                                                   proto::Any config) {
  proto::DeviceCategoryConfig& category_config =
      (*manifest_.mutable_category_configs())[base::ToString(device)];
  (*category_config.mutable_feature_configs())[name] = std::move(config);
  return *this;
}

proto::Manifest ManifestBuilder::Build() {
  return manifest_;
}

ManifestComponentDirectory::ManifestComponentDirectory(
    const proto::Manifest& manifest) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  Add(manifest);
}
ManifestComponentDirectory::~ManifestComponentDirectory() = default;

ManifestComponentDirectory& ManifestComponentDirectory::Add(
    const proto::Manifest& manifest) {
  CHECK(base::WriteFile(temp_dir_.GetPath().Append(kManifestFileName),
                        manifest.SerializeAsString()));
  return *this;
}

ManifestComponentDirectory& ManifestComponentDirectory::Add(
    const std::string& filename,
    const proto::SolutionConfig& config) {
  CHECK(base::WriteFile(temp_dir_.GetPath().AppendASCII(filename),
                        config.SerializeAsString()));
  return *this;
}

}  // namespace optimization_guide
