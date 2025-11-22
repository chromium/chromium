// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/safety_client.h"
#include "components/optimization_guide/core/model_execution/safety_config.h"
#include "components/optimization_guide/core/model_execution/safety_model_info.h"
#include "components/optimization_guide/core/model_execution/session_impl.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/cpp/text_safety_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

namespace {

proto::OnDeviceModelVersions GetModelVersions(
    const OnDeviceModelMetadata& model_metadata,
    const SafetyClient& safety_client,
    std::optional<int64_t> adaptation_version) {
  proto::OnDeviceModelVersions versions;
  auto* on_device_model_version =
      versions.mutable_on_device_model_service_version();
  on_device_model_version->set_component_version(model_metadata.version());
  on_device_model_version->mutable_on_device_base_model_metadata()
      ->set_base_model_name(model_metadata.model_spec().model_name);
  on_device_model_version->mutable_on_device_base_model_metadata()
      ->set_base_model_version(model_metadata.model_spec().model_version);

  if (safety_client.safety_model_info()) {
    versions.set_text_safety_model_version(
        safety_client.safety_model_info()->GetVersion());
  }

  if (adaptation_version) {
    on_device_model_version->set_model_adaptation_version(*adaptation_version);
  }

  return versions;
}

void CloseFilesInBackground(on_device_model::ModelAssets assets) {
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::DoNothingWithBoundArgs(std::move(assets)));
}

OnDeviceModelEligibilityReason GetBaseModelError(
    mojom::OnDeviceFeature feature,
    OnDeviceModelComponentStateManager* state_manager) {
  if (!state_manager) {
    return OnDeviceModelEligibilityReason::kModelNotEligible;
  }
  OnDeviceModelStatus on_device_model_status =
      state_manager->GetOnDeviceModelStatus();

  switch (on_device_model_status) {
    case OnDeviceModelStatus::kNotEligible:
      return OnDeviceModelEligibilityReason::kModelNotEligible;
    case OnDeviceModelStatus::kInsufficientDiskSpace:
      return OnDeviceModelEligibilityReason::kInsufficientDiskSpace;
    case OnDeviceModelStatus::kInstallNotComplete:
    case OnDeviceModelStatus::kModelInstallerNotRegisteredForUnknownReason:
    case OnDeviceModelStatus::kModelInstalledTooLate:
    case OnDeviceModelStatus::kNotReadyForUnknownReason:
    case OnDeviceModelStatus::kNoOnDeviceFeatureUsed:
    case OnDeviceModelStatus::kReady:
      // The model is downloaded but the installation is not completed yet.
      base::UmaHistogramEnumeration(
          base::StrCat({"OptimizationGuide.ModelExecution."
                        "OnDeviceModelToBeInstalledReason.",
                        GetVariantName(feature)}),
          on_device_model_status);
      return OnDeviceModelEligibilityReason::kModelToBeInstalled;
  }
}

void LogEligibilityReason(mojom::OnDeviceFeature feature,
                          OnDeviceModelEligibilityReason reason) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.",
           GetVariantName(feature)}),
      reason);
}

void RecordOnDeviceLoadModelResult(
    on_device_model::mojom::LoadModelResult result) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceBaseModelLoadResult", result);
}

}  // namespace

OnDeviceModelServiceController::OnDeviceModelServiceController(
    std::unique_ptr<OnDeviceModelAccessController> access_controller,
    base::SafeRef<PerformanceClassifier> performance_classifier,
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager,
    UsageTracker& usage_tracker,
    base::SafeRef<on_device_model::ServiceClient> service_client)
    : access_controller_(std::move(access_controller)),
      on_device_component_state_manager_(
          std::move(on_device_component_state_manager)),
      usage_tracker_(usage_tracker),
      service_client_(std::move(service_client)),
      safety_client_(service_client_->GetWeakPtr()),
      model_broker_impl_(
          *usage_tracker_,
          base::BindRepeating(
              &PerformanceClassifier::EnsurePerformanceClassAvailable,
              performance_classifier)) {
  base_model_controller_.emplace(weak_ptr_factory_.GetSafeRef(), nullptr);
  service_client_->set_on_disconnect_fn(base::BindRepeating(
      &OnDeviceModelServiceController::OnServiceDisconnected,
      weak_ptr_factory_.GetWeakPtr()));
  model_metadata_loader_.emplace(
      base::BindRepeating(&OnDeviceModelServiceController::UpdateModel,
                          weak_ptr_factory_.GetWeakPtr()),
      on_device_component_state_manager_);
}

