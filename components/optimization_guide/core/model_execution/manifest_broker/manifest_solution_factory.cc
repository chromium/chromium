// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_solution_factory.h"

#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "services/on_device_model/public/cpp/model_assets.h"

namespace optimization_guide {

namespace {

ml::ModelBackendType ConvertBackendType(
    proto::BaseModelRecipe::BackendType type) {
  switch (type) {
    case proto::BaseModelRecipe::BACKEND_TYPE_GPU:
      return ml::ModelBackendType::kGpuBackend;
    case proto::BaseModelRecipe::BACKEND_TYPE_CPU:
      return ml::ModelBackendType::kCpuBackend;
    default:
      return ml::ModelBackendType::kGpuBackend;
  }
}

ml::ModelPerformanceHint ConvertPerformanceHint(
    proto::BaseModelRecipe::PerformanceHint hint) {
  switch (hint) {
    case proto::BaseModelRecipe::PERFORMANCE_HINT_FASTEST_INFERENCE:
      return ml::ModelPerformanceHint::kFastestInference;
    case proto::BaseModelRecipe::PERFORMANCE_HINT_HIGHEST_QUALITY:
      return ml::ModelPerformanceHint::kHighestQuality;
    default:
      // Default to something reasonable if unspecified, though explicit is
      // better.
      return ml::ModelPerformanceHint::kFastestInference;
  }
}

base::flat_map<std::string, ManifestSolutionFactory::AssetState> MakeAssetsMap(
    const proto::Assets& assets) {
  std::vector<std::pair<std::string, ManifestSolutionFactory::AssetState>>
      states;
  states.reserve(assets.on_demand_components().size() + 1);
  states.emplace_back(
      kManifestAssetName,
      base::unexpected(
          ManifestSolutionFactory::AssetUnavailableReason::kUninitialized));
  for (const auto& [name, _] : assets.on_demand_components()) {
    states.emplace_back(
        name,
        base::unexpected(
            ManifestSolutionFactory::AssetUnavailableReason::kUninitialized));
  }
  return base::flat_map<std::string, ManifestSolutionFactory::AssetState>(
      std::move(states));
}

base::flat_map<std::string, ManifestSolutionFactory::BaseModelState>
MakeBaseModelsMap(const proto::Recipes& recipes) {
  std::vector<std::pair<std::string, ManifestSolutionFactory::BaseModelState>>
      states;
  states.reserve(recipes.base_models().size());
  for (const auto& [name, recipe] : recipes.base_models()) {
    states.emplace_back(name, ManifestSolutionFactory::BaseModelState());
  }
  return base::flat_map<std::string, ManifestSolutionFactory::BaseModelState>(
      std::move(states));
}

base::flat_map<std::string, ManifestSolutionFactory::AdaptationState>
MakeAdaptationsMap(const proto::Recipes& recipes) {
  std::vector<std::pair<std::string, ManifestSolutionFactory::AdaptationState>>
      states;
  states.reserve(recipes.adaptations().size());
  for (const auto& [name, recipe] : recipes.adaptations()) {
    states.emplace_back(name, ManifestSolutionFactory::AdaptationState());
  }
  return base::flat_map<std::string, ManifestSolutionFactory::AdaptationState>(
      std::move(states));
}

base::flat_map<std::string, ManifestSolutionFactory::SafetyModelState>
MakeSafetyModelsMap(const proto::Recipes& recipes) {
  std::vector<std::pair<std::string, ManifestSolutionFactory::SafetyModelState>>
      states;
  states.reserve(recipes.safety_models().size());
  for (const auto& [name, recipe] : recipes.safety_models()) {
    states.emplace_back(name, ManifestSolutionFactory::SafetyModelState());
  }
  return base::flat_map<std::string, ManifestSolutionFactory::SafetyModelState>(
      std::move(states));
}

base::flat_map<std::string, ManifestSolutionFactory::SolutionState>
MakeSolutionsMap(const proto::Recipes& recipes) {
  std::vector<std::pair<std::string, ManifestSolutionFactory::SolutionState>>
      states;
  states.reserve(recipes.solutions().size());
  for (const auto& [name, recipe] : recipes.solutions()) {
    states.emplace_back(name, ManifestSolutionFactory::SolutionState());
  }
  return base::flat_map<std::string, ManifestSolutionFactory::SolutionState>(
      std::move(states));
}

void CloseAssetsInBackground(on_device_model::ModelAssets assets) {
  // Close the files on a background thread.
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::DoNothingWithBoundArgs(std::move(assets)));
}

void CloseAssetsInBackground(on_device_model::AdaptationAssets assets) {
  // Close the files on a background thread.
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::DoNothingWithBoundArgs(std::move(assets)));
}

