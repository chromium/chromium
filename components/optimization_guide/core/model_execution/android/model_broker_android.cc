// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/android/model_broker_android.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "services/on_device_model/android/backend_model_impl_android.h"
#include "services/on_device_model/android/model_downloader_android.h"
#include "services/on_device_model/on_device_model_mojom_impl.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace optimization_guide {

namespace {

using BaseModelSpec = on_device_model::ModelDownloaderAndroid::BaseModelSpec;
using DownloadFailureReason =
    on_device_model::ModelDownloaderAndroid::DownloadFailureReason;

class SolutionImpl : public ModelBrokerImpl::Solution {
 public:
  SolutionImpl(base::WeakPtr<ModelBrokerAndroid> parent,
               scoped_refptr<const OnDeviceModelFeatureAdapter> adapter);
  ~SolutionImpl() override;

 private:
  // ModelBrokerImpl::Solution:
  bool IsValid() const override;
  mojom::ModelSolutionConfigPtr MakeConfig() const override;

  // mojom::ModelSolution
  void CreateSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> pending,
      on_device_model::mojom::SessionParamsPtr params) override;
  void CreateTextSafetySession(
      mojo::PendingReceiver<on_device_model::mojom::TextSafetySession> pending)
      override;
  void ReportHealthyCompletion() override;

  base::WeakPtr<ModelBrokerAndroid> parent_;
  scoped_refptr<const OnDeviceModelFeatureAdapter> adapter_;
};

SolutionImpl::SolutionImpl(
    base::WeakPtr<ModelBrokerAndroid> parent,
    scoped_refptr<const OnDeviceModelFeatureAdapter> adapter)
    : parent_(std::move(parent)), adapter_(std::move(adapter)) {}
SolutionImpl::~SolutionImpl() = default;

bool SolutionImpl::IsValid() const {
  // TODO: crbug.com/441578339 - Implement.
  // Ensure parent_'s model is compatible with the adapter.
  return !!parent_;
}

mojom::ModelSolutionConfigPtr SolutionImpl::MakeConfig() const {
  auto config = mojom::ModelSolutionConfig::New();
  config->feature_config = mojo_base::ProtoWrapper(adapter_->config());
  // TODO: crbug.com/441578339 - Add model versions.
  config->model_versions =
      mojo_base::ProtoWrapper(proto::OnDeviceModelVersions());
  config->max_tokens = adapter_->GetTokenLimits().max_tokens;
  // TODO: crbug.com/442914748 - Add safety config.
  config->text_safety_config =
      mojo_base::ProtoWrapper(proto::FeatureTextSafetyConfiguration());
  return config;
}

void SolutionImpl::CreateSession(
    mojo::PendingReceiver<on_device_model::mojom::Session> pending,
    on_device_model::mojom::SessionParamsPtr params) {
  if (parent_) {
    parent_->GetOrCreateModelRemote(adapter_->config().feature())
        ->StartSession(std::move(pending), std::move(params));
  }
}

void SolutionImpl::CreateTextSafetySession(
    mojo::PendingReceiver<on_device_model::mojom::TextSafetySession> pending) {
  // TODO: crbug.com/442914748 - Implement (can be a no-op initially).
}

void SolutionImpl::ReportHealthyCompletion() {
  // TODO: crbug.com/441578339 - Implement (this could potentially do nothing).
}

}  // namespace

ModelBrokerAndroid::ModelService::ModelService() = default;
ModelBrokerAndroid::ModelService::~ModelService() = default;
ModelBrokerAndroid::ModelService::ModelService(ModelService&&) = default;
ModelBrokerAndroid::ModelService& ModelBrokerAndroid::ModelService::operator=(
    ModelService&&) = default;

