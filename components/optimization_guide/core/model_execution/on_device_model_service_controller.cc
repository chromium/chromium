// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
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
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
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
    ModelBasedCapabilityKey feature,
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
                        GetStringNameForModelExecutionFeature(feature)}),
          on_device_model_status);
      return OnDeviceModelEligibilityReason::kModelToBeInstalled;
  }
}

void LogEligibilityReason(ModelBasedCapabilityKey feature,
                          OnDeviceModelEligibilityReason reason) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.",
           GetStringNameForModelExecutionFeature(feature)}),
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
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager,
    on_device_model::ServiceClient::LaunchFn launch_fn)
    : access_controller_(std::move(access_controller)),
      on_device_component_state_manager_(
          std::move(on_device_component_state_manager)),
      service_client_(launch_fn),
      safety_client_(service_client_.GetWeakPtr()) {
  base_model_controller_.emplace(weak_ptr_factory_.GetSafeRef(), nullptr);
  service_client_.set_on_disconnect_fn(base::BindRepeating(
      &OnDeviceModelServiceController::OnServiceDisconnected,
      weak_ptr_factory_.GetWeakPtr()));
}

OnDeviceModelServiceController::~OnDeviceModelServiceController() = default;

void OnDeviceModelServiceController::Init() {
  model_metadata_loader_.emplace(
      base::BindRepeating(&OnDeviceModelServiceController::UpdateModel,
                          weak_ptr_factory_.GetWeakPtr()),
      on_device_component_state_manager_);
}

OnDeviceModelEligibilityReason OnDeviceModelServiceController::CanCreateSession(
    ModelBasedCapabilityKey feature) {
  return GetSolutionProvider(feature).solution().error_or(
      OnDeviceModelEligibilityReason::kSuccess);
}

std::unique_ptr<OptimizationGuideModelExecutor::Session>
OnDeviceModelServiceController::CreateSession(
    ModelBasedCapabilityKey feature,
    ExecuteRemoteFn execute_remote_fn,
    base::WeakPtr<OptimizationGuideLogger> optimization_guide_logger,
    const std::optional<SessionConfigParams>& config_params) {
  auto& solution = GetSolutionProvider(feature).solution();
  auto reason = solution.error_or(OnDeviceModelEligibilityReason::kSuccess);
  LogEligibilityReason(feature, reason);

  if (on_device_component_state_manager_) {
    on_device_component_state_manager_->OnDeviceEligibleFeatureUsed(feature);
  }

  // Return if we cannot do anything more for right now.
  if (reason != OnDeviceModelEligibilityReason::kSuccess) {
    VLOG(1) << "Failed to create Session:" << reason;
    return nullptr;
  }

  CHECK(base_model_controller_->model_metadata());
  CHECK(features::internal::GetOptimizationTargetForCapability(feature));
  auto* adaptation_metadata = GetFeatureMetadata(feature);
  CHECK(adaptation_metadata);

  OnDeviceOptions opts;
  opts.model_client = std::make_unique<OnDeviceModelClient>(
      feature, weak_ptr_factory_.GetWeakPtr(), solution->model_controller());
  opts.model_versions =
      GetModelVersions(*base_model_controller_->model_metadata(),
                       safety_client_, adaptation_metadata->version());
  opts.safety_checker =
      std::make_unique<SafetyChecker>(solution->safety_checker());
  opts.token_limits = solution->adapter()->GetTokenLimits();
  opts.adapter = solution->adapter();

  opts.logger = optimization_guide_logger;
  if (config_params) {
    opts.capabilities = config_params->capabilities;
    // TODO: can this be required?
    if (config_params->sampling_params) {
      opts.sampling_params = *config_params->sampling_params;
    }
  }

  return std::make_unique<SessionImpl>(
      feature, std::move(opts), std::move(execute_remote_fn), config_params);
}

void OnDeviceModelServiceController::SetLanguageDetectionModel(
    base::optional_ref<const ModelInfo> model_info) {
  safety_client_.SetLanguageDetectionModel(model_info);
  UpdateSolutionProviders();
}

void OnDeviceModelServiceController::MaybeUpdateSafetyModel(
    base::optional_ref<const ModelInfo> model_info) {
  safety_client_.MaybeUpdateSafetyModel(model_info);
  UpdateSolutionProviders();
}