void CloseAssetsInBackground(
    on_device_model::mojom::TextSafetyModelParamsPtr params) {
  // Close the files on a background thread.
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::DoNothingWithBoundArgs(std::move(params)));
}

}  // namespace

// A Solution for backed by a ManifestSolutionFactory.
class ManifestSolutionFactory::Solution : public ModelBrokerImpl::Solution {
 public:
  // Constructs a Solution for the given use case using solution_id.
  // Constructing this implies all of the required assets are available.
  Solution(base::WeakPtr<ManifestSolutionFactory> factory,
           const std::string use_case,
           const std::string solution_id)
      : factory_(factory), solution_id_(std::move(solution_id)) {}

  ~Solution() override = default;

  const proto::SolutionRecipe& recipe() const {
    return factory_->manifest_.GetRecipes().solutions().at(solution_id_);
  }

  const ManifestSolutionFactory::SolutionState& state() const {
    return factory_->solutions_.at(solution_id_);
  }

  bool IsValid() const override {
    // Individual assets can't become unavailable, so the solution is valid
    // as long as we continue to use the same manifest.
    // If we support the broker changing to alternative solutions, then we will
    // need to invalidate these solutions when the broker changes.
    return !!factory_;
  }

  mojom::ModelSolutionConfigPtr MakeConfig() const override {
    if (!factory_) {
      return nullptr;
    }
    auto config = mojom::ModelSolutionConfig::New();
    config->feature_config = mojo_base::ProtoWrapper(state().config_.feature());
    config->text_safety_config =
        mojo_base::ProtoWrapper(state().config_.safety());
    config->model_versions =
        mojo_base::ProtoWrapper(state().config_.model_versions());
    return config;
  }

  void CreateSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> pending,
      on_device_model::mojom::SessionParamsPtr params) override {
    if (!factory_) {
      return;
    }
    factory_->GetOrLoadModel(recipe().model_recipe_id())
        ->StartSession(std::move(pending), std::move(params));
  }

  void CreateTextSafetySession(
      mojo::PendingReceiver<on_device_model::mojom::TextSafetySession> pending)
      override {
    if (!factory_) {
      return;
    }
    factory_->GetOrLoadTextSafetyModel(recipe().safety_model_recipe_id())
        ->StartSession(std::move(pending));
  }

  void ReportHealthyCompletion() override {
    // No-op
  }

 private:
  base::WeakPtr<ManifestSolutionFactory> factory_;
  std::string use_case_;
  std::string solution_id_;
};

ManifestSolutionFactory::BaseModelState::BaseModelState() = default;
ManifestSolutionFactory::BaseModelState::~BaseModelState() = default;
ManifestSolutionFactory::BaseModelState::BaseModelState(
    BaseModelState&& other) = default;
ManifestSolutionFactory::BaseModelState&
ManifestSolutionFactory::BaseModelState::operator=(BaseModelState&& other) =
    default;

ManifestSolutionFactory::AdaptationState::AdaptationState() = default;
ManifestSolutionFactory::AdaptationState::~AdaptationState() = default;
ManifestSolutionFactory::AdaptationState::AdaptationState(
    AdaptationState&& other) = default;
