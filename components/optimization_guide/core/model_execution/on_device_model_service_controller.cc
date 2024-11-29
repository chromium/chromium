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
#include "base/metrics/histogram_functions.h"
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
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/receiver.h"
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

}  // namespace

OnDeviceModelServiceController::OnDeviceModelServiceController(
    std::unique_ptr<OnDeviceModelAccessController> access_controller,
    base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
        on_device_component_state_manager,
    on_device_model::ServiceClient::LaunchFn launch_fn)
    : access_controller_(std::move(access_controller)),
      on_device_component_state_manager_(
          std::move(on_device_component_state_manager)),
      service_client_(launch_fn),
      safety_client_(service_client_.GetWeakPtr()) {
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
  if (!features::internal::GetOptimizationTargetForCapability(feature)) {
    return OnDeviceModelEligibilityReason::kFeatureExecutionNotEnabled;
  }

  if (!model_metadata_) {
    if (!on_device_component_state_manager_) {
      return OnDeviceModelEligibilityReason::kModelNotEligible;
    }

    switch (on_device_component_state_manager_->GetOnDeviceModelStatus()) {
      case optimization_guide::OnDeviceModelStatus::kNotEligible:
        return OnDeviceModelEligibilityReason::kModelNotEligible;
      case optimization_guide::OnDeviceModelStatus::kInsufficientDiskSpace:
        return OnDeviceModelEligibilityReason::kInsufficientDiskSpace;
      case optimization_guide::OnDeviceModelStatus::kInstallNotComplete:
      case optimization_guide::OnDeviceModelStatus::
          kModelInstallerNotRegisteredForUnknownReason:
      case optimization_guide::OnDeviceModelStatus::kModelInstalledTooLate:
      case optimization_guide::OnDeviceModelStatus::kNotReadyForUnknownReason:
      case optimization_guide::OnDeviceModelStatus::kNoOnDeviceFeatureUsed:
        return OnDeviceModelEligibilityReason::kModelToBeInstalled;
      case optimization_guide::OnDeviceModelStatus::kReady:
        // The model is downloaded but the installation is not completed yet.
        return OnDeviceModelEligibilityReason::kModelToBeInstalled;
    }
  }

  // Check feature config.
  auto adapter = GetFeatureAdapter(feature);
  if (!adapter) {
    return OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature;
  }
  // Check safety info.
  auto checker =
      safety_client_.MakeSafetyChecker(feature, adapter->CanSkipTextSafety());
  if (!checker.has_value()) {
    return checker.error();
  }

  return access_controller_->ShouldStartNewSession();
}

std::unique_ptr<OptimizationGuideModelExecutor::Session>
OnDeviceModelServiceController::CreateSession(
    ModelBasedCapabilityKey feature,
    ExecuteRemoteFn execute_remote_fn,
    base::WeakPtr<OptimizationGuideLogger> optimization_guide_logger,
    base::WeakPtr<ModelQualityLogsUploaderService>
        model_quality_uploader_service,
    const std::optional<SessionConfigParams>& config_params) {
  OnDeviceModelEligibilityReason reason = CanCreateSession(feature);
  CHECK_NE(reason, OnDeviceModelEligibilityReason::kUnknown);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.",
           GetStringNameForModelExecutionFeature(feature)}),
      reason);

  if (on_device_component_state_manager_) {
    on_device_component_state_manager_->OnDeviceEligibleFeatureUsed(feature);
  }

  // Return if we cannot do anything more for right now.
  if (reason != OnDeviceModelEligibilityReason::kSuccess) {
    return nullptr;
  }

  CHECK(model_metadata_);
  on_device_model::ModelAssetPaths model_paths = PopulateModelPaths();

  auto adapter = GetFeatureAdapter(feature);
  CHECK(adapter);

  std::optional<on_device_model::AdaptationAssetPaths> adaptation_assets;
  std::optional<int64_t> adaptation_version;
  auto adaptation_metadata_it = model_adaptation_metadata_.find(feature);
  if (adaptation_metadata_it != model_adaptation_metadata_.end()) {
    CHECK(features::internal::GetOptimizationTargetForCapability(feature));
    adaptation_assets =
        base::OptionalFromPtr(adaptation_metadata_it->second.asset_paths());
    adaptation_version = adaptation_metadata_it->second.version();
  }

  SessionImpl::OnDeviceOptions opts;
  opts.model_client = std::make_unique<OnDeviceModelClient>(
      feature, weak_ptr_factory_.GetWeakPtr(), model_paths, adaptation_assets);
  opts.model_versions =
      GetModelVersions(*model_metadata_, safety_client_, adaptation_version);
  opts.safety_checker = std::move(
      safety_client_.MakeSafetyChecker(feature, adapter->CanSkipTextSafety())
          .value());
  opts.token_limits = adapter->GetTokenLimits();
  opts.adapter = std::move(adapter);

  base::WeakPtr<ModelQualityLogsUploaderService> log_uploader =
      (config_params && config_params->logging_mode ==
                            SessionConfigParams::LoggingMode::kAlwaysDisable
           ? nullptr
           : model_quality_uploader_service);

  has_started_session_ = true;
  return std::make_unique<SessionImpl>(
      feature, std::move(opts), std::move(execute_remote_fn),
      optimization_guide_logger, log_uploader, config_params);
}

