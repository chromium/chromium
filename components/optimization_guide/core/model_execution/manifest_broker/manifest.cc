// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace optimization_guide {

namespace {

base::expected<Manifest, Manifest::ParseError> ReadManifestFile(
    const base::FilePath& directory,
    DeviceCategory device_category) {
  TRACE_EVENT("optimization_guide", "ReadManifestFile");
  // Unpack and verify model config file.
  std::string binary_manifest_pb;
  if (!base::ReadFileToString(directory.Append(kManifestFileName),
                              &binary_manifest_pb)) {
    return base::unexpected(Manifest::ParseError::kFileNotFound);
  }

  proto::Manifest manifest;
  if (!manifest.ParseFromString(binary_manifest_pb)) {
    return base::unexpected(Manifest::ParseError::kProtoParseError);
  }
  return Manifest::Create(directory, std::move(manifest), device_category);
}

absl::flat_hash_map<std::string, std::string> ComputeAssetIdByPublicKey(
    const proto::Assets& assets) {
  absl::flat_hash_map<std::string, std::string> asset_id_by_public_key;
  for (const auto& [id, component] : assets.on_demand_components()) {
    asset_id_by_public_key[component.public_key()] = id;
  }
  return asset_id_by_public_key;
}

proto::DeviceCategoryConfig SelectDeviceCategoryConfig(
    const proto::Manifest& manifest,
    DeviceCategory device_category) {
  auto it = manifest.category_configs().find(base::ToString(device_category));
  if (it == manifest.category_configs().end()) {
    return {};
  }
  return it->second;
}

// Validates that all identifiers are unique across all types.
std::optional<Manifest::ParseError> CheckUniqueIdentifiers(
    const proto::Manifest& manifest) {
  absl::flat_hash_set<std::string> asset_identifiers;
  asset_identifiers.emplace(kManifestAssetName);
  // Check that all asset ids are unique.
  for (const auto& [id, _] : manifest.assets().on_demand_components()) {
    if (!asset_identifiers.insert(id).second) {
      return Manifest::ParseError::kDuplicateIdentifier;
    }
  }

  absl::flat_hash_set<std::string> model_identifiers;
  for (const auto& [id, _] : manifest.recipes().adaptations()) {
    if (!model_identifiers.insert(id).second) {
      return Manifest::ParseError::kDuplicateIdentifier;
    }
  }
  for (const auto& [id, _] : manifest.recipes().base_models()) {
    if (!model_identifiers.insert(id).second) {
      return Manifest::ParseError::kDuplicateIdentifier;
    }
  }

  return std::nullopt;
}

// Validates that all referenced identifiers exist and are of the correct type.
std::optional<Manifest::ParseError> ValidateReferences(
    const proto::Manifest& manifest) {
  const proto::Recipes& recipes = manifest.recipes();
  auto is_valid_asset = [&](const std::string& asset_id) {
    return asset_id == kManifestAssetName ||
           manifest.assets().on_demand_components().contains(asset_id);
  };
  for (const auto& [_, solution] : manifest.recipes().solutions()) {
    if (!recipes.adaptations().contains(solution.model_recipe_id()) &&
        !recipes.base_models().contains(solution.model_recipe_id())) {
      return Manifest::ParseError::kMissingIdentifier;
    }
    if (!solution.safety_model_recipe_id().empty() &&
        !recipes.safety_models().contains(solution.safety_model_recipe_id())) {
      return Manifest::ParseError::kMissingIdentifier;
    }
    if (!is_valid_asset(solution.config_file().asset_id())) {
      return Manifest::ParseError::kMissingIdentifier;
    }
  }

  for (const auto& [_, adaptation] : manifest.recipes().adaptations()) {
    if (!recipes.base_models().contains(adaptation.base_model_recipe_id())) {
      return Manifest::ParseError::kMissingIdentifier;
    }
    if (!is_valid_asset(adaptation.weights_file().asset_id())) {
      return Manifest::ParseError::kMissingIdentifier;
    }
  }

  for (const auto& [_, base_model] : manifest.recipes().base_models()) {
    if (!is_valid_asset(base_model.weights_file().asset_id())) {
      return Manifest::ParseError::kMissingIdentifier;
    }
  }

  for (const auto& [_, safety_model] : manifest.recipes().safety_models()) {
    if (!is_valid_asset(safety_model.weights_file().asset_id())) {
      return Manifest::ParseError::kMissingIdentifier;
    }
  }

  for (const auto& [category, config] : manifest.category_configs()) {
    for (const auto& [use_case, use_case_config] : config.use_cases()) {
      if (!recipes.solutions().contains(use_case_config.solution_recipe_id())) {
        return Manifest::ParseError::kMissingIdentifier;
      }
    }
  }

  return std::nullopt;
}

struct References {
  absl::flat_hash_set<std::string> solutions;
  absl::flat_hash_set<std::string> safety_models;
  absl::flat_hash_set<std::string> adaptations;
  absl::flat_hash_set<std::string> base_models;
  absl::flat_hash_set<Manifest::AssetId> assets;