ManifestSolutionFactory::AdaptationState&
ManifestSolutionFactory::AdaptationState::operator=(AdaptationState&& other) =
    default;

ManifestSolutionFactory::SafetyModelState::SafetyModelState() = default;
ManifestSolutionFactory::SafetyModelState::~SafetyModelState() = default;
ManifestSolutionFactory::SafetyModelState::SafetyModelState(
    SafetyModelState&& other) = default;
ManifestSolutionFactory::SafetyModelState&
ManifestSolutionFactory::SafetyModelState::operator=(SafetyModelState&& other) =
    default;

ManifestSolutionFactory::SolutionState::SolutionState() = default;
ManifestSolutionFactory::SolutionState::~SolutionState() = default;
ManifestSolutionFactory::SolutionState::SolutionState(SolutionState&& other) =
    default;
ManifestSolutionFactory::SolutionState&
ManifestSolutionFactory::SolutionState::operator=(SolutionState&& other) =
    default;

ManifestSolutionFactory::ManifestSolutionFactory(
    Manifest manifest,
    ModelBrokerImpl& broker_impl,
    UsageTracker& usage_tracker,
    on_device_model::ServiceClient& service_client,
    base::OnceClosure on_init_complete)
    : broker_impl_(broker_impl),
      service_client_(service_client),
      usage_tracker_(usage_tracker),
      manifest_(std::move(manifest)),
      assets_(MakeAssetsMap(manifest_.GetAssets())),
      base_models_(MakeBaseModelsMap(manifest_.GetRecipes())),
      adaptations_(MakeAdaptationsMap(manifest_.GetRecipes())),
      safety_models_(MakeSafetyModelsMap(manifest_.GetRecipes())),
      solutions_(MakeSolutionsMap(manifest_.GetRecipes())),
      on_asset_init_(
          base::BarrierClosure(assets_.size(), std::move(on_init_complete))) {}
ManifestSolutionFactory::~ManifestSolutionFactory() = default;

void ManifestSolutionFactory::UpdateAssetState(const std::string& asset_id,
                                               AssetState new_state) {
  TRACE_EVENT("optimization_guide", "ManifestSolutionFactory::UpdateAssetState",
              "asset_id", asset_id, "is_available", new_state.has_value());
  CHECK(new_state != base::unexpected(AssetUnavailableReason::kUninitialized));
  auto it = assets_.find(asset_id);
  // We already know all possible assets from the manifest.
  CHECK(it != assets_.end());

  // We shouldn't support an available asset being updated.
  // TODO(holte): Stop uninstalling assets a factory might have been provided.
  if (it->second.has_value()) {
    return;
  }

  // Call UpdateSolutions after any config loads complete
  base::OnceClosure on_complete =
      base::BindOnce(&ManifestSolutionFactory::UpdateSolutions,
                     weak_ptr_factory_.GetWeakPtr());

  // If this is initializing the asset, track progress of initialization.
  if (it->second == base::unexpected(AssetUnavailableReason::kUninitialized)) {
    on_complete = std::move(on_complete).Then(on_asset_init_);
  }
  it->second = new_state;

  if (it->second.has_value()) {
    // Start loading all of the configs provided by this asset.
    LoadSolutionConfigsFrom(asset_id, std::move(on_complete));
  } else {
    // No asset to load things from, so we're done.
    std::move(on_complete).Run();
  }
}

void ManifestSolutionFactory::UpdateSolutions() {
  TRACE_EVENT("optimization_guide", "ManifestSolutionFactory::UpdateSolutions");
  for (const auto& [use_case_name, _] :
       manifest_.GetDeviceCategoryConfig().use_cases()) {
    broker_impl_->GetSolutionProvider(use_case_name)
        .Update(CreateSolutionForUseCase(use_case_name));
  }
}

