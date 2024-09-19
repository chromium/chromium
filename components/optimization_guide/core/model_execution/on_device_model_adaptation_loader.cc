// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
#include "components/prefs/pref_service.h"
#include "services/on_device_model/public/cpp/model_assets.h"

namespace optimization_guide {

namespace {

void RecordAdaptationModelAvailability(
    ModelBasedCapabilityKey feature,
    OnDeviceModelAdaptationAvailability availability) {
  base::UmaHistogramEnumeration(
      base::StrCat({"OptimizationGuide.ModelExecution."
                    "OnDeviceAdaptationModelAvailability.",
                    GetStringNameForModelExecutionFeature(feature)}),
      availability);
}

base::expected<std::unique_ptr<OnDeviceModelAdaptationMetadata>,
               OnDeviceModelAdaptationAvailability>
CreateAdaptatonMetadataFromModelExecutionConfig(
    ModelBasedCapabilityKey feature,
    std::unique_ptr<on_device_model::AdaptationAssetPaths> asset_paths,
    int64_t version,
    std::unique_ptr<proto::OnDeviceModelExecutionConfig> execution_config) {
  if (!execution_config) {
    return base::unexpected(OnDeviceModelAdaptationAvailability::
                                kAdaptationModelExecutionConfigInvalid);
  }
  if (execution_config->feature_configs_size() != 1) {
    return base::unexpected(OnDeviceModelAdaptationAvailability::
                                kAdaptationModelExecutionConfigInvalid);
  }
  auto& config = *execution_config->mutable_feature_configs(0);
  if (config.feature() != ToModelExecutionFeatureProto(feature)) {
    return base::unexpected(OnDeviceModelAdaptationAvailability::
                                kAdaptationModelExecutionConfigInvalid);
  }
  return base::ok(OnDeviceModelAdaptationMetadata::New(
      asset_paths.get(), version,
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config))));
}

std::unique_ptr<OnDeviceModelAdaptationMetadata>
OnDeviceModelAdaptationMetadataCreated(
    ModelBasedCapabilityKey feature,
    base::expected<std::unique_ptr<OnDeviceModelAdaptationMetadata>,
                   OnDeviceModelAdaptationAvailability> metadata) {
  if (!metadata.has_value()) {
    RecordAdaptationModelAvailability(feature, metadata.error());
    return nullptr;
  }
  RecordAdaptationModelAvailability(
      feature, OnDeviceModelAdaptationAvailability::kAvailable);
  return std::move(metadata.value());
}

}  // namespace

// static
std::unique_ptr<OnDeviceModelAdaptationMetadata>
OnDeviceModelAdaptationMetadata::New(
    on_device_model::AdaptationAssetPaths* asset_paths,
    int64_t version,
    scoped_refptr<OnDeviceModelFeatureAdapter> adapter) {
  return base::WrapUnique(new OnDeviceModelAdaptationMetadata(
      asset_paths, version, std::move(adapter)));
}

OnDeviceModelAdaptationMetadata::OnDeviceModelAdaptationMetadata(
    on_device_model::AdaptationAssetPaths* asset_paths,
    int64_t version,
    scoped_refptr<OnDeviceModelFeatureAdapter> adapter)
    : asset_paths_(base::OptionalFromPtr(asset_paths)),
      version_(version),
      adapter_(std::move(adapter)) {}

OnDeviceModelAdaptationMetadata::OnDeviceModelAdaptationMetadata(
    const OnDeviceModelAdaptationMetadata&) = default;
OnDeviceModelAdaptationMetadata::~OnDeviceModelAdaptationMetadata() = default;

OnDeviceModelAdaptationLoader::OnDeviceModelAdaptationLoader(
    ModelBasedCapabilityKey feature,
    OptimizationGuideModelProvider* model_provider,
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager,
    PrefService* local_state,
    OnLoadFn on_load_fn)
    : feature_(feature),
      on_load_fn_(on_load_fn),
      on_device_component_state_manager_(on_device_component_state_manager),
      local_state_(local_state),
      model_provider_(model_provider),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {
  CHECK(features::internal::IsOnDeviceModelAdaptationEnabled(feature_));
  if (!on_device_component_state_manager) {
    return;
  }

  component_state_manager_observation_.Observe(
      on_device_component_state_manager.get());
  if (auto* state = on_device_component_state_manager->GetState()) {
    StateChanged(state);
  }
}

OnDeviceModelAdaptationLoader::~OnDeviceModelAdaptationLoader() {
  if (registered_with_model_provider_) {
    model_provider_->RemoveObserverForOptimizationTargetModel(
        features::internal::GetOptimizationTargetForModelAdaptation(feature_),
        this);
  }
}

void OnDeviceModelAdaptationLoader::StateChanged(
    const OnDeviceModelComponentState* state) {
  MaybeRegisterModelDownload(
      state, WasOnDeviceEligibleFeatureRecentlyUsed(feature_, *local_state_));
}

