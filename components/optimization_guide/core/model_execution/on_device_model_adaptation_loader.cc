// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
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

}  // namespace

OnDeviceModelAdaptationLoader::OnDeviceModelAdaptationLoader(
    ModelBasedCapabilityKey feature,
    OptimizationGuideModelProvider* model_provider,
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager,
    OnLoadFn on_load_fn)
    : feature_(feature),
      on_load_fn_(on_load_fn),
      model_provider_(model_provider) {
  CHECK(features::internal::IsOnDeviceModelAdaptationEnabled(feature_));
  if (const auto adaptations_override = GetOnDeviceModelAdaptationOverride(
          ToModelExecutionFeatureProto(feature_))) {
    // Do not register with model provider or component state manager, when
    // override is provided.
    model_provider_ = nullptr;
    on_load_fn_.Run(nullptr);
    return;
  }
  component_state_manager_observation_.Observe(
      on_device_component_state_manager.get());
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
  if (!base_model_spec_) {
    RecordAdaptationModelAvailability(
        feature_, OnDeviceModelAdaptationAvailability::kBaseModelSpecInvalid);
    return;
  }

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/optimization_guide.proto.OnDeviceBaseModelMetadata");
  proto::OnDeviceBaseModelMetadata model_metadata;
  model_metadata.set_base_model_version(base_model_spec_->model_version);
  model_metadata.set_base_model_name(base_model_spec_->model_name);
  model_metadata.SerializeToString(any_metadata.mutable_value());

  model_provider_->AddObserverForOptimizationTargetModel(
      features::internal::GetOptimizationTargetForModelAdaptation(feature_),
      any_metadata, this);
  registered_with_model_provider_ = true;
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
  RecordAdaptationModelAvailability(
      feature_, OnDeviceModelAdaptationAvailability::kAvailable);
  on_load_fn_.Run(std::move(result.value()));
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
  if (!base_model_spec_) {
    return base::unexpected(
        OnDeviceModelAdaptationAvailability::kBaseModelUnavailable);
  }
  if (supported_model_spec->base_model_name() != base_model_spec_->model_name ||
      supported_model_spec->base_model_version() !=
          base_model_spec_->model_version) {
    return base::unexpected(
        OnDeviceModelAdaptationAvailability::kAdaptationModelIncompatible);
  }

  auto model_file = model_info->GetAdditionalFileWithBaseName(
      kOnDeviceModelAdaptationModelFile);
  if (!model_file) {
    return base::unexpected(
        OnDeviceModelAdaptationAvailability::kAdaptationModelInvalid);
  }
  auto weights_file = model_info->GetAdditionalFileWithBaseName(
      kOnDeviceModelAdaptationWeightsFile);
  auto adaptations_assets =
      std::make_unique<on_device_model::AdaptationAssetPaths>();
  adaptations_assets->model = *model_file;
  if (weights_file) {
    adaptations_assets->weights = *weights_file;
  }
  return base::ok(std::move(adaptations_assets));
}

}  // namespace optimization_guide