// static
void OnDeviceModelServiceController::GetEstimatedPerformanceClass(
    scoped_refptr<OnDeviceModelServiceController> controller,
    base::OnceCallback<void(OnDeviceModelPerformanceClass)> callback) {
  auto* raw_controller = controller.get();
  raw_controller->service_client_.Get()->GetEstimatedPerformanceClass(
      base::BindOnce(&ConvertToOnDeviceModelPerformanceClass)
          .Then(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback),
              OnDeviceModelPerformanceClass::kServiceCrash))
          .Then(base::OnceClosure(
              base::DoNothingWithBoundArgs(std::move(controller)))));
}

mojo::Remote<on_device_model::mojom::OnDeviceModel>&
OnDeviceModelServiceController::GetOrCreateModelRemote(
    ModelBasedCapabilityKey feature,
    const on_device_model::ModelAssetPaths& model_paths,
    base::optional_ref<const on_device_model::AdaptationAssetPaths>
        adaptation_assets) {
  MaybeCreateBaseModelRemote(model_paths);
  if (!adaptation_assets.has_value()) {
    // The base model is being used by a feature directly, so set the idle
    // handler.
    base_model_remote_.set_idle_handler(
        features::GetOnDeviceModelIdleTimeout(),
        base::BindRepeating(
            &OnDeviceModelServiceController::OnBaseModelRemoteIdle,
            base::Unretained(this)));
    return base_model_remote_;
  }
  auto it = model_adaptation_controllers_.find(feature);
  if (it == model_adaptation_controllers_.end()) {
    it = model_adaptation_controllers_
             .emplace(
                 std::piecewise_construct, std::forward_as_tuple(feature),
                 std::forward_as_tuple(feature, weak_ptr_factory_.GetWeakPtr()))
             .first;
  }
  return it->second.GetOrCreateModelRemote(*adaptation_assets);
}

void OnDeviceModelServiceController::MaybeCreateBaseModelRemote(
    const on_device_model::ModelAssetPaths& model_paths) {
  if (base_model_remote_) {
    return;
  }
  service_client_.AddPendingUsage();  // Warm up the service.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&on_device_model::LoadModelAssets, model_paths),
      base::BindOnce(&OnDeviceModelServiceController::OnModelAssetsLoaded,
                     base_model_scoped_weak_ptr_factory_.GetWeakPtr(),
                     base_model_remote_.BindNewPipeAndPassReceiver()));
  base_model_remote_.set_disconnect_handler(
      base::BindOnce(&OnDeviceModelServiceController::OnBaseModelDisconnected,
                     base::Unretained(this)));
  // By default the model will be reset immediately when idle. If a feature is
  // going using the base model, the idle handler will be set explicitly there.
  base_model_remote_.reset_on_idle_timeout(base::TimeDelta());
}

void OnDeviceModelServiceController::OnModelAssetsLoaded(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    on_device_model::ModelAssets assets) {
  if (!service_client_.is_bound()) {
    // Close the files on a background thread.
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::DoNothingWithBoundArgs(std::move(assets)));
    return;
  }
  auto params = on_device_model::mojom::LoadModelParams::New();
  params->assets = std::move(assets);
  // TODO(crbug.com/302402959): Choose max_tokens based on device.
  params->max_tokens = features::GetOnDeviceModelMaxTokens();
  params->adaptation_ranks = features::GetOnDeviceModelAllowedAdaptationRanks();
  service_client_.Get()->LoadModel(
      std::move(params), std::move(model),
      base::DoNothingAs<void(on_device_model::mojom::LoadModelResult)>());
  service_client_.RemovePendingUsage();
}