OnDeviceModelServiceController::~OnDeviceModelServiceController() = default;

OnDeviceModelEligibilityReason OnDeviceModelServiceController::CanCreateSession(
    mojom::OnDeviceFeature feature) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::CanCreateSession", "feature",
              base::ToString(feature));
  // Ensure an initial solution is computed to avoid giving kUnknown error.
  UpdateSolutionProvider(feature);
  return model_broker_impl_.GetSolutionProvider(feature).solution().error_or(
      OnDeviceModelEligibilityReason::kSuccess);
}

std::unique_ptr<OnDeviceSession> OnDeviceModelServiceController::CreateSession(
    mojom::OnDeviceFeature feature,
    base::WeakPtr<OptimizationGuideLogger> logger,
    const SessionConfigParams& config_params) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::CreateSession", "feature",
              base::ToString(feature));
  // Ensure an initial solution is computed to avoid giving kUnknown error.
  UpdateSolutionProvider(feature);
  auto& maybe_solution =
      model_broker_impl_.GetSolutionProvider(feature).solution();
  auto reason =
      maybe_solution.error_or(OnDeviceModelEligibilityReason::kSuccess);
  LogEligibilityReason(feature, reason);

  usage_tracker_->OnDeviceEligibleFeatureUsed(feature);

  // Return if we cannot do anything more for right now.
  if (reason != OnDeviceModelEligibilityReason::kSuccess) {
    VLOG(1) << "Failed to create Session:" << reason;
    return nullptr;
  }

  return model_broker_impl_.GetSolutionProvider(feature)
      .local_subscriber()
      .client()
      ->CreateSession(config_params, logger);
}

void OnDeviceModelServiceController::SetLanguageDetectionModel(
    base::optional_ref<const ModelInfo> model_info) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::SetLanguageDetectionModel",
              "has_model", model_info.has_value());
  safety_client_.SetLanguageDetectionModel(model_info);
  UpdateSolutionProviders();
}

void OnDeviceModelServiceController::MaybeUpdateSafetyModel(
    std::unique_ptr<SafetyModelInfo> safety_model_info) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::MaybeUpdateSafetyModel",
              "has_model", !!safety_model_info);
  safety_client_.MaybeUpdateSafetyModel(std::move(safety_model_info));
  UpdateSolutionProviders();
}

void OnDeviceModelServiceController::UpdateModel(
    std::unique_ptr<OnDeviceModelMetadata> model_metadata) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::UpdateModel", "has_model",
              !!model_metadata);
  bool did_model_change =
      !model_metadata.get() != !base_model_controller_->model_metadata();
  base_model_controller_.emplace(weak_ptr_factory_.GetSafeRef(),
                                 std::move(model_metadata));

  if (did_model_change) {
    UpdateSolutionProviders();
  }
}

void OnDeviceModelServiceController::MaybeUpdateModelAdaptation(
    mojom::OnDeviceFeature feature,
    base::expected<OnDeviceModelAdaptationMetadata, AdaptationUnavailability>
        adaptation_metadata) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::MaybeUpdateModelAdaptation",
              "feature", base::ToString(feature), "has_model",
              adaptation_metadata.has_value());
  if (!adaptation_metadata_.MaybeUpdate(feature,
                                        std::move(adaptation_metadata))) {
    // Duplicate update (can be caused by multiple profiles).
    // Don't invalidate the existing controller.
    return;
  }
  base_model_controller_->EraseController(feature);
  UpdateSolutionProvider(feature);
}

