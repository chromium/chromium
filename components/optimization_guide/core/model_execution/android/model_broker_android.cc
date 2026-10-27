// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/android/model_broker_android.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/token.h"
#include "build/branding_buildflags.h"
#include "components/optimization_guide/core/model_execution/android/android_manifest.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
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
using ModelStatus = on_device_model::ModelDownloaderAndroid::ModelStatus;
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
         model_execution::prefs::
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

OnDeviceModelEligibilityReason GetEligibilityReasonForModelStatus(
    ModelStatus status) {
  switch (status) {
    // The device is not eligible to have the on-device model capability.
    // Return kModelNotEligible to indicate the ineligibility specifically.
    case ModelStatus::kUnavailable:
      return OnDeviceModelEligibilityReason::kModelNotEligible;
    // The model is not yet available but can be downloaded. Return
    // kNoOnDeviceFeatureUsed which may prompt the caller to trigger the
    // download.
    case ModelStatus::kDownloadable:
      return OnDeviceModelEligibilityReason::kNoOnDeviceFeatureUsed;
    // The model downloading is in progress but not completed. Return
    // kModelToBeInstalled to indicate the model is on the way.
    case ModelStatus::kDownloading:
      return OnDeviceModelEligibilityReason::kModelToBeInstalled;
    // CheckStatus does not return the model specification, so report pending
    // until StartDownload supplies it.
    case ModelStatus::kAvailable:
      return OnDeviceModelEligibilityReason::kModelToBeInstalled;
    // The backend API is not constructed. This usually happens in
    // Chrome-branded builds that do not check model status via MLKit. Callers
    // must bypass eligibility mapping and use the normal download path.
    case ModelStatus::kApiNotAvailable:
      NOTREACHED();
  }
}

class SolutionImpl : public ModelBrokerImpl::Solution {
 public:
  SolutionImpl(base::WeakPtr<ModelBrokerAndroid> parent,
               scoped_refptr<const OnDeviceModelFeatureAdapter> adapter,
               const OnDeviceBaseModelSpec& spec,
               int64_t adaptation_version,
               std::optional<base::Token> manifest_id);
  ~SolutionImpl() override;

 private:
  // ModelBrokerImpl::Solution:
  bool IsValid() const override;
  mojom::ModelSolutionConfigPtr MakeConfig() const override;

  // mojom::ModelSolution
  void CreateSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> pending,
      on_device_model::mojom::SessionParamsPtr params) override;
  void ReportHealthyCompletion() override;

  base::WeakPtr<ModelBrokerAndroid> parent_;
  scoped_refptr<const OnDeviceModelFeatureAdapter> adapter_;
  const OnDeviceBaseModelSpec spec_;
  const int64_t adaptation_version_;
  const std::optional<base::Token> manifest_id_;
};

SolutionImpl::SolutionImpl(
    base::WeakPtr<ModelBrokerAndroid> parent,
    scoped_refptr<const OnDeviceModelFeatureAdapter> adapter,
    const OnDeviceBaseModelSpec& spec,
    int64_t adaptation_version,
    std::optional<base::Token> manifest_id)
    : parent_(std::move(parent)),
      adapter_(std::move(adapter)),
      spec_(spec),
      adaptation_version_(adaptation_version),
      manifest_id_(manifest_id) {}
SolutionImpl::~SolutionImpl() = default;

bool SolutionImpl::IsValid() const {
  // Adaptation-backed solutions (Scam Detection) have no manifest dependency,
  // so manifest replacement does not invalidate them. Manifest-backed solutions
  // become stale when the broker selects a different manifest.
  // TODO: crbug.com/441578339 - Implement.
  // Ensure parent_'s model is compatible with the adapter.
  return !!parent_ &&
         (!manifest_id_ || parent_->IsManifestActive(*manifest_id_));
}