void OnDeviceModelServiceController::UpdateModel(
    std::unique_ptr<OnDeviceModelMetadata> model_metadata) {
  bool did_model_change =
      !model_metadata.get() != !base_model_controller_->model_metadata();
  base_model_controller_.emplace(weak_ptr_factory_.GetSafeRef(),
                                 std::move(model_metadata));

  if (did_model_change) {
    UpdateSolutionProviders();
  }
}

void OnDeviceModelServiceController::MaybeUpdateModelAdaptation(
    ModelBasedCapabilityKey feature,
    std::unique_ptr<OnDeviceModelAdaptationMetadata> adaptation_metadata) {
  if (!adaptation_metadata) {
    model_adaptation_metadata_.erase(feature);
    base_model_controller_->EraseController(feature);
    UpdateSolutionProvider(feature);
    return;
  }
  auto it = model_adaptation_metadata_.find(feature);
  if (it != model_adaptation_metadata_.end() &&
      it->second == *adaptation_metadata) {
    // Duplicate update (can be caused by multiple profiles).
    // Don't invalidate the existing controller.
    return;
  }
  model_adaptation_metadata_.emplace(feature, *adaptation_metadata);
  base_model_controller_->EraseController(feature);
  UpdateSolutionProvider(feature);
}

void OnDeviceModelServiceController::OnServiceDisconnected(
    on_device_model::ServiceDisconnectReason reason) {
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

OnDeviceModelServiceController::OnDeviceModelClient::OnDeviceModelClient(
    ModelBasedCapabilityKey feature,
    base::WeakPtr<OnDeviceModelServiceController> controller,
    base::WeakPtr<ModelController> model_controller)
    : feature_(feature),
      controller_(std::move(controller)),
      model_controller_(std::move(model_controller)) {}

OnDeviceModelServiceController::OnDeviceModelClient::~OnDeviceModelClient() =
    default;

std::unique_ptr<OnDeviceOptions::Client>
OnDeviceModelServiceController::OnDeviceModelClient::Clone() const {
  return std::make_unique<OnDeviceModelServiceController::OnDeviceModelClient>(
      feature_, controller_, model_controller_);
}

bool OnDeviceModelServiceController::OnDeviceModelClient::ShouldUse() {
  return controller_ && model_controller_ &&
         controller_->access_controller_->ShouldStartNewSession() ==
             OnDeviceModelEligibilityReason::kSuccess;
}

void OnDeviceModelServiceController::OnDeviceModelClient::StartSession(
    mojo::PendingReceiver<on_device_model::mojom::Session> pending,
    on_device_model::mojom::SessionParamsPtr params) {
  model_controller_->GetOrCreateRemote()->StartSession(std::move(pending),
                                                       std::move(params));
}

void OnDeviceModelServiceController::OnDeviceModelClient::
    OnResponseCompleted() {
  if (controller_) {
    controller_->access_controller_->OnResponseCompleted();
  }
}

OnDeviceModelAdaptationMetadata*
OnDeviceModelServiceController::GetFeatureMetadata(
    ModelBasedCapabilityKey feature) {
  if (auto it = model_adaptation_metadata_.find(feature);
      it != model_adaptation_metadata_.end()) {
    return &it->second;
  }
  return nullptr;
}

void OnDeviceModelServiceController::AddOnDeviceModelAvailabilityChangeObserver(
    ModelBasedCapabilityKey feature,
    OnDeviceModelAvailabilityObserver* observer) {
  DCHECK(features::internal::GetOptimizationTargetForCapability(feature));
  GetSolutionProvider(feature).AddObserver(observer);
}

void OnDeviceModelServiceController::
    RemoveOnDeviceModelAvailabilityChangeObserver(
        ModelBasedCapabilityKey feature,
        OnDeviceModelAvailabilityObserver* observer) {
  DCHECK(features::internal::GetOptimizationTargetForCapability(feature));
  GetSolutionProvider(feature).RemoveObserver(observer);
}

on_device_model::Capabilities
OnDeviceModelServiceController::GetCapabilities() {
  if (!base_model_controller_->model_metadata()) {
    return {};
  }
  return base_model_controller_->model_metadata()->capabilities();
}

OnDeviceModelServiceController::MaybeSolution
OnDeviceModelServiceController::GetSolution(ModelBasedCapabilityKey feature) {
  if (!features::internal::GetOptimizationTargetForCapability(feature)) {
    return base::unexpected(
        OnDeviceModelEligibilityReason::kFeatureExecutionNotEnabled);
  }

  if (!base_model_controller_->model_metadata()) {
    return base::unexpected(
        GetBaseModelError(feature, on_device_component_state_manager_.get()));
  }

  // Check feature config.
  auto* metadata = GetFeatureMetadata(feature);
  if (!metadata) {
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

  return Solution(feature, metadata->adapter(),
                  base_model_controller_->GetOrCreateFeatureController(
                      feature, base::OptionalFromPtr(metadata->asset_paths())),
                  std::move(checker.value()), weak_ptr_factory_.GetSafeRef());
}

OnDeviceModelServiceController::SolutionProvider&
OnDeviceModelServiceController::GetSolutionProvider(
    ModelBasedCapabilityKey feature) {
  auto it = solution_providers_.find(feature);
  if (it == solution_providers_.end()) {
    it = solution_providers_
             .emplace(
                 std::piecewise_construct, std::forward_as_tuple(feature),
                 std::forward_as_tuple(feature, weak_ptr_factory_.GetSafeRef()))
             .first;
    it->second.Update(GetSolution(feature));
  }
  return it->second;
}

void OnDeviceModelServiceController::UpdateSolutionProviders() {
  for (const auto& entry : solution_providers_) {
    UpdateSolutionProvider(entry.first);
  }
}

void OnDeviceModelServiceController::UpdateSolutionProvider(
    ModelBasedCapabilityKey feature) {
  auto entry_it = solution_providers_.find(feature);
  if (entry_it == solution_providers_.end()) {
    return;
  }
  entry_it->second.Update(GetSolution(feature));
}

void OnDeviceModelServiceController::Subscribe(
    mojom::ModelSubscriptionOptionsPtr opts,
    mojo::PendingRemote<mojom::ModelSubscriber> subscriber) {
  EnsurePerformanceClassAvailable(base::BindOnce(
      &OnDeviceModelServiceController::SubscribeInternal,
      weak_ptr_factory_.GetWeakPtr(), std::move(opts), std::move(subscriber)));
}

void OnDeviceModelServiceController::SubscribeInternal(
    mojom::ModelSubscriptionOptionsPtr opts,
    mojo::PendingRemote<mojom::ModelSubscriber> subscriber) {
  auto feature = ToModelBasedCapabilityKey(opts->id);
  if (opts->mark_used && on_device_component_state_manager_) {
    on_device_component_state_manager_->OnDeviceEligibleFeatureUsed(feature);
  }
  GetSolutionProvider(feature).AddSubscriber(std::move(subscriber));
}

OnDeviceModelServiceController::BaseModelController::BaseModelController(
    base::SafeRef<OnDeviceModelServiceController> controller,
    std::unique_ptr<OnDeviceModelMetadata> model_metadata)
    : controller_(controller), model_metadata_(std::move(model_metadata)) {
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

base::WeakPtr<ModelController> OnDeviceModelServiceController::
    BaseModelController::GetOrCreateFeatureController(
        ModelBasedCapabilityKey feature,
        base::optional_ref<const on_device_model::AdaptationAssetPaths>
            adaptation_assets) {
  if (!adaptation_assets.has_value()) {
    has_direct_use_ = true;
    return weak_ptr_factory_.GetWeakPtr();
  }
  auto it = model_adaptation_controllers_.find(feature);
  if (it == model_adaptation_controllers_.end()) {
    it = model_adaptation_controllers_
             .emplace(std::piecewise_construct, std::forward_as_tuple(feature),
                      std::forward_as_tuple(feature, GetWeakPtr(),
                                            *adaptation_assets))
             .first;
  }
  // Path should be equal.
  return it->second.GetWeakPtr();
}

void OnDeviceModelServiceController::BaseModelController::EraseController(
    ModelBasedCapabilityKey feature) {
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
  controller_->service_client_.AddPendingUsage();  // Warm up the service.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&on_device_model::LoadModelAssets, PopulateModelPaths()),
      base::BindOnce(
          [](base::WeakPtr<BaseModelController> self,
             mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel>
                 receiver,
             on_device_model::ModelAssets assets) {
            if (!self || !self->controller_->service_client_.is_bound()) {
              if (self) {
                self->controller_->service_client_.RemovePendingUsage();
              }
              CloseFilesInBackground(std::move(assets));
              return;
            }
            self->OnModelAssetsLoaded(std::move(receiver), std::move(assets));
          },
          weak_ptr_factory_.GetWeakPtr(),
          remote_.BindNewPipeAndPassReceiver()));
  remote_.set_disconnect_handler(base::BindOnce(
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
  if (features::ForceCpuBackendForOnDeviceModel()) {
    model_paths.cache =
        model_metadata_->model_path().Append(kExperimentalCacheFile);
  }

  return model_paths;
}

void OnDeviceModelServiceController::BaseModelController::OnModelAssetsLoaded(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    on_device_model::ModelAssets assets) {
  auto params = on_device_model::mojom::LoadModelParams::New();
  params->backend_type = features::ForceCpuBackendForOnDeviceModel()
                             ? ml::ModelBackendType::kCpuBackend
                             : ml::ModelBackendType::kGpuBackend;
  params->assets = std::move(assets);
  // TODO(crbug.com/302402959): Choose max_tokens based on device.
  params->max_tokens = features::GetOnDeviceModelMaxTokens();
  params->adaptation_ranks = features::GetOnDeviceModelAllowedAdaptationRanks();
  if (controller_->on_device_component_state_manager_ &&
      controller_->on_device_component_state_manager_->IsLowTierDevice()) {
    params->performance_hint = ml::ModelPerformanceHint::kFastestInference;
  }
  controller_->service_client_.Get()->LoadModel(
      std::move(params), std::move(model),
      base::BindOnce(&RecordOnDeviceLoadModelResult));
  controller_->service_client_.RemovePendingUsage();
}

void OnDeviceModelServiceController::BaseModelController::OnDisconnect() {
  LOG(ERROR) << "Base model disconnected unexpectedly.";
  remote_.reset();
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
  DCHECK(model_validator_);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", result);
  model_validator_ = nullptr;
  access_controller().OnValidationFinished(result);
  controller_->UpdateSolutionProviders();
}

ModelController::ModelController() = default;
ModelController::~ModelController() = default;

OnDeviceModelServiceController::SolutionProvider::SolutionProvider(
    ModelBasedCapabilityKey feature,
    base::SafeRef<OnDeviceModelServiceController> controller)
    : feature_(feature), controller_(std::move(controller)) {}
OnDeviceModelServiceController::SolutionProvider::~SolutionProvider() = default;

void OnDeviceModelServiceController::SolutionProvider::AddSubscriber(
    mojo::PendingRemote<mojom::ModelSubscriber> pending) {
  auto id = subscribers_.Add(std::move(pending));
  UpdateSubscriber(*subscribers_.Get(id));
}
void OnDeviceModelServiceController::SolutionProvider::AddObserver(
    OnDeviceModelAvailabilityObserver* observer) {
  observers_.AddObserver(observer);
}
void OnDeviceModelServiceController::SolutionProvider::RemoveObserver(
    OnDeviceModelAvailabilityObserver* observer) {
  observers_.RemoveObserver(observer);
}

void OnDeviceModelServiceController::SolutionProvider::Update(
    MaybeSolution solution) {
  if (solution.has_value() && solution_.has_value() && solution_->IsValid()) {
    // Current solution is still valid, no need to update.
    return;
  }
  receivers_.Clear();
  solution_ = std::move(solution);
  UpdateSubscribers();
  UpdateObservers();
}

void OnDeviceModelServiceController::SolutionProvider::UpdateSubscribers() {
  for (auto& subscriber : subscribers_) {
    UpdateSubscriber(*subscriber);
  }
}

void OnDeviceModelServiceController::SolutionProvider::UpdateSubscriber(
    mojom::ModelSubscriber& subscriber) {
  if (!solution_.has_value()) {
    subscriber.Unavailable(
        *AvailabilityFromEligibilityReason(solution_.error()));
    return;
  }
  if (!solution_->IsValid()) {
    subscriber.Unavailable(mojom::ModelUnavailableReason::kPendingAssets);
    return;
  }
  auto config = solution_->MakeConfig();
  mojo::PendingRemote<mojom::ModelSolution> pending;
  receivers_.Add(&solution_.value(), pending.InitWithNewPipeAndPassReceiver());
  subscriber.Available(std::move(config), std::move(pending));
  return;
}

void OnDeviceModelServiceController::SolutionProvider::UpdateObservers() {
  for (auto& observer : observers_) {
    observer.OnDeviceModelAvailabilityChanged(
        feature_, solution_.error_or(OnDeviceModelEligibilityReason::kSuccess));
  }
}

OnDeviceModelServiceController::Solution::Solution(
    ModelBasedCapabilityKey feature,
    scoped_refptr<const OnDeviceModelFeatureAdapter> adapter,
    base::WeakPtr<ModelController> model_controller,
    std::unique_ptr<SafetyChecker> safety_checker,
    base::SafeRef<OnDeviceModelServiceController> controller)
    : feature_(feature),
      adapter_(std::move(adapter)),
      model_controller_(std::move(model_controller)),
      safety_checker_(std::move(safety_checker)),
      controller_(std::move(controller)) {}
OnDeviceModelServiceController::Solution::Solution(Solution&&) = default;
OnDeviceModelServiceController::Solution::~Solution() = default;
OnDeviceModelServiceController::Solution&
OnDeviceModelServiceController::Solution::operator=(Solution&&) = default;

bool OnDeviceModelServiceController::Solution::IsValid() {
  return model_controller_ &&
         (!features::ShouldUseTextSafetyClassifierModel() ||
          adapter_->CanSkipTextSafety() || safety_checker_->client());
}

// Creates a config describing this solution;
mojom::ModelSolutionConfigPtr
OnDeviceModelServiceController::Solution::MakeConfig() {
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
  if (!model_controller_) {
    return;
  }
  model_controller_->GetOrCreateRemote()->StartSession(std::move(pending),
                                                       std::move(params));
}

void OnDeviceModelServiceController::Solution::CreateTextSafetySession(
    mojo::PendingReceiver<on_device_model::mojom::TextSafetySession> pending) {
  base::WeakPtr<TextSafetyClient> client = safety_checker_->client();
  if (!client) {
    return;
  }
  client->StartSession(std::move(pending));
}

void OnDeviceModelServiceController::Solution::ReportHealthyCompletion() {
  controller_->access_controller_->OnResponseCompleted();
}

void OnDeviceModelServiceController::EnsurePerformanceClassAvailable(
    base::OnceClosure complete) {
  if (!on_device_component_state_manager_ ||
      !on_device_component_state_manager_->NeedsPerformanceClassUpdate()) {
    std::move(complete).Run();
    return;
  }

  if (performance_class_state_ == PerformanceClassState::kComplete) {
    std::move(complete).Run();
    return;
  }

  // Use unsafe because cancellation isn't needed.
  performance_class_callbacks_.AddUnsafe(std::move(complete));

  if (performance_class_state_ == PerformanceClassState::kComputing) {
    return;
  }

  performance_class_state_ = PerformanceClassState::kComputing;
  service_client_.Get()->GetEstimatedPerformanceClass(
      base::BindOnce(&ConvertToOnDeviceModelPerformanceClass)
          .Then(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              base::BindOnce(
                  &OnDeviceModelServiceController::PerformanceClassUpdated,
                  base::RetainedRef(this)),
              OnDeviceModelPerformanceClass::kServiceCrash)));
}

void OnDeviceModelServiceController::PerformanceClassUpdated(
    OnDeviceModelPerformanceClass perf_class) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelPerformanceClass",
      perf_class);
  RegisterPerformanceClassSyntheticTrial(perf_class);

  auto complete = base::BindOnce(
      [](scoped_refptr<OnDeviceModelServiceController> controller) {
        controller->performance_class_state_ = PerformanceClassState::kComplete;
        controller->performance_class_callbacks_.Notify();
      },
      base::RetainedRef(this));
  if (on_device_component_state_manager_) {
    on_device_component_state_manager_->DevicePerformanceClassChanged(
        std::move(complete), perf_class);
  } else {
    std::move(complete).Run();
  }
}

}  // namespace optimization_guide
