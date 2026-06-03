// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/android/model_broker_android.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
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
using AICoreFeature = proto::ModelExecutionFeature;

// Returns the AICore feature for the given OnDeviceFeature, or std::nullopt if
// the feature is not supported on Android via AICore. Each supported feature is
// gated by a BASE_FEATURE flag defined below. To enable a new
// Android-supported feature, add a case here.
std::optional<AICoreFeature> GetAICoreFeatureFor(
    mojom::OnDeviceFeature feature) {
  switch (feature) {
    // The kSummarize on-device feature currently uses AICore Prompt feature
    // serving model execution on Android.
    case mojom::OnDeviceFeature::kSummarize:
    case mojom::OnDeviceFeature::kPromptApi:
      if (base::FeatureList::IsEnabled(features::kAICorePrompt)) {
        return proto::MODEL_EXECUTION_FEATURE_PROMPT_API;
      }
      break;
    case mojom::OnDeviceFeature::kScamDetection:
      if (base::FeatureList::IsEnabled(features::kAICoreScamDetection)) {
        return proto::MODEL_EXECUTION_FEATURE_SCAM_DETECTION;
      }
      break;
    case mojom::OnDeviceFeature::kTest:
      if (base::FeatureList::IsEnabled(features::kAICoreTest)) {
        return proto::MODEL_EXECUTION_FEATURE_TEST;
      }
      break;
    default:
      break;
  }
  return std::nullopt;
}

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

on_device_model::Capabilities AICoreModelCapabilities() {
  // AICore interface supports image input but doesn't support audio input.
  // Ideally the capabilities should be queried from AICore, but AICore doesn't
  // provide an API for this yet, so we hardcode them here based on AICore
  // documentation.
  return {on_device_model::CapabilityFlags::kImageInput};
}