  // Adds references to the solutions for all of the use cases.
  void AddAllUseCases(const proto::DeviceCategoryConfig& config) {
    for (const auto& [_, use_case_config] : config.use_cases()) {
      solutions.insert(use_case_config.solution_recipe_id());
    }
  }

  // Adds the solution for the given use case, if it is defined.  Returns true
  // if the use case was found, false otherwise.
  bool AddUseCase(const proto::DeviceCategoryConfig& config,
                  const Manifest::UseCaseName& name) {
    auto it = config.use_cases().find(name);
    if (it != config.use_cases().end()) {
      solutions.insert(it->second.solution_recipe_id());
      return true;
    }
    return false;
  }

  // Adds all of the recipes and assets that are needed to support the already
  // referenced recipes.  Recipes should be a valid transitive closure, and
  // references should be valid against the recipes.
  void AddDependencies(const proto::Recipes& recipes) {
    for (const auto& id : solutions) {
      const proto::SolutionRecipe& solution = recipes.solutions().at(id);
      if (recipes.adaptations().contains(solution.model_recipe_id())) {
        adaptations.insert(solution.model_recipe_id());
      } else {
        base_models.insert(solution.model_recipe_id());
      }
      if (solution.has_safety_model_recipe_id()) {
        safety_models.insert(solution.safety_model_recipe_id());
      }
      assets.insert(solution.config_file().asset_id());
    }
    for (const auto& id : adaptations) {
      const proto::AdaptationRecipe& adaptation = recipes.adaptations().at(id);
      base_models.insert(adaptation.base_model_recipe_id());
      assets.insert(adaptation.weights_file().asset_id());
    }
    for (const auto& id : base_models) {
      const proto::BaseModelRecipe& base_model = recipes.base_models().at(id);
      assets.insert(base_model.weights_file().asset_id());
    }
    for (const auto& id : safety_models) {
      const proto::SafetyModelRecipe& safety_model =
          recipes.safety_models().at(id);
      assets.insert(safety_model.weights_file().asset_id());
    }
  }

  // Constructs a new Recipes message with only the referenced recipes.
  proto::Recipes CopyReferencedRecipes(const proto::Recipes& original) {
    proto::Recipes referenced;
    for (const auto& id : solutions) {
      referenced.mutable_solutions()->emplace(id, original.solutions().at(id));
    }
    for (const auto& id : adaptations) {
      referenced.mutable_adaptations()->emplace(id,
                                                original.adaptations().at(id));
    }
    for (const auto& id : base_models) {
      referenced.mutable_base_models()->emplace(id,
                                                original.base_models().at(id));
    }
    for (const auto& id : safety_models) {
      referenced.mutable_safety_models()->emplace(
          id, original.safety_models().at(id));
    }
    return referenced;
  }

  // Constructs a new Assets message with only the referenced assets.
  proto::Assets CopyReferencedAssets(const proto::Assets& original) {
    proto::Assets referenced;
    for (const auto& id : assets) {
      if (id == kManifestAssetName) {
        continue;
      }
      referenced.mutable_on_demand_components()->emplace(
          id, original.on_demand_components().at(id));
    }
    return referenced;
  }
};

// Validates that there are no two components with the same public key.
// This would cause a conflict, as we can only download one version of a
// component.
std::optional<Manifest::ParseError> ValidateUniqueComponent(
    const proto::Assets& assets) {
  absl::flat_hash_set<std::string> public_keys;
  for (const auto& [id, component] : assets.on_demand_components()) {
    if (!public_keys.insert(component.public_key()).second) {
      return Manifest::ParseError::kConflictingComponent;
    }
  }
  return std::nullopt;
}

}  // namespace