void OnDeviceModelServiceController::OnServiceDisconnected(
    on_device_model::ServiceDisconnectReason reason) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::OnServiceDisconnected", "reason",
              reason);
  switch (reason) {
    case on_device_model::ServiceDisconnectReason::kGpuBlocked:
      access_controller_->OnGpuBlocked();
      UpdateSolutionProviders();
      break;
    // Below errors will be tracked by the related model disconnects, so they
    // are not handled specifically here.
    case on_device_model::ServiceDisconnectReason::kFailedToLoadLibrary:
    case on_device_model::ServiceDisconnectReason::kUnspecified:
      break;
  }
}

MaybeAdaptationMetadata& OnDeviceModelServiceController::GetFeatureMetadata(
    mojom::OnDeviceFeature feature) {
  return adaptation_metadata_.Get(feature);
}

proto::OnDeviceModelPerformanceHint
OnDeviceModelServiceController::GetPerformanceHint() {
  if (!base_model_controller_->model_metadata()) {
    return proto::OnDeviceModelPerformanceHint::
        ON_DEVICE_MODEL_PERFORMANCE_HINT_UNSPECIFIED;
  }

  return base_model_controller_->model_metadata()->performance_hint();
}

void OnDeviceModelServiceController::AddOnDeviceModelAvailabilityChangeObserver(
    mojom::OnDeviceFeature feature,
    OnDeviceModelAvailabilityObserver* observer) {
  model_broker_impl_.GetSolutionProvider(feature).AddObserver(observer);
}

void OnDeviceModelServiceController::
    RemoveOnDeviceModelAvailabilityChangeObserver(
        mojom::OnDeviceFeature feature,
        OnDeviceModelAvailabilityObserver* observer) {
  model_broker_impl_.GetSolutionProvider(feature).RemoveObserver(observer);
}

on_device_model::Capabilities
OnDeviceModelServiceController::GetCapabilities() {
  if (!base_model_controller_->model_metadata()) {
    return {};
  }
  return base_model_controller_->model_metadata()->capabilities();
}

OnDeviceModelServiceController::MaybeSolution
OnDeviceModelServiceController::GetSolution(mojom::OnDeviceFeature feature) {
  auto error =
      GetBaseModelError(feature, on_device_component_state_manager_.get());

  if (error != OnDeviceModelEligibilityReason::kModelToBeInstalled) {
    // Device eligibility not determined yet or device ineligible takes
    // precedence over feature usage.
    return base::unexpected(error);
  }

  // Checks usage for feature before checking (eligible) model status, so that
  // kPendingUsage is returned if the feature is not requested but the model was
  // available for a different feature.
  if (!usage_tracker_->WasOnDeviceEligibleFeatureRecentlyUsed(feature)) {
    return base::unexpected(
        OnDeviceModelEligibilityReason::kNoOnDeviceFeatureUsed);
  }

  if (!base_model_controller_->model_metadata()) {
    return base::unexpected(
        OnDeviceModelEligibilityReason::kModelToBeInstalled);
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
  // Check safety info.
  auto checker = safety_client_.MakeSafetyChecker(
      feature, metadata->adapter()->CanSkipTextSafety());
  if (!checker.has_value()) {
    return base::unexpected(checker.error());
  }

  auto reason = access_controller_->ShouldStartNewSession();
  if (reason != OnDeviceModelEligibilityReason::kSuccess) {
    return base::unexpected(reason);
  }

  return std::make_unique<Solution>(
      feature, metadata->adapter(),
      base_model_controller_->GetOrCreateFeatureController(feature, *metadata),
      std::move(checker.value()), weak_ptr_factory_.GetSafeRef());
}

void OnDeviceModelServiceController::UpdateSolutionProviders() {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::UpdateSolutionProviders");
  for (const auto& feature : model_broker_impl_.GetCapabilityKeys()) {
    UpdateSolutionProvider(feature);
  }
}

void OnDeviceModelServiceController::UpdateSolutionProvider(
    mojom::OnDeviceFeature feature) {
  // Note: This always constructs the Solution, even if the provider was not
  // constructed yet, to update supported_adaptation_ranks_ on the base model.
  model_broker_impl_.GetSolutionProvider(feature).Update(GetSolution(feature));
}

OnDeviceModelServiceController::BaseModelController::BaseModelController(
    base::SafeRef<OnDeviceModelServiceController> controller,
    std::unique_ptr<OnDeviceModelMetadata> model_metadata)
    : controller_(controller), model_metadata_(std::move(model_metadata)) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::BaseModelController::"
              "BaseModelController");
  supported_adaptation_ranks_ =
      features::GetOnDeviceModelAllowedAdaptationRanks();
  if (!model_metadata_ || !features::IsOnDeviceModelValidationEnabled()) {
    return;
  }

  // Check if the model needs validation, which may mark it pending validation,
  // blocking session creation.
  if (!access_controller().ShouldValidateModel(model_metadata_->version())) {
    return;
  }

  if (model_metadata_->validation_config().validation_prompts().empty()) {
    // Immediately succeed in validation if there are no prompts specified.
    access_controller().OnValidationFinished(
        OnDeviceModelValidationResult::kSuccess);
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BaseModelController::StartValidation,
                     weak_ptr_factory_.GetWeakPtr()),
      features::GetOnDeviceModelValidationDelay());
}