mojom::ModelSolutionConfigPtr SolutionImpl::MakeConfig() const {
  auto config = mojom::ModelSolutionConfig::New();
  config->feature_config = mojo_base::ProtoWrapper(adapter_->config());
  config->model_versions =
      mojo_base::ProtoWrapper(GetModelVersions(spec_, adaptation_version_));
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

  // Updates model availability for all features.
  void UpdateSolutionProviders();
  // Updates model availability for one feature.
  void UpdateSolutionProvider(mojom::OnDeviceFeature feature);

 private:
  // UsageTracker::Observer
  void OnPriorityIncrease(const std::string& use_case_name,
                          UsageTracker::Priority previous_priority) override;

  // Asks AICore to download the base model.
  void MaybeStartDownload(mojom::OnDeviceFeature feature);

  // Called when an AICore model was found (or not supported).
  void OnAICoreModelUpdated(
      AICoreFeature aicore_feature,
      base::expected<BaseModelSpec, DownloadFailureReason> result);

  // Called when CheckStatus completes. Calls |barrier_done_callback| when
  // finished.
  void OnAICoreModelStatusChecked(AICoreFeature aicore_feature,
                                  base::OnceClosure barrier_done_callback,
                                  ModelStatus status);

  // Updates the model adaptation for the feature.
  void MaybeUpdateModelAdaptation(mojom::OnDeviceFeature feature,
                                  MaybeAdaptationMetadata adaptation_metadata);

  // Constructs a solution for the feature.
  ModelBrokerImpl::MaybeSolution MakeSolution(mojom::OnDeviceFeature feature);

  // The broker that owns |this|.
  raw_ref<ModelBrokerAndroid> parent_;

  // Fetches adaptations delivered through OptimizationGuideModelProvider.
  // This remains the active config path for features such as Scam Detection
  // and tests. Web AI APIs use configs from AndroidManifest instead.
  AdaptationLoaderMap loader_map_;

  // The current model adaptation assets.
  AdaptationMetadataMap adaptation_metadata_;

  // Holds the model spec for each available AICore feature, or an eligibility
  // reason if the spec is not available.
  absl::flat_hash_map<
      AICoreFeature,
      base::expected<OnDeviceBaseModelSpec, OnDeviceModelEligibilityReason>>
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
    if (parent_->usage_tracker_.GetPriority(ToUseCaseName(feature)) >=
        UsageTracker::Priority::kBestEffort) {
      MaybeStartDownload(feature);
    }
  }
}
ModelBrokerAndroid::SolutionFactory::~SolutionFactory() {
  parent_->usage_tracker_.RemoveObserver(this);
}