std::vector<mojom::BrokerModelInfoPtr>
ManifestSolutionFactory::GetBrokerModels() const {
  std::vector<mojom::BrokerModelInfoPtr> result;
  for (const auto& [model_id, state] : base_models_) {
    const auto& recipe = manifest_.GetRecipes().base_models().at(model_id);
    auto info = mojom::BrokerModelInfo::New();
    info->name = model_id;
    if (auto path = ResolveFile(recipe.weights_file())) {
      info->weights_path = path->AsUTF8Unsafe();
    }
    result.push_back(std::move(info));
  }
  return result;
}

std::optional<base::FilePath> ManifestSolutionFactory::ResolveFile(
    const proto::FileReference& file) const {
  auto it = assets_.find(file.asset_id());
  if (it == assets_.end() || !it->second.has_value()) {
    return std::nullopt;
  }
  return it->second->path.AppendASCII(file.relative_path());
}

void ManifestSolutionFactory::LoadSolutionConfigsFrom(
    const std::string& asset_id,
    base::OnceClosure on_complete) {
  TRACE_EVENT("optimization_guide",
              "ManifestSolutionFactory::LoadSolutionConfigsFrom", "asset_id",
              asset_id);
  int count = 0;
  for (const auto& [solution_id, recipe] : manifest_.GetRecipes().solutions()) {
    if (recipe.config_file().asset_id() == asset_id) {
      ++count;
    }
  }
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(count, std::move(on_complete));
  for (const auto& [solution_id, recipe] : manifest_.GetRecipes().solutions()) {
    if (recipe.config_file().asset_id() == asset_id) {
      LoadSolutionConfig(solution_id, *ResolveFile(recipe.config_file()),
                         barrier_closure);
    }
  }
}

void ManifestSolutionFactory::LoadSolutionConfig(
    const std::string& solution_id,
    const base::FilePath& config_path,
    base::OnceClosure on_complete) {
  TRACE_EVENT("optimization_guide",
              "ManifestSolutionFactory::LoadSolutionConfig", "solution_id",
              solution_id);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](base::FilePath config_path)
              -> std::unique_ptr<proto::SolutionConfig> {
            TRACE_EVENT("optimization_guide",
                        "ManifestSolutionFactory::LoadSolutionConfig");
            std::string config_data;
            if (!base::ReadFileToString(config_path, &config_data)) {
              return nullptr;
            }
            auto config = std::make_unique<proto::SolutionConfig>();
            if (!config->ParseFromString(config_data)) {
              return nullptr;
            }
            return config;
          },
          std::move(config_path)),
      base::BindOnce(
          [](base::WeakPtr<ManifestSolutionFactory> factory,
             const std::string& solution_id,
             std::unique_ptr<proto::SolutionConfig> config) {
            TRACE_EVENT("optimization_guide",
                        "ManifestSolutionFactory::OnSolutionConfigLoaded",
                        "solution_id", solution_id);
            if (!factory) {
              return;
            }
            auto& state = factory->solutions_.at(solution_id);
            if (config) {
              state.config_ = std::move(*config);
              state.status_ = SolutionState::kReady;
            } else {
              state.status_ = SolutionState::kFailed;
            }
            // Note: There should already be a deferred UpdateSolutions call
            // from UpdateAssetState that will be called after this.
          },
          weak_ptr_factory_.GetWeakPtr(), solution_id)
          .Then(std::move(on_complete)));
}