// Returns an ineligibility reason if the AICore model is not ready, or
// std::nullopt if the model is available and we should continue.
std::optional<OnDeviceModelEligibilityReason> GetModelIneligibilityReason(
    const std::optional<on_device_model::ModelDownloaderAndroid::ModelStatus>&
        status) {
  // The model status is unknown, return std::nullopt to indicate the "unknown"
  // status, so the caller can proceed with other flow.
  if (!status.has_value()) {
    return std::nullopt;
  }

  switch (status.value()) {
    // The backend API is not constructed. This usually returns from Chrome
    // branded builds whose backend API is not constructed via MLKit library, so
    // the model status will always be marked as kApiNotAvailable. We return
    // std::nullopt in this case to allow the flow to continue since Chrome
    // branded builds may not check model status via MLKit.
    case on_device_model::ModelDownloaderAndroid::ModelStatus::kApiNotAvailable:
      return std::nullopt;
    // The device is not eligible to have the on-device model capability.
    // Return kModelNotEligible to indicate the ineligibility specifically.
    case on_device_model::ModelDownloaderAndroid::ModelStatus::kUnavailable:
      return OnDeviceModelEligibilityReason::kModelNotEligible;
    // The model is not yet available but can be downloaded. Return
    // kNoOnDeviceFeatureUsed which may prompt the caller to trigger the
    // download.
    case on_device_model::ModelDownloaderAndroid::ModelStatus::kDownloadable:
      return OnDeviceModelEligibilityReason::kNoOnDeviceFeatureUsed;
    // The model downloading is in progress but not completed. Return
    // kModelToBeInstalled to indicate the model is on the way.
    case on_device_model::ModelDownloaderAndroid::ModelStatus::kDownloading:
      return OnDeviceModelEligibilityReason::kModelToBeInstalled;
    // kAvailable means the model is available for use. Return std::nullopt so
    // the caller can proceed with other flow.
    case on_device_model::ModelDownloaderAndroid::ModelStatus::kAvailable:
      return std::nullopt;
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
  // TODO: crbug.com/442914748 - Add safety config.
  config->text_safety_config =
      mojo_base::ProtoWrapper(proto::FeatureTextSafetyConfiguration());
  config->model_capabilities = AICoreModelCapabilities();
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
// kAICorePrompt is not yet implemented in Chrome-branded builds, so disable it
// there to avoid assertion failures, while kAICoreScamDetection is only
// available in Chrome-branded builds, so disable it in non-Chrome-branded
// builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
BASE_FEATURE(kAICorePrompt, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAICoreScamDetection, base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kAICorePrompt, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAICoreScamDetection, base::FEATURE_DISABLED_BY_DEFAULT);
#endif
BASE_FEATURE(kAICoreTest, base::FEATURE_DISABLED_BY_DEFAULT);
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

  // Checks the AICore model status for all enabled features. Calls
  // |done_callback| when all status checks complete.
  void CheckModelStatus(base::OnceClosure done_callback);

  // Updates model availability for all features, e.g. for a change in a shared
  // asset like a safety config.
  void UpdateSolutionProviders();
  // Updates model availability for one feature.
  void UpdateSolutionProvider(mojom::OnDeviceFeature feature);

 private:
  // UsageTracker::Observer
  void OnDeviceEligibleUseCaseUsed(const std::string& use_case_name,
                                   bool is_first_usage) override;

  // Asks AICore to download the base model.
  void MaybeStartDownload(mojom::OnDeviceFeature feature);

  // Called when an AICore model was found (or not supported).
  void OnAICoreModelUpdated(
      AICoreFeature aicore_feature,
      base::expected<BaseModelSpec, DownloadFailureReason> result);

  // Called when CheckStatus completes. Calls |barrier_done_callback| when
  // finished.
  void OnAICoreModelStatusChecked(
      AICoreFeature aicore_feature,
      base::OnceClosure barrier_done_callback,
      on_device_model::ModelDownloaderAndroid::ModelStatus status);

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

  // Holds the model spec for each available AICore feature, or a status if spec
  // is not available. The spec is set when base model download completes.
  absl::flat_hash_map<
      AICoreFeature,
      base::expected<OnDeviceBaseModelSpec,
                     on_device_model::ModelDownloaderAndroid::ModelStatus>>
      base_model_specs_;

  // Map from AICore feature to the downloader for the base model. The
  // downloader is not null if and only if a download is ongoing.
  absl::flat_hash_map<AICoreFeature,
                      std::unique_ptr<on_device_model::ModelDownloaderAndroid>>
      model_downloaders_;

  // Downloaders for ongoing check status calls.
  absl::flat_hash_map<AICoreFeature,
                      std::unique_ptr<on_device_model::ModelDownloaderAndroid>>
      status_checkers_;

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
    if (parent_->usage_tracker_.WasUseCaseRecentlyUsed(
            ToUseCaseName(feature))) {
      MaybeStartDownload(feature);
    }
  }
}
ModelBrokerAndroid::SolutionFactory::~SolutionFactory() {
  parent_->usage_tracker_.RemoveObserver(this);
}

void ModelBrokerAndroid::SolutionFactory::OnDeviceEligibleUseCaseUsed(
    const std::string& use_case_name,
    bool is_first_usage) {
  if (!is_first_usage) {
    return;
  }
  auto feature = GetFeatureForUseCase(use_case_name);
  if (!feature) {
    return;
  }

  MaybeStartDownload(*feature);
}