OnDeviceModelServiceController::BaseModelController::~BaseModelController() =
    default;

void OnDeviceModelServiceController::BaseModelController::RequireAdaptationRank(
    uint32_t required_rank) {
  if (required_rank == 0) {
    // Older configs may not specify rank, and should be covered by defaults.
    return;
  }
  if (base::Contains(supported_adaptation_ranks_, required_rank)) {
    return;
  }
  // Add the rank and reset all remotes to force a reload.
  supported_adaptation_ranks_.push_back(required_rank);
  remote_.reset();
  for (auto& kv : model_adaptation_controllers_) {
    kv.second.ResetRemote();
  }
}

base::WeakPtr<ModelController> OnDeviceModelServiceController::
    BaseModelController::GetOrCreateFeatureController(
        mojom::OnDeviceFeature feature,
        const OnDeviceModelAdaptationMetadata& metadata) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::BaseModelController::"
              "GetOrCreateFeatureController",
              "feature", base::ToString(feature));
  if (!metadata.asset_paths()) {
    has_direct_use_ = true;
    return weak_ptr_factory_.GetWeakPtr();
  }
  RequireAdaptationRank(metadata.adapter()->config().adaptation_rank());
  auto it = model_adaptation_controllers_.find(feature);
  if (it == model_adaptation_controllers_.end()) {
    it = model_adaptation_controllers_
             .emplace(std::piecewise_construct, std::forward_as_tuple(feature),
                      std::forward_as_tuple(feature, GetWeakPtr(),
                                            *metadata.asset_paths()))
             .first;
  }
  // Path should be equal.
  return it->second.GetWeakPtr();
}

void OnDeviceModelServiceController::BaseModelController::EraseController(
    mojom::OnDeviceFeature feature) {
  auto it = model_adaptation_controllers_.find(feature);
  if (it != model_adaptation_controllers_.end()) {
    model_adaptation_controllers_.erase(it);
  }
}

