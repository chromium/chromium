// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_util.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "components/prefs/pref_service.h"
#include "services/on_device_model/public/cpp/model_assets.h"

namespace optimization_guide {

namespace {

void RecordAdaptationModelAvailability(
    mojom::OnDeviceFeature feature,
    OnDeviceModelAdaptationAvailability availability) {
  base::UmaHistogramEnumeration(
      base::StrCat({"OptimizationGuide.ModelExecution."
                    "OnDeviceAdaptationModelAvailability.",
                    GetVariantName(feature)}),
      availability);
}

base::expected<OnDeviceModelAdaptationMetadata,
               OnDeviceModelAdaptationAvailability>
CreateAdaptationMetadataFromModelExecutionConfig(
    mojom::OnDeviceFeature feature,
    std::unique_ptr<on_device_model::AdaptationAssetPaths> asset_paths,
    int64_t version,
    std::unique_ptr<proto::OnDeviceModelExecutionConfig> execution_config) {
  TRACE_EVENT("optimization_guide",
              "CreateAdaptationMetadataFromModelExecutionConfig");
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
  return OnDeviceModelAdaptationMetadata(
      asset_paths.get(), version,
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config)));
}

MaybeAdaptationMetadata OnDeviceModelAdaptationMetadataCreated(
    mojom::OnDeviceFeature feature,
    base::expected<OnDeviceModelAdaptationMetadata,
                   OnDeviceModelAdaptationAvailability> metadata) {
  if (!metadata.has_value()) {
    RecordAdaptationModelAvailability(feature, metadata.error());
    return base::unexpected(AdaptationUnavailability::kNotSupported);
  }
  RecordAdaptationModelAvailability(
      feature, OnDeviceModelAdaptationAvailability::kAvailable);
  return std::move(metadata.value());
}

bool ArePerformanceHintsCompatible(
    const proto::OnDeviceBaseModelMetadata& adaptation_metadata,
    const OnDeviceBaseModelSpec& base_spec) {
  // If the adaptation model has no specific hints, it supports all.
  if (adaptation_metadata.supported_performance_hints().empty()) {
    return true;
  }
  // Check if the adaptation model supports any of the base model's hints.
  return base::Contains(adaptation_metadata.supported_performance_hints(),
                        base_spec.selected_performance_hint);
}

std::optional<OnDeviceModelAdaptationAvailability>
DetectBaseModelIncompatibility(const optimization_guide::ModelInfo& model_info,
                               const OnDeviceBaseModelSpec& registered_spec) {
  const std::optional<proto::Any>& metadata = model_info.GetModelMetadata();
  if (!metadata.has_value()) {
    return OnDeviceModelAdaptationAvailability::kAdaptationModelInvalid;
  }
  auto supported_model_spec =
      ParsedAnyMetadata<proto::OnDeviceBaseModelMetadata>(metadata.value());
  if (!supported_model_spec) {
    return OnDeviceModelAdaptationAvailability::kAdaptationModelInvalid;
  }
  // Check for incompatibility when base model override is not specified
  if (!switches::GetOnDeviceModelExecutionOverride()) {
    if (supported_model_spec->base_model_name() != registered_spec.model_name ||
        supported_model_spec->base_model_version() !=
            registered_spec.model_version) {
      return OnDeviceModelAdaptationAvailability::kAdaptationModelIncompatible;
    }
    if (!ArePerformanceHintsCompatible(*supported_model_spec,
                                       registered_spec)) {
      return OnDeviceModelAdaptationAvailability::
          kAdaptationModelHintsIncompatible;
    }
  }
  return std::nullopt;
}

std::unique_ptr<on_device_model::AdaptationAssetPaths> MaybeGetAdaptationPaths(
    const optimization_guide::ModelInfo& model_info) {
  auto weights_file = model_info.GetAdditionalFileWithBaseName(
      kOnDeviceModelAdaptationWeightsFile);
  if (!weights_file) {
    return nullptr;
  }
  auto adaptation_assets =
      std::make_unique<on_device_model::AdaptationAssetPaths>();
  adaptation_assets->weights = *weights_file;
  return adaptation_assets;
}

}  // namespace

OnDeviceModelAdaptationMetadata::OnDeviceModelAdaptationMetadata(
    on_device_model::AdaptationAssetPaths* asset_paths,
    int64_t version,
    scoped_refptr<OnDeviceModelFeatureAdapter> adapter)
    : asset_paths_(base::OptionalFromPtr(asset_paths)),
      version_(version),
      adapter_(std::move(adapter)) {}

OnDeviceModelAdaptationMetadata::OnDeviceModelAdaptationMetadata(
    const OnDeviceModelAdaptationMetadata&) = default;
OnDeviceModelAdaptationMetadata::OnDeviceModelAdaptationMetadata(
    OnDeviceModelAdaptationMetadata&&) = default;
OnDeviceModelAdaptationMetadata::~OnDeviceModelAdaptationMetadata() = default;

OnDeviceModelAdaptationMetadata& OnDeviceModelAdaptationMetadata::operator=(
    OnDeviceModelAdaptationMetadata&&) = default;

bool OnDeviceModelAdaptationMetadata::operator==(
    const OnDeviceModelAdaptationMetadata& other) const {
  return version_ == other.version_ && asset_paths_ == other.asset_paths_;
}

const on_device_model::AdaptationAssetPaths*
OnDeviceModelAdaptationMetadata::asset_paths() const {
  return base::OptionalToPtr(asset_paths_);
}

AdaptationMetadataMap::AdaptationMetadataMap() = default;
AdaptationMetadataMap::~AdaptationMetadataMap() = default;
MaybeAdaptationMetadata& AdaptationMetadataMap::Get(
    mojom::OnDeviceFeature feature) {
  auto it =
      metadata_
          .emplace(feature,
                   base::unexpected(AdaptationUnavailability::kUpdatePending))
          .first;
  return it->second;
}