ModelBrokerImpl::MaybeSolution
ManifestSolutionFactory::CreateSolutionForUseCase(
    const std::string& use_case_name) {
  auto required_assets = manifest_.GetRequiredAssets(use_case_name);
  if (!required_assets) {
    // Use case not found in manifest.
    return base::unexpected(
        OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature);
  }
  // Check that all assets are available.
  bool has_unavailable_asset = false;
  for (const auto& asset : *required_assets) {
    const AssetState& state = assets_.at(asset);
    if (state.has_value()) {
      continue;
    }
    if (state.error() == AssetUnavailableReason::kFailed) {
      return base::unexpected(
          OnDeviceModelEligibilityReason::kModelAdaptationNotAvailable);
    }
    has_unavailable_asset = true;
  }
  const proto::UseCaseConfig& use_case =
      manifest_.GetDeviceCategoryConfig().use_cases().at(use_case_name);

  if (has_unavailable_asset) {
    if (!usage_tracker_->WasUseCaseRecentlyUsed(use_case_name)) {
      return base::unexpected(
          OnDeviceModelEligibilityReason::kNoOnDeviceFeatureUsed);
    }
    return base::unexpected(
        OnDeviceModelEligibilityReason::kModelToBeInstalled);
  }
  const std::string& solution_id = use_case.solution_recipe_id();
  switch (solutions_.at(solution_id).status_) {
    case SolutionState::kNotLoaded:
    case SolutionState::kLoading:
      return base::unexpected(
          OnDeviceModelEligibilityReason::kModelToBeInstalled);
    case SolutionState::kFailed:
      return base::unexpected(
          OnDeviceModelEligibilityReason::kModelAdaptationNotAvailable);
    case SolutionState::kReady:
      break;
  }

  // Everything is available, create the solution.
  return std::make_unique<Solution>(weak_ptr_factory_.GetWeakPtr(),
                                    use_case_name, solution_id);
}

mojo::Remote<on_device_model::mojom::OnDeviceModel>&
ManifestSolutionFactory::GetOrLoadModel(const std::string& model_id) {
  if (auto it = base_models_.find(model_id); it != base_models_.end()) {
    auto& state = it->second;
    if (!state.remote_) {
      LoadBaseModel(model_id, state);
    }
    return state.remote_;
  }
  if (auto it = adaptations_.find(model_id); it != adaptations_.end()) {
    auto& state = it->second;
    if (!state.remote_) {
      LoadAdaptation(model_id, state);
    }
    return state.remote_;
  }
  NOTREACHED();
}

mojo::Remote<on_device_model::mojom::TextSafetyModel>&
ManifestSolutionFactory::GetOrLoadTextSafetyModel(const std::string& model_id) {
  TRACE_EVENT("optimization_guide",
              "ManifestSolutionFactory::GetOrLoadTextSafetyModel", "model_id",
              model_id);
  auto& state = safety_models_.at(model_id);
  if (!state.remote_) {
    const auto& recipe = manifest_.GetRecipes().safety_models().at(model_id);
    on_device_model::TextSafetyLoaderParams params;
    if (recipe.has_weights_file()) {
      params.ts_paths.emplace();
      // We should not get here unless the asset is available.
      params.ts_paths->data = *ResolveFile(recipe.weights_file());
    }
    // TODO(holte): Add language detection model file to manifest.
    // if (recipe.has_language_detection_model_file()) {
    //   params.language_paths.emplace();
    //   // We should not get here unless the asset is available.
    //   params.language_paths->model =
    //   *ResolveFile(recipe.language_detection_model_file());
    // }
    service_client_->AddPendingUsage();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&on_device_model::LoadTextSafetyParams,
                       std::move(params)),
        base::BindOnce(
            [](base::WeakPtr<ManifestSolutionFactory> factory,
               const std::string& model_id,
               mojo::PendingReceiver<on_device_model::mojom::TextSafetyModel>
                   model,
               on_device_model::mojom::TextSafetyModelParamsPtr params) {
              TRACE_EVENT(
                  "optimization_guide",
                  "ManifestSolutionFactory::OnTextSafetyModelParamsLoaded",
                  "model_id", model_id);
              if (!factory) {
                CloseAssetsInBackground(std::move(params));
                return;
              }
              factory->service_client_->Get()->LoadTextSafetyModel(
                  std::move(params), std::move(model));
              factory->service_client_->RemovePendingUsage();
            },
            weak_ptr_factory_.GetWeakPtr(), model_id,
            state.remote_.BindNewPipeAndPassReceiver()));
    // Disconnects should only happen on a service crash, and we track those
    // elsewhere.
    state.remote_.reset_on_disconnect();
    state.remote_.reset_on_idle_timeout(
        features::GetOnDeviceModelIdleTimeout());
  }
  return state.remote_;
}

