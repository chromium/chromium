// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/android/model_broker_android.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "services/on_device_model/android/backend_model_impl_android.h"
#include "services/on_device_model/android/downloader_params.mojom.h"
#include "services/on_device_model/android/model_downloader_android.h"
#include "services/on_device_model/on_device_model_mojom_impl.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace optimization_guide {

namespace {

using BaseModelSpec = on_device_model::ModelDownloaderAndroid::BaseModelSpec;
using DownloadFailureReason =
    on_device_model::ModelDownloaderAndroid::DownloadFailureReason;

bool IsModelAllowed(PrefService* local_state) {
  return features::IsOnDeviceExecutionEnabled() &&
         optimization_guide::
                 GetGenAILocalFoundationalModelEnterprisePolicySettings(
                     local_state) ==
             model_execution::prefs::
                 GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed;
}

proto::OnDeviceModelVersions GetModelVersions(const OnDeviceBaseModelSpec& spec,
                                              int64_t adaptation_version) {
  proto::OnDeviceModelVersions versions;
  auto* on_device_model_version =
      versions.mutable_on_device_model_service_version();
  on_device_model_version->set_model_adaptation_version(adaptation_version);
  auto* base_model_metadata =
      on_device_model_version->mutable_on_device_base_model_metadata();
  base_model_metadata->set_base_model_name(spec.model_name);
  base_model_metadata->set_base_model_version(spec.model_version);
  return versions;
}

bool RequirePersistentModeForFeature(mojom::OnDeviceFeature feature) {
  switch (feature) {
    case mojom::OnDeviceFeature::kScamDetection:
      // TODO(crbug.com/428248156): Pending decision on whether it is required
      // to gate scam detection on persistent mode, which may limit device reach
      // on other OEMs.
      return base::FeatureList::IsEnabled(
          features::kRequirePersistentModeForScamDetection);
    default:
      return true;
  }
}

class SolutionImpl : public ModelBrokerImpl::Solution {
 public:
  SolutionImpl(base::WeakPtr<ModelBrokerAndroid> parent,
               scoped_refptr<const OnDeviceModelFeatureAdapter> adapter,
               const OnDeviceBaseModelSpec& spec,
               int64_t adaptation_version);
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
  const OnDeviceBaseModelSpec spec_;
  const int64_t adaptation_version_;
};

SolutionImpl::SolutionImpl(
    base::WeakPtr<ModelBrokerAndroid> parent,
    scoped_refptr<const OnDeviceModelFeatureAdapter> adapter,
    const OnDeviceBaseModelSpec& spec,
    int64_t adaptation_version)
    : parent_(std::move(parent)),
      adapter_(std::move(adapter)),
      spec_(spec),
      adaptation_version_(adaptation_version) {}
SolutionImpl::~SolutionImpl() = default;

bool SolutionImpl::IsValid() const {
  // TODO: crbug.com/441578339 - Implement.
  // Ensure parent_'s model is compatible with the adapter.
  return !!parent_;
}

mojom::ModelSolutionConfigPtr SolutionImpl::MakeConfig() const {
  auto config = mojom::ModelSolutionConfig::New();
  config->feature_config = mojo_base::ProtoWrapper(adapter_->config());
  config->model_versions =
      mojo_base::ProtoWrapper(GetModelVersions(spec_, adaptation_version_));
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

namespace features {
BASE_FEATURE(kRequirePersistentModeForScamDetection,
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

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
  void UpdateSolutionProvider(mojom::OnDeviceFeature feature);

 private:
  // UsageTracker::Observer
  void OnDeviceEligibleFeatureFirstUsed(
      mojom::OnDeviceFeature feature) override;

  // Asks AICore to download the base model.
  void MaybeStartDownload(mojom::OnDeviceFeature feature);

  // Called when an AICore model was found (or not supported).
  void OnAICoreModelUpdated(
      mojom::OnDeviceFeature feature,
      base::expected<BaseModelSpec, DownloadFailureReason> result);

  // Updates the model adaptation for the feature.
  void MaybeUpdateModelAdaptation(mojom::OnDeviceFeature feature,
                                  MaybeAdaptationMetadata adaptation_metadata);

  // Constructs a solution for the feature.
  ModelBrokerImpl::MaybeSolution MakeSolution(mojom::OnDeviceFeature feature);

  // The broker that owns |this|.
  raw_ref<ModelBrokerAndroid> parent_;

  // Current registrations to fetch adaptation assets.
  AdaptationLoaderMap loader_map_;

  // The current model adaptation assets.
  AdaptationMetadataMap adaptation_metadata_;

  // The base model spec for each feature. This is set when the base model is
  // downloaded.
  absl::flat_hash_map<mojom::OnDeviceFeature, OnDeviceBaseModelSpec>
      base_model_specs_;

  // Map from feature to the downloader for the base model. The downloader is
  // not null if and only if a download is ongoing.
  absl::flat_hash_map<mojom::OnDeviceFeature,
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
  for (auto feature : OnDeviceFeatureSet::All()) {
    if (parent_->usage_tracker_.WasOnDeviceEligibleFeatureRecentlyUsed(
            feature)) {
      MaybeStartDownload(feature);
    }
  }
}
ModelBrokerAndroid::SolutionFactory::~SolutionFactory() {
  parent_->usage_tracker_.RemoveObserver(this);
}

void ModelBrokerAndroid::SolutionFactory::OnDeviceEligibleFeatureFirstUsed(
    mojom::OnDeviceFeature feature) {
  MaybeStartDownload(feature);
}

void ModelBrokerAndroid::SolutionFactory::MaybeStartDownload(
    mojom::OnDeviceFeature feature) {
  if (!IsModelAllowed(&(*parent_->local_state_))) {
    MaybeUpdateModelAdaptation(
        feature, base::unexpected(AdaptationUnavailability::kNotSupported));
    return;
  }
  // If there is an ongoing download, do nothing.
  if (model_downloaders_.contains(feature)) {
    return;
  }
  auto params = on_device_model::mojom::DownloaderParams::New();
  params->require_persistent_mode = RequirePersistentModeForFeature(feature);
  model_downloaders_[feature] =
      std::make_unique<on_device_model::ModelDownloaderAndroid>(
          ToModelExecutionFeatureProto(feature), std::move(params));
  model_downloaders_[feature]->StartDownload(
      base::BindOnce(&SolutionFactory::OnAICoreModelUpdated,
                     weak_ptr_factory_.GetWeakPtr(), feature));
  UpdateSolutionProvider(feature);
}

void ModelBrokerAndroid::SolutionFactory::OnAICoreModelUpdated(
    mojom::OnDeviceFeature feature,
    base::expected<BaseModelSpec, DownloadFailureReason> result) {
  // The download has completed, so the downloader can be removed.
  model_downloaders_.erase(feature);
  if (result.has_value()) {
    // Performance hint is not supported on Android.
    OnDeviceBaseModelSpec spec{
        result->name, result->version,
        proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_UNSPECIFIED};
    base_model_specs_.insert_or_assign(feature, spec);
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
    mojom::OnDeviceFeature feature,
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
    mojom::OnDeviceFeature feature) {
  parent_->impl_.GetSolutionProvider(feature).Update(MakeSolution(feature));
}

ModelBrokerImpl::MaybeSolution
ModelBrokerAndroid::SolutionFactory::MakeSolution(
    mojom::OnDeviceFeature feature) {
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

  auto spec_it = base_model_specs_.find(feature);
  // Base model specs should always be set before adaptation becomes available.
  CHECK(spec_it != base_model_specs_.end());
  return std::make_unique<SolutionImpl>(parent_->weak_ptr_factory_.GetWeakPtr(),
                                        metadata->adapter(), spec_it->second,
                                        metadata->version());
}

ModelBrokerAndroid::ModelBrokerAndroid(
    PrefService& local_state,
    OptimizationGuideModelProvider& model_provider)
    : local_state_(local_state),
      model_provider_(model_provider),
      usage_tracker_(&local_state),
      impl_(usage_tracker_,
            base::BindRepeating(&ModelBrokerAndroid::EnsureSolutionFactory,
                                base::Unretained(this))) {}
ModelBrokerAndroid::~ModelBrokerAndroid() = default;

void ModelBrokerAndroid::BindModelBroker(
    mojo::PendingReceiver<mojom::ModelBroker> receiver) {
  if (features::IsOnDeviceExecutionEnabled()) {
    impl_.BindBroker(std::move(receiver));
  }
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
    service.remote.set_idle_handler(
        features::GetOnDeviceModelIdleTimeout(),
        base::BindRepeating(&ModelBrokerAndroid::OnModelDisconnected,
                            weak_ptr_factory_.GetWeakPtr(), feature, nullptr));
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