void OnDeviceModelServiceController::SetLanguageDetectionModel(
    base::optional_ref<const ModelInfo> model_info) {
  safety_client_.SetLanguageDetectionModel(model_info);
}

void OnDeviceModelServiceController::MaybeUpdateSafetyModel(
    base::optional_ref<const ModelInfo> model_info) {
  safety_client_.MaybeUpdateSafetyModel(model_info);
}

on_device_model::ModelAssetPaths
OnDeviceModelServiceController::PopulateModelPaths() {
  on_device_model::ModelAssetPaths model_paths;
  model_paths.weights = model_metadata_->model_path().Append(kWeightsFile);
  return model_paths;
}

void OnDeviceModelServiceController::UpdateModel(
    std::unique_ptr<OnDeviceModelMetadata> model_metadata) {
  bool did_model_change = !model_metadata.get() != !model_metadata_.get();
  model_adaptation_controllers_.clear();
  base_model_remote_.reset();
  base_model_scoped_weak_ptr_factory_.InvalidateWeakPtrs();
  model_metadata_ = std::move(model_metadata);
  has_started_session_ = false;
  model_validator_ = nullptr;

  if (did_model_change) {
    for (const auto& entry : model_availability_change_observers_) {
      NotifyModelAvailabilityChange(entry.first);
    }
  }

  if (!model_metadata_ || !features::IsOnDeviceModelValidationEnabled()) {
    return;
  }

  if (model_metadata_->validation_config().validation_prompts().empty()) {
    // Immediately succeed in validation if there are no prompts specified.
    if (access_controller_->ShouldValidateModel(model_metadata_->version())) {
      access_controller_->OnValidationFinished(
          OnDeviceModelValidationResult::kSuccess);
    }
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceModelServiceController::StartValidation,
                     base_model_scoped_weak_ptr_factory_.GetWeakPtr()),
      features::GetOnDeviceModelValidationDelay());
}

void OnDeviceModelServiceController::StartValidation() {
  // Skip validation if a session has started to avoid interrupting.
  if (has_started_session_) {
    return;
  }

  if (!access_controller_->ShouldValidateModel(model_metadata_->version())) {
    return;
  }

  MaybeCreateBaseModelRemote(PopulateModelPaths());
  mojo::Remote<on_device_model::mojom::Session> session;
  base_model_remote_->StartSession(session.BindNewPipeAndPassReceiver());
  model_validator_ = std::make_unique<OnDeviceModelValidator>(
      model_metadata_->validation_config(),
      base::BindOnce(&OnDeviceModelServiceController::FinishValidation,
                     GetWeakPtr()),
      std::move(session));
}

void OnDeviceModelServiceController::FinishValidation(
    OnDeviceModelValidationResult result) {
  if (!model_validator_) {
    return;
  }

  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", result);

  model_validator_ = nullptr;
  access_controller_->OnValidationFinished(result);
}

void OnDeviceModelServiceController::MaybeUpdateModelAdaptation(
    ModelBasedCapabilityKey feature,
    std::unique_ptr<OnDeviceModelAdaptationMetadata> adaptation_metadata) {
  if (!adaptation_metadata) {
    model_adaptation_metadata_.erase(feature);
  } else {
    model_adaptation_metadata_.emplace(feature, *adaptation_metadata);
  }
  auto it = model_adaptation_controllers_.find(feature);
  if (it != model_adaptation_controllers_.end()) {
    model_adaptation_controllers_.erase(it);
  }
  NotifyModelAvailabilityChange(feature);
}

void OnDeviceModelServiceController::OnServiceDisconnected(
    on_device_model::ServiceDisconnectReason reason) {
  switch (reason) {
    case on_device_model::ServiceDisconnectReason::kGpuBlocked:
      access_controller_->OnGpuBlocked();
      break;
    // Below errors will be tracked by the related model disconnects, so they
    // are not handled specifically here.
    case on_device_model::ServiceDisconnectReason::kFailedToLoadLibrary:
    case on_device_model::ServiceDisconnectReason::kUnspecified:
      break;
  }
}