void ModelBrokerAndroid::SolutionFactory::MaybeStartDownload(
    mojom::OnDeviceFeature feature) {
  if (!IsModelAllowed(&(*parent_->local_state_))) {
    MaybeUpdateModelAdaptation(
        feature, base::unexpected(AdaptationUnavailability::kNotSupported));
    return;
  }

  const auto aicore_feature = GetAICoreFeatureFor(feature);
  if (!aicore_feature) {
    return;
  }

  // If there is an ongoing download for this AICore feature, do nothing.
  if (model_downloaders_.contains(*aicore_feature)) {
    return;
  }
  auto params = on_device_model::mojom::DownloaderParams::New();
  params->require_persistent_mode = RequirePersistentModeForFeature(feature);
  auto [it, inserted] = model_downloaders_.emplace(
      *aicore_feature,
      std::make_unique<on_device_model::ModelDownloaderAndroid>(
          *aicore_feature, std::move(params)));
  CHECK(inserted);
  auto& downloader = it->second;
  downloader->StartDownload(
      base::BindOnce(&SolutionFactory::OnAICoreModelUpdated,
                     weak_ptr_factory_.GetWeakPtr(), *aicore_feature),
      base::BindRepeating(&ModelBrokerAndroid::OnDownloadProgressUpdated,
                          parent_->weak_ptr_factory_.GetWeakPtr()));
  UpdateSolutionProvider(feature);
}