void ModelBrokerAndroid::SolutionFactory::OnPriorityIncrease(
    const std::string& use_case_name,
    UsageTracker::Priority previous_priority) {
  if (previous_priority >= UsageTracker::Priority::kBestEffort) {
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
    // Refresh availability without storing policy state as an adaptation result
    // since MakeSolution evaluates policy directly.
    UpdateSolutionProvider(feature);
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
    OnDeviceBaseModelSpec spec{.model_name = result->name,
                               .model_version = result->version};
    base_model_specs_.insert_or_assign(aicore_feature, spec);
    // Register the model download for all features that share the same
    // AICore feature, since multiple mojom::OnDeviceFeature values may map to
    // the same underlying model (e.g. kSummarize and kPromptApi may both map to
    // MODEL_EXECUTION_FEATURE_PROMPT_API).
    for (auto f : OnDeviceFeatureSet::All()) {
      if (GetAICoreFeatureFor(f) == aicore_feature) {
        loader_map_.MaybeRegisterModelDownload(
            f, spec,
            parent_->usage_tracker_.GetPriority(ToUseCaseName(f)) >=
                UsageTracker::Priority::kBestEffort);
        // Update solution because the returned model spec may allow
        // MakeSolution to select a solution config and publish an available
        // solution.
        UpdateSolutionProvider(f);
      }
    }
  } else {
    // ModelDownloaderAndroid already records the backend-specific failure
    // reason. The broker exposes a stable user-facing availability state, so
    // collapse download failures to a terminal result that completes pending
    // session requests.
    base_model_specs_.insert_or_assign(
        aicore_feature,
        base::unexpected(OnDeviceModelEligibilityReason::kModelNotEligible));
    // Notify all features that share this AICore feature of the failure.
    for (auto f : OnDeviceFeatureSet::All()) {
      if (GetAICoreFeatureFor(f) == aicore_feature) {
        UpdateSolutionProvider(f);
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

  if (!IsModelAllowed(&(*parent_->local_state_))) {
    // Preserve the existing policy-denial mapping previously represented by
    // AdaptationUnavailability::kNotSupported.
    return base::unexpected(
        OnDeviceModelEligibilityReason::kModelAdaptationNotAvailable);
  }

  auto spec_it = base_model_specs_.find(*aicore_feature);

  // No spec or status record yet.
  if (spec_it == base_model_specs_.end()) {
    return base::unexpected(OnDeviceModelEligibilityReason::kValidationPending);
  }

  // Check base model availability.
  if (!spec_it->second.has_value()) {
    return base::unexpected(spec_it->second.error());
  }

  const MaybeAdaptationMetadata& metadata = adaptation_metadata_.Get(feature);
  scoped_refptr<const OnDeviceModelFeatureAdapter> adapter;
  std::optional<base::Token> manifest_id;
  // Manifest configs do not have a downloaded adaptation version. Use 0 to
  // preserve the existing ModelSolutionConfig version shape.
  int64_t adaptation_version = 0;
  // EnsureSolutionFactory initializes the manifest before constructing the
  // factory. Add a CHECK to ensure the manifest exists.
  CHECK(parent_->manifest_);
  // Each feature is expected to use one config source for both construction and
  // updates, e.g., adaptations for Scam Detection and manifests for Summarizer.
  // A feature should not mix the two delivery paths. A late adaptation does not
  // replace an already-valid manifest solution: SolutionProvider::Update
  // retains it until its snapshot is invalidated.
  if (metadata.has_value()) {
    // A feature must not have both adaptation and manifest configs.
    CHECK(!parent_->manifest_->SelectConfig(feature));
    adapter = metadata->adapter();
    adaptation_version = metadata->version();
  } else if (auto config = parent_->manifest_->SelectConfig(feature)) {
    adapter = std::move(config);
    manifest_id = parent_->manifest_->id();
  } else {
    return base::unexpected(
        metadata.error() == AdaptationUnavailability::kNotSupported
            ? OnDeviceModelEligibilityReason::kModelAdaptationNotAvailable
            : OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature);
  }

  // A base model spec is required for both adaptation and manifest solutions.
  return std::make_unique<SolutionImpl>(
      parent_->weak_ptr_factory_.GetWeakPtr(), std::move(adapter),
      spec_it->second.value(), adaptation_version, manifest_id);
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
    ModelStatus status) {
  status_checkers_.erase(aicore_feature);

  switch (status) {
    case ModelStatus::kUnavailable:
    case ModelStatus::kDownloadable:
    case ModelStatus::kDownloading:
      // The model is not available. Insert or overwrite any previously stored
      // status.
      base_model_specs_.insert_or_assign(
          aicore_feature,
          base::unexpected(GetEligibilityReasonForModelStatus(status)));
      break;
    case ModelStatus::kAvailable:
      // CheckStatus does not return a model specification. Request the
      // already-installed model so StartDownload resolves with its spec. The
      // model is already installed, so this only retrieves its base model name
      // and does not initiate a download or emit progress events. Skip this if
      // MaybeStartDownload for a recently used feature already supplied one
      // before the status check completed.
      if (auto it = base_model_specs_.find(aicore_feature);
          it == base_model_specs_.end() || !it->second.has_value()) {
        base_model_specs_.insert_or_assign(
            aicore_feature,
            base::unexpected(GetEligibilityReasonForModelStatus(status)));
        for (auto feature : OnDeviceFeatureSet::All()) {
          if (GetAICoreFeatureFor(feature) == aicore_feature) {
            MaybeStartDownload(feature);
          }
        }
      }
      break;
    case ModelStatus::kApiNotAvailable:
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
                base::Unretained(this))) {
  impl_.SetFeatureConfigs(AndroidManifest::GetFeatureRoutingConfigs());
}
ModelBrokerAndroid::~ModelBrokerAndroid() = default;

void ModelBrokerAndroid::OnManifestLoaded(
    base::expected<AndroidManifest, AndroidManifest::ParseError> manifest) {
  if (!manifest) {
    // Ignore the manifest if it failed to parse.
    // TODO(crbug.com/575550015): Handle manifest parse errors appropriately.
    return;
  }

  // Activating a different snapshot invalidates solutions whose snapshot IDs
  // no longer match. Store it even before first use, without initializing
  // AICore.
  manifest_ = std::move(*manifest);
  if (solution_factory_) {
    solution_factory_->UpdateSolutionProviders();
  }
}

bool ModelBrokerAndroid::IsManifestActive(
    const base::Token& manifest_id) const {
  return manifest_ && manifest_->id() == manifest_id;
}

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

  // Model crash count parameters are omitted because Android's impl
  // doesn't track or use the crash counts (the crash count card won't
  // be displayed in the UI).

  std::move(callback).Run(std::move(result));
}

void ModelBrokerAndroid::SetUseCaseRequested(const std::string& use_case,
                                             bool requested) {
  usage_tracker_.SetPriority(use_case,
                             requested ? UsageTracker::Priority::kUserBlocking
                                       : UsageTracker::Priority::kEvictable);
}

void ModelBrokerAndroid::UninstallModels() {
  // Not supported for android, since we don't own the models.
}

void ModelBrokerAndroid::ResetModelCrashCount() {
  // Omitted because Android's impl doesn't track or use crash counts.
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
        base::Minutes(1),
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

  // Preserve a component snapshot received before first use. Initialize the
  // fallback before the factory constructor can request solutions.
  if (!manifest_) {
    manifest_ = AndroidManifest::CreateFromBuiltin();
  }

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

void ModelBrokerAndroid::AddObserver(
    mojo::PendingRemote<mojom::ModelBrokerDebugObserver>) {
  // Not yet implemented on Android.
}

void ModelBrokerAndroid::AddAssetDownloadObserver(
    const std::string& asset_name,
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer) {
  // Not yet implemented on Android.
}

}  // namespace optimization_guide