mojo::Remote<on_device_model::mojom::OnDeviceModel>&
OnDeviceModelServiceController::BaseModelController::GetOrCreateRemote() {
  if (remote_) {
    return remote_;
  }
  TRACE_EVENT(
      "optimization_guide",
      "OnDeviceModelServiceController::BaseModelController::CreateRemote");
  controller_->service_client_->AddPendingUsage();  // Warm up the service.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&on_device_model::LoadModelAssets, PopulateModelPaths()),
      base::BindOnce(
          [](base::WeakPtr<BaseModelController> self,
             mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel>
                 receiver,
             on_device_model::ModelAssets assets) {
            if (!self || !self->controller_->service_client_->is_bound()) {
              if (self) {
                self->controller_->service_client_->RemovePendingUsage();
              }
              CloseFilesInBackground(std::move(assets));
              return;
            }
            self->OnModelAssetsLoaded(std::move(receiver), std::move(assets));
          },
          weak_ptr_factory_.GetWeakPtr(),
          remote_.BindNewPipeAndPassReceiver()));
  remote_.set_disconnect_with_reason_handler(base::BindOnce(
      &BaseModelController::OnDisconnect, base::Unretained(this)));
  // By default the model will be reset immediately when idle. If a feature is
  // going using the base model, the idle handler will be set explicitly there.
  remote_.reset_on_idle_timeout(has_direct_use_
                                    ? features::GetOnDeviceModelIdleTimeout()
                                    : base::TimeDelta());
  base::UmaHistogramSparse(
      "OptimizationGuide.ModelExecution.OnDeviceBaseModelLoadVersion",
      base::HashMetricName(model_metadata_->version()));
  return remote_;
}

on_device_model::ModelAssetPaths
OnDeviceModelServiceController::BaseModelController::PopulateModelPaths() {
  on_device_model::ModelAssetPaths model_paths;
  model_paths.weights = model_metadata_->model_path().Append(kWeightsFile);

  // TODO(crbug.com/400998489): Cache files are experimental for now.
  if (model_metadata_->performance_hint() ==
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU) {
    model_paths.cache =
        model_metadata_->model_path().Append(kExperimentalCacheFile);
  }
  model_paths.encoder_cache =
      model_metadata_->model_path().Append(kEncoderCacheFile);
  model_paths.adapter_cache =
      model_metadata_->model_path().Append(kAdapterCacheFile);

  return model_paths;
}

void OnDeviceModelServiceController::BaseModelController::OnModelAssetsLoaded(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    on_device_model::ModelAssets assets) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::BaseModelController::"
              "OnModelAssetsLoaded");
  auto params = on_device_model::mojom::LoadModelParams::New();
  params->backend_type = ml::ModelBackendType::kGpuBackend;
  params->assets = std::move(assets);
  params->max_tokens = kOnDeviceModelMaxTokens;
  params->adaptation_ranks = supported_adaptation_ranks_;

  proto::OnDeviceModelPerformanceHint hint =
      model_metadata_->performance_hint();
  if (hint == proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU) {
    params->backend_type = ml::ModelBackendType::kCpuBackend;
  } else if (hint ==
             proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE) {
    params->performance_hint = ml::ModelPerformanceHint::kFastestInference;
  }
  controller_->service_client_->Get()->LoadModel(
      std::move(params), std::move(model),
      base::BindOnce(&RecordOnDeviceLoadModelResult));
  controller_->service_client_->RemovePendingUsage();
}

void OnDeviceModelServiceController::BaseModelController::OnDisconnect(
    uint32_t reason,
    const std::string& description) {
  TRACE_EVENT(
      "optimization_guide",
      "OnDeviceModelServiceController::BaseModelController::OnDisconnect");
  remote_.reset();
  const bool is_idle =
      reason == static_cast<uint32_t>(
                    on_device_model::ModelDisconnectReason::kIdleShutdown);
  base::UmaHistogramBoolean(
      "OptimizationGuide.ModelExecution.OnDeviceBaseModelIdleDisconnect",
      is_idle);
  if (is_idle) {
    return;
  }
  LOG(ERROR) << "Base model disconnected unexpectedly.";
  base::TimeDelta delay =
      access_controller().OnDisconnectedFromRemote() - base::Time::Now();
  if (delay.is_positive()) {
    // Notify providers that solutions are disabled.
    controller_->UpdateSolutionProviders();
    // Check again once the delay elapses.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OnDeviceModelServiceController::UpdateSolutionProviders,
                       controller_->GetWeakPtr()),
        delay);
  }
}