void ModelBrokerAndroid::SolutionFactory::OnAICoreModelUpdated(
    AICoreFeature aicore_feature,
    base::expected<BaseModelSpec, DownloadFailureReason> result) {
  // The download has completed, so the downloader can be removed.
  model_downloaders_.erase(aicore_feature);
  if (result.has_value()) {
    parent_->model_already_downloaded_ = true;
    parent_->has_active_download_progress_ = false;
    // Performance hint is not supported on Android.
    OnDeviceBaseModelSpec spec{
        result->name, result->version,
        proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_UNSPECIFIED};
    base_model_specs_.insert_or_assign(aicore_feature, spec);
    // Register the model download for all features that share the same
    // AICore feature, since multiple mojom::OnDeviceFeature values may map to
    // the same underlying model (e.g. kSummarize and kPromptApi may both map to
    // MODEL_EXECUTION_FEATURE_PROMPT_API).
    for (auto f : OnDeviceFeatureSet::All()) {
      if (GetAICoreFeatureFor(f) == aicore_feature) {
        loader_map_.MaybeRegisterModelDownload(
            f, spec,
            parent_->usage_tracker_.WasUseCaseRecentlyUsed(ToUseCaseName(f)));
      }
    }
  } else {
    // Notify all features that share this AICore feature of the failure.
    for (auto f : OnDeviceFeatureSet::All()) {
      if (GetAICoreFeatureFor(f) == aicore_feature) {
        MaybeUpdateModelAdaptation(
            f, base::unexpected(AdaptationUnavailability::kNotSupported));
      }
    }
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
  for (auto feature : OnDeviceFeatureSet::All()) {
    UpdateSolutionProvider(feature);
  }
}

void ModelBrokerAndroid::SolutionFactory::UpdateSolutionProvider(
    mojom::OnDeviceFeature feature) {
  parent_->impl_.GetSolutionProvider(feature).Update(MakeSolution(feature));
}

ModelBrokerImpl::MaybeSolution
ModelBrokerAndroid::SolutionFactory::MakeSolution(
    mojom::OnDeviceFeature feature) {
  const auto aicore_feature = GetAICoreFeatureFor(feature);
  if (!aicore_feature) {
    return base::unexpected(
        OnDeviceModelEligibilityReason::kFeatureExecutionNotEnabled);
  }

  auto spec_it = base_model_specs_.find(*aicore_feature);

  // No spec or status record yet.
  if (spec_it == base_model_specs_.end()) {
    return base::unexpected(OnDeviceModelEligibilityReason::kValidationPending);
  }

  // Check base model availability.
  if (!spec_it->second.has_value()) {
    if (auto ineligibility_reason =
            GetModelIneligibilityReason(spec_it->second.error())) {
      return base::unexpected(*ineligibility_reason);
    }
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

  // Base model specs should always be set before adaptation becomes available.
  return std::make_unique<SolutionImpl>(
      parent_->weak_ptr_factory_.GetWeakPtr(), metadata->adapter(),
      spec_it->second.value(), metadata->version());
}

void ModelBrokerAndroid::SolutionFactory::CheckModelStatus(
    base::OnceClosure done_callback) {
  CHECK(status_checkers_.empty());

  // Build the set of unique AICore features to check by iterating all
  // OnDeviceFeatures and collecting those enabled on Android.
  base::flat_set<AICoreFeature> aicore_features;
  for (auto feature : OnDeviceFeatureSet::All()) {
    if (auto aicore_feature = GetAICoreFeatureFor(feature)) {
      aicore_features.insert(*aicore_feature);
    }
  }

  // Use a BarrierClosure to wait for all status checks to complete before
  // firing the done callback. The done closure signals completion and updates
  // solutions.
  auto all_done = base::BarrierClosure(
      aicore_features.size(),
      base::BindOnce(
          [](base::WeakPtr<SolutionFactory> self, base::OnceClosure callback) {
            if (!self) {
              return;
            }
            self->UpdateSolutionProviders();
            std::move(callback).Run();
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(done_callback)));

  for (auto aicore_feature : aicore_features) {
    auto [it, inserted] = status_checkers_.emplace(
        aicore_feature,
        std::make_unique<on_device_model::ModelDownloaderAndroid>(
            aicore_feature, on_device_model::mojom::DownloaderParams::New()));
    CHECK(inserted);
    auto& status_checker = it->second;
    status_checker->CheckStatus(
        base::BindOnce(&SolutionFactory::OnAICoreModelStatusChecked,
                       weak_ptr_factory_.GetWeakPtr(), aicore_feature,
                       base::OnceClosure(all_done)));
  }
}

void ModelBrokerAndroid::SolutionFactory::OnAICoreModelStatusChecked(
    AICoreFeature aicore_feature,
    base::OnceClosure barrier_done_callback,
    on_device_model::ModelDownloaderAndroid::ModelStatus status) {
  status_checkers_.erase(aicore_feature);

  switch (status) {
    case on_device_model::ModelDownloaderAndroid::ModelStatus::kUnavailable:
    case on_device_model::ModelDownloaderAndroid::ModelStatus::kDownloadable:
    case on_device_model::ModelDownloaderAndroid::ModelStatus::kDownloading:
      // The model is not available. Insert or overwrite any previously stored
      // status.
      base_model_specs_.insert_or_assign(aicore_feature,
                                         base::unexpected(status));
      break;
    case on_device_model::ModelDownloaderAndroid::ModelStatus::kAvailable:
      // The model is available but we may not have a valid spec yet. Store the
      // status so MakeSolution knows the status check completed. Skip if a
      // valid spec is already present — MaybeStartDownload for recently used
      // features may have completed a download before the status check fires.
      if (auto it = base_model_specs_.find(aicore_feature);
          it == base_model_specs_.end() || !it->second.has_value()) {
        base_model_specs_.insert_or_assign(aicore_feature,
                                           base::unexpected(status));
      }
      break;
    case on_device_model::ModelDownloaderAndroid::ModelStatus::kApiNotAvailable:
      // Store the status so MakeSolution knows the status check completed.
      // This usually happens on Chrome branded builds whose backend API is not
      // constructed via MLKit library.
      base_model_specs_.insert_or_assign(aicore_feature,
                                         base::unexpected(status));
      break;
  }

  std::move(barrier_done_callback).Run();
}

ModelBrokerAndroid::ModelBrokerAndroid(
    PrefService& local_state,
    OptimizationGuideModelProvider& model_provider)
    : local_state_(local_state),
      model_provider_(model_provider),
      usage_tracker_(&local_state),
      impl_(usage_tracker_,
            base::BindRepeating(&ModelBrokerAndroid::EnsureSolutionFactory,
                                base::Unretained(this)),
            base::BindRepeating(
                &ModelBrokerAndroid::AddModelDownloadProgressObserver,
                base::Unretained(this))) {}
ModelBrokerAndroid::~ModelBrokerAndroid() = default;

void ModelBrokerAndroid::BindModelBroker(
    mojo::PendingReceiver<mojom::ModelBroker> receiver) {
  if (features::IsOnDeviceExecutionEnabled()) {
    impl_.BindBroker(std::move(receiver));
  }
}

void ModelBrokerAndroid::BindModelBrokerDebug(
    base::PassKey<on_device_internals::PageHandler> key,
    mojo::PendingReceiver<mojom::ModelBrokerDebug> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ModelBrokerAndroid::GetStateInfo(
    mojom::ModelBrokerDebug::GetStateInfoCallback callback) {
  auto result = mojom::BrokerStateInfo::New();
  // TODO: crbug.com/489511500 - Expose relevant info.
  // result->properties = performance_classifier_.GetBrokerProperties();
  // base::Extend(result->properties,
  //              component_state_manager_.GetBrokerProperties());
  // result->assets = component_state_manager_.GetBrokerAssets();
  result->use_cases = impl_.GetBrokerUseCaseInfo();
  // result->models = base_model_controller_.GetBrokerModels();
  std::move(callback).Run(std::move(result));
}

void ModelBrokerAndroid::SetUseCaseRequested(const std::string& use_case,
                                             bool requested) {
  usage_tracker_.SetUseCaseRequested(use_case, requested);
}

void ModelBrokerAndroid::UninstallModels() {
  // Not supported for android, since we don't own the models.
}

mojo::Remote<on_device_model::mojom::OnDeviceModel>&
ModelBrokerAndroid::GetOrCreateModelRemote(AICoreFeature feature) {
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
    ModelBrokerImpl::InitCallback init_callback) {
  if (status_check_complete_) {
    std::move(init_callback).Run(AICoreModelCapabilities());
    return;
  }

  pending_init_callbacks_.push_back(std::move(init_callback));

  if (!solution_factory_) {
    solution_factory_ = std::make_unique<SolutionFactory>(*this);
    solution_factory_->CheckModelStatus(
        base::BindOnce(&ModelBrokerAndroid::OnStatusCheckComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ModelBrokerAndroid::OnStatusCheckComplete() {
  status_check_complete_ = true;
  std::vector<ModelBrokerImpl::InitCallback> callbacks =
      std::move(pending_init_callbacks_);
  pending_init_callbacks_.clear();
  for (auto& callback : callbacks) {
    std::move(callback).Run(AICoreModelCapabilities());
  }
}

void ModelBrokerAndroid::OnModelDisconnected(
    proto::ModelExecutionFeature feature,
    base::WeakPtr<on_device_model::mojom::OnDeviceModel> model) {
  model_services_.erase(feature);
}

void ModelBrokerAndroid::AddModelDownloadProgressObserver(
    const std::string& use_case,
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer) {
  auto id = download_observers_.Add(std::move(observer));
  // Blink's CreateMonitor CHECKs that the first progress update has
  // downloaded_bytes == 0 (see create_monitor.cc). When an observer joins
  // mid-download, we send an initial (0, max) event to satisfy this.
  if (has_active_download_progress_) {
    download_observers_.Get(id)->OnDownloadProgressUpdate(
        0, kNormalizedDownloadProgressMax);
    return;
  }

  // The model was already downloaded when this observer joined. Send 0% and
  // 100% to match desktop behavior.
  if (model_already_downloaded_) {
    download_observers_.Get(id)->OnDownloadProgressUpdate(
        0, kNormalizedDownloadProgressMax);
    download_observers_.Get(id)->OnDownloadProgressUpdate(
        kNormalizedDownloadProgressMax, kNormalizedDownloadProgressMax);
  }
}

void ModelBrokerAndroid::OnDownloadProgressUpdated(int64_t downloaded_bytes,
                                                   int64_t total_bytes) {
  has_active_download_progress_ = true;
  int64_t normalized_progress =
      NormalizeModelDownloadProgress(downloaded_bytes, total_bytes);
  for (auto& observer : download_observers_) {
    observer->OnDownloadProgressUpdate(normalized_progress,
                                       kNormalizedDownloadProgressMax);
  }
}

}  // namespace optimization_guide