// Lazily initialized object that utilizes AiCore.
class ModelBrokerAndroid::SolutionFactory final
    : public UsageTracker::Observer {
 public:
  explicit SolutionFactory(ModelBrokerAndroid& parent);
  ~SolutionFactory() override;

  // Updates model availability for all features, e.g. for a change in a shared
  // asset like a safety config.
  void UpdateSolutionProviders();
  // Updates model availability for one feature.
  void UpdateSolutionProvider(ModelBasedCapabilityKey feature);

 private:
  // UsageTracker::Observer
  void OnDeviceEligibleFeatureFirstUsed(
      ModelBasedCapabilityKey feature) override;

  // Asks AICore to download the base model.
  void StartDownload(ModelBasedCapabilityKey feature);

  // Called when an AICore model was found (or not supported).
  void OnAICoreModelUpdated(
      ModelBasedCapabilityKey feature,
      base::expected<BaseModelSpec, DownloadFailureReason> result);

  // Updates the model adaptation for the feature.
  void MaybeUpdateModelAdaptation(ModelBasedCapabilityKey feature,
                                  MaybeAdaptationMetadata adaptation_metadata);

  // Constructs a solution for the feature.
  ModelBrokerImpl::MaybeSolution MakeSolution(ModelBasedCapabilityKey feature);

  // The broker that owns |this|.
  raw_ref<ModelBrokerAndroid> parent_;

  // Current registrations to fetch adaptation assets.
  AdaptationLoaderMap loader_map_;

  // The current model adaptation assets.
  AdaptationMetadataMap adaptation_metadata_;

  // Map from feature to the downloader for the base model. The downloader is
  // not null if and only if a download is ongoing.
  absl::flat_hash_map<ModelBasedCapabilityKey,
                      std::unique_ptr<on_device_model::ModelDownloaderAndroid>>
      model_downloaders_;

  base::WeakPtrFactory<ModelBrokerAndroid::SolutionFactory> weak_ptr_factory_{
      this};
};

ModelBrokerAndroid::SolutionFactory::SolutionFactory(ModelBrokerAndroid& parent)
    : parent_(parent),
      loader_map_(
          *parent.model_provider_,
          base::BindRepeating(&SolutionFactory::MaybeUpdateModelAdaptation,
                              base::Unretained(this))) {
  parent_->usage_tracker_.AddObserver(this);
  // Start model downloads for recently used features
  for (auto feature : kAllModelBasedCapabilityKeys) {
    if (parent_->usage_tracker_.WasOnDeviceEligibleFeatureRecentlyUsed(
            feature)) {
      StartDownload(feature);
    }
  }
}
ModelBrokerAndroid::SolutionFactory::~SolutionFactory() {
  parent_->usage_tracker_.RemoveObserver(this);
}

void ModelBrokerAndroid::SolutionFactory::OnDeviceEligibleFeatureFirstUsed(
    ModelBasedCapabilityKey feature) {
  StartDownload(feature);
}

void ModelBrokerAndroid::SolutionFactory::StartDownload(
    ModelBasedCapabilityKey feature) {
  // If there is an ongoing download, do nothing.
  if (model_downloaders_.contains(feature)) {
    return;
  }
  model_downloaders_[feature] =
      std::make_unique<on_device_model::ModelDownloaderAndroid>(
          ToModelExecutionFeatureProto(feature));
  model_downloaders_[feature]->StartDownload(
      base::BindOnce(&SolutionFactory::OnAICoreModelUpdated,
                     weak_ptr_factory_.GetWeakPtr(), feature));
  UpdateSolutionProvider(feature);
}

void ModelBrokerAndroid::SolutionFactory::OnAICoreModelUpdated(
    ModelBasedCapabilityKey feature,
    base::expected<BaseModelSpec, DownloadFailureReason> result) {
  // The download has completed, so the downloader can be removed.
  model_downloaders_.erase(feature);
  if (result.has_value()) {
    // Performance hint is not supported on Android.
    OnDeviceBaseModelSpec spec{
        result->name, result->version,
        proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_UNSPECIFIED};
    loader_map_.MaybeRegisterModelDownload(
        feature, spec,
        parent_->usage_tracker_.WasOnDeviceEligibleFeatureRecentlyUsed(
            feature));
  } else {
    MaybeUpdateModelAdaptation(
        feature, base::unexpected(AdaptationUnavailability::kNotSupported));
  }
}