void OnDeviceModelServiceController::BaseModelController::StartValidation() {
  TRACE_EVENT(
      "optimization_guide",
      "OnDeviceModelServiceController::BaseModelController::StartValidation");
  mojo::Remote<on_device_model::mojom::Session> session;
  GetOrCreateRemote()->StartSession(session.BindNewPipeAndPassReceiver(),
                                    nullptr);
  model_validator_ = std::make_unique<OnDeviceModelValidator>(
      model_metadata_->validation_config(),
      base::BindOnce(&BaseModelController::FinishValidation,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(session));
}

void OnDeviceModelServiceController::BaseModelController::FinishValidation(
    OnDeviceModelValidationResult result) {
  TRACE_EVENT(
      "optimization_guide",
      "OnDeviceModelServiceController::BaseModelController::FinishValidation");
  DCHECK(model_validator_);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", result);
  model_validator_ = nullptr;
  access_controller().OnValidationFinished(result);
  controller_->UpdateSolutionProviders();
}

ModelController::ModelController() = default;
ModelController::~ModelController() = default;

OnDeviceModelServiceController::Solution::Solution(
    mojom::OnDeviceFeature feature,
    scoped_refptr<const OnDeviceModelFeatureAdapter> adapter,
    base::WeakPtr<ModelController> model_controller,
    std::unique_ptr<SafetyChecker> safety_checker,
    base::SafeRef<OnDeviceModelServiceController> controller)
    : feature_(feature),
      adapter_(std::move(adapter)),
      model_controller_(std::move(model_controller)),
      safety_checker_(std::move(safety_checker)),
      controller_(std::move(controller)) {}
OnDeviceModelServiceController::Solution::~Solution() = default;

bool OnDeviceModelServiceController::Solution::IsValid() const {
  return model_controller_ &&
         (!features::ShouldUseTextSafetyClassifierModel() ||
          adapter_->CanSkipTextSafety() || safety_checker_->client());
}

// Creates a config describing this solution;
mojom::ModelSolutionConfigPtr
OnDeviceModelServiceController::Solution::MakeConfig() const {
  auto config = mojom::ModelSolutionConfig::New();
  config->feature_config = mojo_base::ProtoWrapper(adapter_->config());
  config->model_versions = mojo_base::ProtoWrapper(
      GetModelVersions(*controller_->base_model_controller_->model_metadata(),
                       controller_->safety_client_,
                       controller_->GetFeatureMetadata(feature_)->version()));
  config->max_tokens = adapter_->GetTokenLimits().max_tokens;
  config->text_safety_config =
      mojo_base::ProtoWrapper(safety_checker_->safety_cfg().proto());
  return config;
}

void OnDeviceModelServiceController::Solution::CreateSession(
    mojo::PendingReceiver<on_device_model::mojom::Session> pending,
    on_device_model::mojom::SessionParamsPtr params) {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelServiceController::Solution::CreateSession");
  if (!model_controller_) {
    return;
  }
  model_controller_->GetOrCreateRemote()->StartSession(std::move(pending),
                                                       std::move(params));
}

void OnDeviceModelServiceController::Solution::CreateTextSafetySession(
    mojo::PendingReceiver<on_device_model::mojom::TextSafetySession> pending) {
  TRACE_EVENT(
      "optimization_guide",
      "OnDeviceModelServiceController::Solution::CreateTextSafetySession");
  base::WeakPtr<TextSafetyClient> client = safety_checker_->client();
  if (!client) {
    return;
  }
  client->StartSession(std::move(pending));
}

void OnDeviceModelServiceController::Solution::ReportHealthyCompletion() {
  TRACE_EVENT(
      "optimization_guide",
      "OnDeviceModelServiceController::Solution::ReportHealthyCompletion");
  controller_->access_controller_->OnResponseCompleted();
}

}  // namespace optimization_guide