void ManifestSolutionFactory::LoadBaseModel(const std::string& model_id,
                                            BaseModelState& state) {
  TRACE_EVENT("optimization_guide", "ManifestSolutionFactory::LoadBaseModel",
              "model_id", model_id);
  const auto& recipe = manifest_.GetRecipes().base_models().at(model_id);
  on_device_model::ModelAssetPaths paths;
  // We should not get here unless the asset is available.
  paths.weights = *ResolveFile(recipe.weights_file());

  service_client_->AddPendingUsage();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&on_device_model::LoadModelAssets, std::move(paths)),
      base::BindOnce(
          [](base::WeakPtr<ManifestSolutionFactory> factory,
             const std::string& model_id,
             mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
             on_device_model::ModelAssets assets) {
            TRACE_EVENT("optimization_guide",
                        "ManifestSolutionFactory::OnBaseModelAssetsLoaded",
                        "model_id", model_id);
            if (!factory) {
              CloseAssetsInBackground(std::move(assets));
              return;
            }
            const auto& recipe =
                factory->manifest_.GetRecipes().base_models().at(model_id);
            auto params = on_device_model::mojom::LoadModelParams::New();
            params->max_tokens = recipe.max_tokens();
            params->performance_hint =
                ConvertPerformanceHint(recipe.performance_hint());
            for (int32_t rank : recipe.supported_adaptation_ranks()) {
              params->adaptation_ranks.push_back(rank);
            }
            params->backend_type = ConvertBackendType(recipe.backend_type());
            params->assets = std::move(assets);
            factory->service_client_->Get()->LoadModel(
                std::move(params), std::move(model), base::DoNothing());
            factory->service_client_->RemovePendingUsage();
          },
          weak_ptr_factory_.GetWeakPtr(), model_id,
          state.remote_.BindNewPipeAndPassReceiver()));
  // Disconnects should only happen on a service crash, and we track those
  // elsewhere.
  state.remote_.reset_on_disconnect();
  state.remote_.reset_on_idle_timeout(features::GetOnDeviceModelIdleTimeout());
}

void ManifestSolutionFactory::LoadAdaptation(const std::string& model_id,
                                             AdaptationState& state) {
  TRACE_EVENT("optimization_guide", "ManifestSolutionFactory::LoadAdaptation",
              "model_id", model_id);
  const auto& recipe = manifest_.GetRecipes().adaptations().at(model_id);
  on_device_model::AdaptationAssetPaths paths;
  // We should not get here unless the asset is available.
  paths.weights = *ResolveFile(recipe.weights_file());

  // TODO(holte): Warm up base model remote and add pending usage for
  // adaptations.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&on_device_model::LoadAdaptationAssets, std::move(paths)),
      base::BindOnce(
          [](base::WeakPtr<ManifestSolutionFactory> factory,
             const std::string& model_id,
             mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
             on_device_model::AdaptationAssets assets) {
            TRACE_EVENT("optimization_guide",
                        "ManifestSolutionFactory::OnAdaptationAssetsLoaded",
                        "model_id", model_id);
            if (!factory) {
              CloseAssetsInBackground(std::move(assets));
              return;
            }
            const auto& recipe =
                factory->manifest_.GetRecipes().adaptations().at(model_id);
            auto params = on_device_model::mojom::LoadAdaptationParams::New();
            params->assets = std::move(assets);
            factory->GetOrLoadModel(recipe.base_model_recipe_id())
                ->LoadAdaptation(std::move(params), std::move(model),
                                 base::DoNothing());
            // TODO(holte): Remove pending usage for adaptations.
          },
          weak_ptr_factory_.GetWeakPtr(), model_id,
          state.remote_.BindNewPipeAndPassReceiver()));
  state.remote_.reset_on_disconnect();
  state.remote_.reset_on_idle_timeout(features::GetOnDeviceModelIdleTimeout());
}

}  // namespace optimization_guide