void ModelBrokerAndroid::SolutionFactory::MaybeUpdateModelAdaptation(
    ModelBasedCapabilityKey feature,
    MaybeAdaptationMetadata adaptation_metadata) {
  if (adaptation_metadata_.MaybeUpdate(feature,
                                       std::move(adaptation_metadata))) {
    UpdateSolutionProvider(feature);
  }
}

void ModelBrokerAndroid::SolutionFactory::UpdateSolutionProviders() {
  auto keys = parent_->impl_.GetCapabilityKeys();
  for (const auto& key : keys) {
    UpdateSolutionProvider(key);
  }
}

void ModelBrokerAndroid::SolutionFactory::UpdateSolutionProvider(
    ModelBasedCapabilityKey feature) {
  parent_->impl_.GetSolutionProvider(feature).Update(MakeSolution(feature));
}

ModelBrokerImpl::MaybeSolution
ModelBrokerAndroid::SolutionFactory::MakeSolution(
    ModelBasedCapabilityKey feature) {
  if (!features::internal::GetOptimizationTargetForCapability(feature)) {
    return base::unexpected(
        OnDeviceModelEligibilityReason::kFeatureExecutionNotEnabled);
  }

  // Check feature config.
  MaybeAdaptationMetadata metadata = adaptation_metadata_.Get(feature);
  if (!metadata.has_value()) {
    if (metadata.error() == AdaptationUnavailability::kNotSupported) {
      return base::unexpected(
          OnDeviceModelEligibilityReason::kModelAdaptationNotAvailable);
    }
    return base::unexpected(
        OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature);
  }

  if (!metadata->adapter()->CanSkipTextSafety()) {
    // TODO: crbug.com/442914748 - Support text safety.
    return base::unexpected(
        OnDeviceModelEligibilityReason::kModelAdaptationNotAvailable);
  }

  return std::make_unique<SolutionImpl>(parent_->weak_ptr_factory_.GetWeakPtr(),
                                        metadata->adapter());
}

ModelBrokerAndroid::ModelBrokerAndroid(
    PrefService& local_state,
    OptimizationGuideModelProvider& model_provider)
    : model_provider_(model_provider),
      usage_tracker_(&local_state),
      impl_(usage_tracker_,
            base::BindRepeating(&ModelBrokerAndroid::EnsureSolutionFactory,
                                base::Unretained(this))) {}
ModelBrokerAndroid::~ModelBrokerAndroid() = default;

void ModelBrokerAndroid::BindBroker(
    mojo::PendingReceiver<mojom::ModelBroker> receiver) {
  impl_.BindBroker(std::move(receiver));
}

mojo::Remote<on_device_model::mojom::OnDeviceModel>&
ModelBrokerAndroid::GetOrCreateModelRemote(
    proto::ModelExecutionFeature feature) {
  if (!model_services_.contains(feature)) {
    ModelService service;
    auto backend_model =
        std::make_unique<on_device_model::BackendModelImplAndroid>(feature);
    service.impl = std::make_unique<on_device_model::OnDeviceModelMojomImpl>(
        std::move(backend_model), service.remote.BindNewPipeAndPassReceiver(),
        base::BindOnce(&ModelBrokerAndroid::OnModelDisconnected,
                       weak_ptr_factory_.GetWeakPtr(), feature));
    service.remote.reset_on_disconnect();
    service.remote.reset_on_idle_timeout(
        features::GetOnDeviceModelIdleTimeout());
    model_services_.emplace(feature, std::move(service));
  }
  return model_services_.at(feature).remote;
}

void ModelBrokerAndroid::EnsureSolutionFactory(
    base::OnceClosure done_callback) {
  if (!solution_factory_) {
    solution_factory_ = std::make_unique<SolutionFactory>(*this);
  }
  std::move(done_callback).Run();
}

void ModelBrokerAndroid::OnModelDisconnected(
    proto::ModelExecutionFeature feature,
    base::WeakPtr<on_device_model::mojom::OnDeviceModel> model) {
  model_services_.erase(feature);
}

}  // namespace optimization_guide