bool AdaptationMetadataMap::MaybeUpdate(mojom::OnDeviceFeature feature,
                                        MaybeAdaptationMetadata metadata) {
  MaybeAdaptationMetadata& current_metadata = Get(feature);
  if (current_metadata == metadata) {
    // Duplicate update (can be caused by multiple profiles providing updates).
    // Keep the existing copy.
    return false;
  }
  current_metadata = std::move(metadata);
  return true;
}

OnDeviceModelAdaptationLoader::OnDeviceModelAdaptationLoader(
    mojom::OnDeviceFeature feature,
    OptimizationGuideModelProvider& model_provider,
    OnLoadFn on_load_fn)
    : feature_(feature),
      target_(GetOptimizationTargetForFeature(feature_)),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      model_provider_observation_(&model_provider,
                                  background_task_runner_,
                                  this),
      on_load_fn_(on_load_fn) {}

OnDeviceModelAdaptationLoader::~OnDeviceModelAdaptationLoader() {
  Unregister();
}

void OnDeviceModelAdaptationLoader::Unregister() {
  if (registered_spec_) {
    model_provider_observation_.Reset();
    registered_spec_.reset();
  }
}

void OnDeviceModelAdaptationLoader::MaybeRegisterModelDownload(
    base::optional_ref<const OnDeviceBaseModelSpec> new_spec,
    bool was_feature_recently_used) {
  TRACE_EVENT("optimization_guide", "MaybeRegisterModelDownload", "feature",
              base::ToString(feature_));
  if (new_spec && *new_spec == registered_spec_) {
    return;
  }

  // The spec has changed, so we need to unregister the old observer.
  Unregister();
  on_load_fn_.Run(base::unexpected(AdaptationUnavailability::kUpdatePending));

  if (!new_spec) {
    RecordAdaptationModelAvailability(
        feature_, OnDeviceModelAdaptationAvailability::kBaseModelUnavailable);
    return;
  }

  if (!switches::GetOnDeviceModelExecutionOverride() &&
      !was_feature_recently_used) {
    RecordAdaptationModelAvailability(
        feature_, OnDeviceModelAdaptationAvailability::kFeatureNotRecentlyUsed);
    return;
  }

  registered_spec_ = *new_spec;
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1.OnDeviceBaseModelMetadata");
  {
    proto::OnDeviceBaseModelMetadata model_metadata;
    model_metadata.set_base_model_version(registered_spec_->model_version);
    model_metadata.set_base_model_name(registered_spec_->model_name);
    model_metadata.add_supported_performance_hints(
        registered_spec_->selected_performance_hint);
    model_metadata.SerializeToString(any_metadata.mutable_value());
  }

  model_provider_observation_.Observe(target_, any_metadata);
}

void OnDeviceModelAdaptationLoader::OnModelUpdated(
    proto::OptimizationTarget optimization_target,
    base::optional_ref<const ModelInfo> model_info) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelAdaptationLoader::OnModelUpdated", "feature",
              base::ToString(feature_));
  CHECK_EQ(optimization_target, target_);
  CHECK(registered_spec_.has_value());
  if (!model_info.has_value()) {
    // The server has indicated no adaptation is available.
    RecordAdaptationModelAvailability(
        feature_,
        OnDeviceModelAdaptationAvailability::kAdaptationModelUnavailable);
    on_load_fn_.Run(base::unexpected(AdaptationUnavailability::kNotSupported));
    return;
  }
  // The current adaptation's files might get cleaned up, so stop using it.
  on_load_fn_.Run(base::unexpected(AdaptationUnavailability::kUpdatePending));
  auto error = DetectBaseModelIncompatibility(*model_info, *registered_spec_);
  if (error) {
    RecordAdaptationModelAvailability(feature_, *error);
    // Likely a stale asset that was on disk, and we haven't fetched yet.
    // Don't notify the controller yet.
    return;
  }
  auto execution_config_file = model_info->GetAdditionalFileWithBaseName(
      kOnDeviceModelExecutionConfigFile);
  if (!execution_config_file) {
    RecordAdaptationModelAvailability(
        feature_, OnDeviceModelAdaptationAvailability::
                      kAdaptationModelExecutionConfigInvalid);
    on_load_fn_.Run(base::unexpected(AdaptationUnavailability::kNotSupported));
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadOnDeviceModelExecutionConfig, *execution_config_file),
      base::BindOnce(&CreateAdaptationMetadataFromModelExecutionConfig,
                     feature_, MaybeGetAdaptationPaths(*model_info),
                     model_info->GetVersion())
          .Then(
              base::BindOnce(&OnDeviceModelAdaptationMetadataCreated, feature_))
          .Then(on_load_fn_));
}

AdaptationLoaderMap::AdaptationLoaderMap(
    OptimizationGuideModelProvider& provider,
    OnLoadFn on_load_fn) {
  for (mojom::OnDeviceFeature feature : OnDeviceFeatureSet::All()) {
    loaders_[feature] = std::make_unique<OnDeviceModelAdaptationLoader>(
        feature, provider, base::BindRepeating(on_load_fn, feature));
  }
}
AdaptationLoaderMap::~AdaptationLoaderMap() = default;

void AdaptationLoaderMap::MaybeRegisterModelDownload(
    mojom::OnDeviceFeature feature,
    base::optional_ref<const OnDeviceBaseModelSpec> spec,
    bool was_feature_recently_used) {
  auto it = loaders_.find(feature);
  if (it != loaders_.end()) {
    it->second->MaybeRegisterModelDownload(std::move(spec),
                                           was_feature_recently_used);
  }
}

}  // namespace optimization_guide