void OnDeviceModelAdaptationLoader::MaybeRegisterModelDownload(
    const OnDeviceModelComponentState* state,
    bool was_feature_recently_used) {
  CHECK(model_provider_);
  if (registered_with_model_provider_) {
    model_provider_->RemoveObserverForOptimizationTargetModel(
        features::internal::GetOptimizationTargetForModelAdaptation(feature_),
        this);
    registered_with_model_provider_ = false;
  }
  base_model_spec_ = std::nullopt;
  on_load_fn_.Run(nullptr);
  if (!state) {
    RecordAdaptationModelAvailability(
        feature_, OnDeviceModelAdaptationAvailability::kBaseModelUnavailable);
    return;
  }
  base_model_spec_ = state->GetBaseModelSpec();
  if (!switches::GetOnDeviceModelExecutionOverride()) {
    if (!base_model_spec_) {
      RecordAdaptationModelAvailability(
          feature_, OnDeviceModelAdaptationAvailability::kBaseModelSpecInvalid);
      return;
    }
    if (!was_feature_recently_used) {
      RecordAdaptationModelAvailability(
          feature_,
          OnDeviceModelAdaptationAvailability::kFeatureNotRecentlyUsed);
      return;
    }
  }

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1.OnDeviceBaseModelMetadata");
  proto::OnDeviceBaseModelMetadata model_metadata;
  if (base_model_spec_) {
    model_metadata.set_base_model_version(base_model_spec_->model_version);
    model_metadata.set_base_model_name(base_model_spec_->model_name);
  }
  model_metadata.SerializeToString(any_metadata.mutable_value());

  model_provider_->AddObserverForOptimizationTargetModel(
      features::internal::GetOptimizationTargetForModelAdaptation(feature_),
      any_metadata, this);
  registered_with_model_provider_ = true;
}

void OnDeviceModelAdaptationLoader::OnDeviceEligibleFeatureFirstUsed(
    ModelBasedCapabilityKey feature) {
  if (feature != feature_) {
    return;
  }
  if (!on_device_component_state_manager_) {
    return;
  }
  MaybeRegisterModelDownload(
      on_device_component_state_manager_->GetState(),
      WasOnDeviceEligibleFeatureRecentlyUsed(feature_, *local_state_));
}

void OnDeviceModelAdaptationLoader::OnModelUpdated(
    proto::OptimizationTarget optimization_target,
    base::optional_ref<const ModelInfo> model_info) {
  CHECK_EQ(
      optimization_target,
      features::internal::GetOptimizationTargetForModelAdaptation(feature_));
  on_load_fn_.Run(nullptr);
  auto result = ProcessModelUpdate(model_info);
  if (!result.has_value()) {
    RecordAdaptationModelAvailability(feature_, result.error());
    return;
  }
  auto execution_config_file = model_info->GetAdditionalFileWithBaseName(
      kOnDeviceModelExecutionConfigFile);
  if (!execution_config_file) {
    RecordAdaptationModelAvailability(
        feature_, OnDeviceModelAdaptationAvailability::
                      kAdaptationModelExecutionConfigInvalid);

    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadOnDeviceModelExecutionConfig, *execution_config_file),
      base::BindOnce(&CreateAdaptatonMetadataFromModelExecutionConfig, feature_,
                     std::move(result.value()), model_info->GetVersion())
          .Then(
              base::BindOnce(&OnDeviceModelAdaptationMetadataCreated, feature_))
          .Then(on_load_fn_));
}

base::expected<std::unique_ptr<on_device_model::AdaptationAssetPaths>,
               OnDeviceModelAdaptationAvailability>
OnDeviceModelAdaptationLoader::ProcessModelUpdate(
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  if (!model_info.has_value()) {
    return base::unexpected(
        OnDeviceModelAdaptationAvailability::kAdaptationModelUnavailable);
  }
  const std::optional<proto::Any>& metadata = model_info->GetModelMetadata();
  if (!metadata.has_value()) {
    return base::unexpected(
        OnDeviceModelAdaptationAvailability::kAdaptationModelInvalid);
  }
  auto supported_model_spec =
      ParsedAnyMetadata<proto::OnDeviceBaseModelMetadata>(metadata.value());
  if (!supported_model_spec) {
    return base::unexpected(
        OnDeviceModelAdaptationAvailability::kAdaptationModelInvalid);
  }
  // Check for incompatibility when base model override is not specified
  if (!switches::GetOnDeviceModelExecutionOverride()) {
    if (!base_model_spec_) {
      return base::unexpected(
          OnDeviceModelAdaptationAvailability::kBaseModelUnavailable);
    }
    if (supported_model_spec->base_model_name() !=
            base_model_spec_->model_name ||
        supported_model_spec->base_model_version() !=
            base_model_spec_->model_version) {
      return base::unexpected(
          OnDeviceModelAdaptationAvailability::kAdaptationModelIncompatible);
    }
  }

  auto weights_file = model_info->GetAdditionalFileWithBaseName(
      kOnDeviceModelAdaptationWeightsFile);
  if (!weights_file) {
    // Return that the weights file was not provided.
    return base::ok(nullptr);
  }
  auto adaptations_assets =
      std::make_unique<on_device_model::AdaptationAssetPaths>();
  adaptations_assets->weights = *weights_file;
  return base::ok(std::move(adaptations_assets));
}

}  // namespace optimization_guide