void OnDeviceModelServiceController::OnBaseModelDisconnected() {
  LOG(ERROR) << "Base model disconnected unexpectedly.";
  // This could be either a true crash or just a failure to load the model,
  // but we handle it the same way in either case.
  // Explicitly reset to adaptations remotes to avoid receiving additional
  // disconnect errors (though they may have already received them).
  model_adaptation_controllers_.clear();
  base_model_remote_.reset();
  access_controller_->OnDisconnectedFromRemote();
  FinishValidation(OnDeviceModelValidationResult::kServiceCrash);
}

void OnDeviceModelServiceController::OnBaseModelRemoteIdle() {
  // Adaptations should all be disconnected already if this is idle, but we
  // reset the explicitly anyway.
  model_adaptation_controllers_.clear();
  base_model_remote_.reset();
}

void OnDeviceModelServiceController::OnModelAdaptationRemoteDisconnected() {
  LOG(ERROR) << "Model adaptation disconnected unexpectedly.";
  // In the event of a service crash, we expect that OnBaseModelDisconnected
  // will usually be called first, and prevent this from firing, otherwise this
  // may double count the crash.
  // TODO: crbug.com/376063340 - Consider tracking these separately and not
  // suppressing the disconnect errors.
  access_controller_->OnDisconnectedFromRemote();
}

OnDeviceModelServiceController::OnDeviceModelClient::OnDeviceModelClient(
    ModelBasedCapabilityKey feature,
    base::WeakPtr<OnDeviceModelServiceController> controller,
    const on_device_model::ModelAssetPaths& model_paths,
    base::optional_ref<const on_device_model::AdaptationAssetPaths>
        adaptation_assets)
    : feature_(feature),
      controller_(controller),
      model_paths_(model_paths),
      adaptation_assets_(adaptation_assets.CopyAsOptional()) {}

OnDeviceModelServiceController::OnDeviceModelClient::~OnDeviceModelClient() =
    default;

bool OnDeviceModelServiceController::OnDeviceModelClient::ShouldUse() {
  return controller_ &&
         controller_->access_controller_->ShouldStartNewSession() ==
             OnDeviceModelEligibilityReason::kSuccess;
}

mojo::Remote<on_device_model::mojom::OnDeviceModel>&
OnDeviceModelServiceController::OnDeviceModelClient::GetModelRemote() {
  return controller_->GetOrCreateModelRemote(feature_, model_paths_,
                                             adaptation_assets_);
}

void OnDeviceModelServiceController::OnDeviceModelClient::
    OnResponseCompleted() {
  if (controller_) {
    controller_->access_controller_->OnResponseCompleted();
  }
}

scoped_refptr<const OnDeviceModelFeatureAdapter>
OnDeviceModelServiceController::GetFeatureAdapter(
    ModelBasedCapabilityKey feature) {
  // Take the feature config from adaptation model metadata or base model
  // metadata.
  auto adaptation_metadata_it = model_adaptation_metadata_.find(feature);
  if (adaptation_metadata_it != model_adaptation_metadata_.end() &&
      adaptation_metadata_it->second.adapter()) {
    return adaptation_metadata_it->second.adapter();
  }
  return model_metadata_->GetAdapter(ToModelExecutionFeatureProto(feature));
}

void OnDeviceModelServiceController::AddOnDeviceModelAvailabilityChangeObserver(
    ModelBasedCapabilityKey feature,
    OnDeviceModelAvailabilityObserver* observer) {
  DCHECK(features::internal::GetOptimizationTargetForCapability(feature));
  model_availability_change_observers_[feature].AddObserver(observer);
}

void OnDeviceModelServiceController::
    RemoveOnDeviceModelAvailabilityChangeObserver(
        ModelBasedCapabilityKey feature,
        OnDeviceModelAvailabilityObserver* observer) {
  DCHECK(features::internal::GetOptimizationTargetForCapability(feature));
  model_availability_change_observers_[feature].RemoveObserver(observer);
}

void OnDeviceModelServiceController::NotifyModelAvailabilityChange(
    ModelBasedCapabilityKey feature) {
  auto entry_it = model_availability_change_observers_.find(feature);
  if (entry_it == model_availability_change_observers_.end()) {
    return;
  }
  auto can_create_session = CanCreateSession(feature);
  for (auto& observer : entry_it->second) {
    observer.OnDeviceModelAvailabilityChanged(feature, can_create_session);
  }
}

}  // namespace optimization_guide