std::ostream& operator<<(std::ostream& stream, DeviceCategory device_category) {
  switch (device_category) {
    case DeviceCategory::kGpuHighTier:
      stream << "gpu_high_tier";
      break;
    case DeviceCategory::kGpuLowTier:
      stream << "gpu_low_tier";
      break;
    case DeviceCategory::kCpu:
      stream << "cpu";
      break;
  }
  return stream;
}

// static
base::expected<Manifest, Manifest::ParseError> Manifest::Create(
    const base::FilePath& directory,
    proto::Manifest manifest,
    DeviceCategory device_category) {
  if (auto error = CheckUniqueIdentifiers(manifest); error.has_value()) {
    return base::unexpected(error.value());
  }

  if (auto error = ValidateReferences(manifest); error.has_value()) {
    return base::unexpected(error.value());
  }

  proto::DeviceCategoryConfig device_category_config =
      SelectDeviceCategoryConfig(manifest, device_category);

  // Narrow down the recipe maps to only those recipes that are reachable for
  // the use cases relevant to the device category.
  References references;
  references.AddAllUseCases(device_category_config);
  references.AddDependencies(manifest.recipes());
  proto::Recipes recipes = references.CopyReferencedRecipes(manifest.recipes());
  proto::Assets assets = references.CopyReferencedAssets(manifest.assets());
  if (auto error = ValidateUniqueComponent(assets); error.has_value()) {
    return base::unexpected(error.value());
  }

  return Manifest(directory, std::move(device_category_config),
                  std::move(recipes), std::move(assets));
}

void Manifest::Load(
    const base::FilePath& directory,
    DeviceCategory device_category,
    base::OnceCallback<void(base::expected<Manifest, ParseError>)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadManifestFile, directory, device_category),
      std::move(callback));
}

Manifest::Manifest(Manifest::UninstallReason uninstall_reason)
    : uninstall_reason_(uninstall_reason) {}
Manifest::Manifest(base::FilePath directory,
                   proto::DeviceCategoryConfig device_category_config,
                   proto::Recipes recipes,
                   proto::Assets assets)
    : directory_(std::move(directory)),
      device_category_config_(std::move(device_category_config)),
      recipes_(std::move(recipes)),
      assets_(std::move(assets)),
      asset_id_by_public_key_(ComputeAssetIdByPublicKey(assets_)) {}

Manifest::~Manifest() = default;

Manifest::Manifest(const Manifest&) = default;
Manifest& Manifest::operator=(const Manifest&) = default;
Manifest::Manifest(Manifest&&) = default;
Manifest& Manifest::operator=(Manifest&&) = default;

bool Manifest::HasAssets() const {
  return !assets_.on_demand_components().empty();
}

const proto::OnDemandComponent* Manifest::GetAssetByPublicKey(
    const std::string& public_key) const {
  auto it = asset_id_by_public_key_.find(public_key);
  if (it == asset_id_by_public_key_.end()) {
    return nullptr;
  }
  return &assets_.on_demand_components().at(it->second);
}

std::optional<absl::flat_hash_set<Manifest::AssetId>>
Manifest::GetRequiredAssets(const UseCaseName& use_case) const {
  References references;
  if (!references.AddUseCase(device_category_config_, use_case)) {
    return std::nullopt;
  }
  references.AddDependencies(recipes_);
  return references.assets;
}

absl::flat_hash_set<Manifest::AssetId> Manifest::GetRequiredAssets(
    const std::vector<UseCaseName>& use_cases) const {
  References references;
  for (const auto& use_case : use_cases) {
    references.AddUseCase(device_category_config_, use_case);
  }
  references.AddDependencies(recipes_);
  return references.assets;
}

}  // namespace optimization_guide
