// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/model_provider_registry.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/optimization_guide/core/delivery/model_util.h"

namespace optimization_guide {

namespace {

// Returns whether the model metadata proto is on the server allowlist.
bool IsModelMetadataTypeOnServerAllowlist(const proto::Any& model_metadata) {
  static const auto* const kAllowList = new base::flat_set<std::string>{
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1."
      "OnDeviceTailSuggestModelMetadata",
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1."
      "PageTopicsModelMetadata",
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1."
      "SegmentationModelMetadata",
      "type.googleapis.com/"
      "google.privacy.webpermissionpredictions.v1."
      "WebPermissionPredictionsModelMetadata",
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1."
      "ClientSidePhishingModelMetadata",
      "type.googleapis.com/"
      "lens.prime.csc.VisualSearchModelMetadata",
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1."
      "OnDeviceBaseModelMetadata",
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1."
      "AutofillFieldClassificationModelMetadata",
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1."
      "AutocompleteScoringModelMetadata",
      "type.googleapis.com/"
      "google.privacy.webpermissionpredictions.v1."
      "WebPermissionPredictionsClientInfo",
      "type.googleapis.com/"
      "google.privacy.webpermissionpredictions.aiv3.v1."
      "PermissionsAiv3ModelMetadata"};

  return base::Contains(*kAllowList, model_metadata.type_url());
}

}  // namespace

ModelProviderRegistry::ModelProviderRegistry(OptimizationGuideLogger* logger)
    : optimization_guide_logger_(logger) {}
ModelProviderRegistry::~ModelProviderRegistry() = default;

ModelProviderRegistry::ModelRegistrationInfo::ModelRegistrationInfo(
    std::optional<proto::Any> metadata)
    : metadata(metadata) {}
ModelProviderRegistry::ModelRegistrationInfo::~ModelRegistrationInfo() =
    default;

void ModelProviderRegistry::AddObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    const std::optional<proto::Any>& model_metadata,
    scoped_refptr<base::SequencedTaskRunner> model_task_runner,
    OptimizationTargetModelObserver* observer) {
  CHECK(!model_metadata ||
        IsModelMetadataTypeOnServerAllowlist(*model_metadata));

  auto it = model_registration_info_map_.emplace(
      std::piecewise_construct, std::forward_as_tuple(optimization_target),
      std::forward_as_tuple(model_metadata));
  it.first->second.model_observers.AddObserver(observer);
  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
        optimization_guide_logger_.get())
        << "Observer added for OptimizationTarget: " << optimization_target;
  }

  // Notify observer of existing model file path.
  auto model_it = optimization_target_model_info_map_.find(optimization_target);
  if (model_it != optimization_target_model_info_map_.end()) {
    observer->OnModelUpdated(optimization_target, *model_it->second);
    if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
          optimization_guide_logger_.get())
          << "OnModelFileUpdated for OptimizationTarget: "
          << optimization_target << "\nFile path: "
          << model_it->second->GetModelFilePath().AsUTF8Unsafe()
          << "\nHas metadata: " << (model_metadata ? "True" : "False");
    }
    RecordLifecycleState(optimization_target,
                         ModelDeliveryEvent::kModelDeliveredAtRegistration);
  }
}

void ModelProviderRegistry::RemoveObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    OptimizationTargetModelObserver* observer) {
  auto registration_info =
      model_registration_info_map_.find(optimization_target);
  CHECK(registration_info != model_registration_info_map_.end());

  auto& observers = registration_info->second.model_observers;
  DCHECK(observers.HasObserver(observer));
  observers.RemoveObserver(observer);
  if (observers.empty()) {
    model_registration_info_map_.erase(registration_info);
  }
}

base::flat_set<proto::OptimizationTarget>
ModelProviderRegistry::GetRegisteredOptimizationTargets() const {
  base::flat_set<proto::OptimizationTarget> optimization_targets;
  for (const auto& registration_info : model_registration_info_map_) {
    optimization_targets.insert(registration_info.first);
  }
  return optimization_targets;
}

const ModelInfo* ModelProviderRegistry::GetModel(
    proto::OptimizationTarget target) const {
  auto it = optimization_target_model_info_map_.find(target);
  if (it != optimization_target_model_info_map_.end()) {
    return it->second.get();
  }
  return nullptr;
}

base::optional_ref<const proto::Any>
ModelProviderRegistry::GetRegistrationMetadata(
    proto::OptimizationTarget target) const {
  auto it = model_registration_info_map_.find(target);
  if (it == model_registration_info_map_.end()) {
    return std::nullopt;
  }
  return it->second.metadata;
}

std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
ModelProviderRegistry::GetDownloadedModelsInfoForWebUI() const {
  std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
      downloaded_models_info;
  downloaded_models_info.reserve(optimization_target_model_info_map_.size());
  for (const auto& it : optimization_target_model_info_map_) {
    const std::string& optimization_target_name =
        optimization_guide::proto::OptimizationTarget_Name(it.first);
    const optimization_guide::ModelInfo* const model_info = it.second.get();
    auto downloaded_model_info_ptr =
        optimization_guide_internals::mojom::DownloadedModelInfo::New(
            optimization_target_name, model_info->GetVersion(),
            model_info->GetModelFilePath().AsUTF8Unsafe());
    downloaded_models_info.push_back(std::move(downloaded_model_info_ptr));
  }
  return downloaded_models_info;
}

void ModelProviderRegistry::UpdateModel(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<ModelInfo> model_info) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ModelProviderRegistry::NotifyObserversOfNewModel,
                     weak_ptr_factory_.GetWeakPtr(), optimization_target,
                     *model_info));
  optimization_target_model_info_map_.insert_or_assign(optimization_target,
                                                       std::move(model_info));
}

void ModelProviderRegistry::RemoveModel(
    proto::OptimizationTarget optimization_target) {
  optimization_target_model_info_map_.erase(optimization_target);
  NotifyObserversOfNewModel(optimization_target, std::nullopt);
}

void ModelProviderRegistry::UpdateModelImmediatelyForTesting(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<ModelInfo> model_info) {
  auto it = optimization_target_model_info_map_
                .insert_or_assign(optimization_target, std::move(model_info))
                .first;
  NotifyObserversOfNewModel(optimization_target, *it->second);
}

// static
void ModelProviderRegistry::RecordLifecycleState(
    proto::OptimizationTarget optimization_target,
    ModelDeliveryEvent event) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PredictionManager.ModelDeliveryEvents." +
          GetStringNameForOptimizationTarget(optimization_target),
      event);
}

void ModelProviderRegistry::NotifyObserversOfNewModel(
    proto::OptimizationTarget optimization_target,
    base::optional_ref<const ModelInfo> model_info) {
  auto registration_info_it =
      model_registration_info_map_.find(optimization_target);
  if (registration_info_it == model_registration_info_map_.end()) {
    return;
  }
  RecordLifecycleState(optimization_target,
                       ModelDeliveryEvent::kModelDelivered);
  for (auto& observer : registration_info_it->second.model_observers) {
    observer.OnModelUpdated(optimization_target, model_info);
  }
  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    if (model_info.has_value()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
          optimization_guide_logger_.get())
          << "OnModelFileUpdated for target: " << optimization_target
          << "\nFile path: " << model_info->GetModelFilePath().AsUTF8Unsafe()
          << "\nHas metadata: "
          << (model_info->GetModelMetadata() ? "True" : "False");
    } else {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
          optimization_guide_logger_.get())
          << "OnModelFileUpdated for target: " << optimization_target
          << " for model removed";
    }
  }
}

}  // namespace optimization_guide
