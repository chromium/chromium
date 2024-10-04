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
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
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
#include "services/on_device_model/public/cpp/text_safety_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

namespace {

proto::OnDeviceModelVersions GetModelVersions(
    const OnDeviceModelMetadata& model_metadata,
    const SafetyModelInfo* safety_model_info,
    std::optional<int64_t> adaptation_version) {
  proto::OnDeviceModelVersions versions;
  auto* on_device_model_version =
      versions.mutable_on_device_model_service_version();
  on_device_model_version->set_component_version(model_metadata.version());
  on_device_model_version->mutable_on_device_base_model_metadata()
      ->set_base_model_name(model_metadata.model_spec().model_name);
  on_device_model_version->mutable_on_device_base_model_metadata()
      ->set_base_model_version(model_metadata.model_spec().model_version);

  if (safety_model_info) {
    versions.set_text_safety_model_version(safety_model_info->GetVersion());
  }

  if (adaptation_version) {
    on_device_model_version->set_model_adaptation_version(*adaptation_version);
  }

  return versions;
}

const auto& GetTokenLimits() {
  // TODO(b/302402959): Choose max_tokens based on device.
  static const TokenLimits token_limits = []() {
    auto context =
        static_cast<uint32_t>(features::GetOnDeviceModelMaxTokensForContext());
    auto execute =
        static_cast<uint32_t>(features::GetOnDeviceModelMaxTokensForExecute());
    auto output =
        static_cast<uint32_t>(features::GetOnDeviceModelMaxTokensForOutput());
    return TokenLimits{
        .max_tokens = (context + execute + output),
        .max_context_tokens = context,
        .max_execute_tokens = execute,
        .max_output_tokens = output,
    };
  }();
  return token_limits;
}

}  // namespace

OnDeviceModelServiceController::OnDeviceModelServiceController(
    std::unique_ptr<OnDeviceModelAccessController> access_controller,
    base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
        on_device_component_state_manager)
    : access_controller_(std::move(access_controller)),
      on_device_component_state_manager_(
          std::move(on_device_component_state_manager)) {}

OnDeviceModelServiceController::~OnDeviceModelServiceController() = default;

void OnDeviceModelServiceController::Init() {
  model_metadata_loader_.emplace(
      base::BindRepeating(&OnDeviceModelServiceController::UpdateModel,
                          weak_ptr_factory_.GetWeakPtr()),
      on_device_component_state_manager_);
}

OnDeviceModelEligibilityReason OnDeviceModelServiceController::CanCreateSession(
    ModelBasedCapabilityKey feature) {
  if (!features::internal::IsOnDeviceModelEnabled(feature)) {
    return OnDeviceModelEligibilityReason::kFeatureExecutionNotEnabled;
  }

  if (!model_metadata_) {
    if (!on_device_component_state_manager_) {
      return OnDeviceModelEligibilityReason::kModelNotAvailable;
    }

    switch (on_device_component_state_manager_->GetOnDeviceModelStatus()) {
      case optimization_guide::OnDeviceModelStatus::kNotEligible:
        return OnDeviceModelEligibilityReason::kModelNotAvailable;
      case optimization_guide::OnDeviceModelStatus::kInstallNotComplete:
      case optimization_guide::OnDeviceModelStatus::
          kModelInstallerNotRegisteredForUnknownReason:
      case optimization_guide::OnDeviceModelStatus::kModelInstalledTooLate:
      case optimization_guide::OnDeviceModelStatus::kNotReadyForUnknownReason:
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
  if (features::ShouldUseTextSafetyClassifierModel() &&
      !adapter->CanSkipTextSafety()) {
    if (!safety_model_info_) {
      return OnDeviceModelEligibilityReason::kSafetyModelNotAvailable;
    }

    std::optional<proto::FeatureTextSafetyConfiguration> safety_config =
        safety_model_info_->GetConfig(ToModelExecutionFeatureProto(feature));
    if (!safety_config) {
      return OnDeviceModelEligibilityReason::
          kSafetyConfigNotAvailableForFeature;
    }

    if (!safety_config->allowed_languages().empty() &&
        !language_detection_model_path_) {
      return OnDeviceModelEligibilityReason::
          kLanguageDetectionModelNotAvailable;
    }
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

  auto ts_params = PopulateTextSafetyParams();
  std::optional<proto::FeatureTextSafetyConfiguration> safety_config;
  if (features::ShouldUseTextSafetyClassifierModel() &&
      !adapter->CanSkipTextSafety()) {
    CHECK(safety_model_info_);
    safety_config =
        safety_model_info_->GetConfig(ToModelExecutionFeatureProto(feature));
    CHECK(safety_config);
    CHECK(ts_params.ts_paths);
    if (!safety_config->allowed_languages().empty()) {
      CHECK(ts_params.language_paths);
    }
  }

  std::optional<on_device_model::AdaptationAssetPaths> adaptation_assets;
  std::optional<int64_t> adaptation_version;
  auto adaptation_metadata_it = model_adaptation_metadata_.find(feature);
  if (adaptation_metadata_it != model_adaptation_metadata_.end()) {
    CHECK(features::internal::IsOnDeviceModelAdaptationEnabled(feature));
    adaptation_assets =
        base::OptionalFromPtr(adaptation_metadata_it->second.asset_paths());
    adaptation_version = adaptation_metadata_it->second.version();
  }

  SessionImpl::OnDeviceOptions opts;
  opts.model_client = std::make_unique<OnDeviceModelClient>(
      feature, weak_ptr_factory_.GetWeakPtr(), ts_params, model_paths,
      adaptation_assets);
  opts.model_versions = GetModelVersions(
      *model_metadata_, safety_model_info_.get(), adaptation_version);
  opts.adapter = std::move(adapter);
  opts.safety_checker =
      std::make_unique<SafetyChecker>(SafetyConfig(safety_config));
  opts.token_limits = GetTokenLimits();

  has_started_session_ = true;
  return std::make_unique<SessionImpl>(
      feature, std::move(opts), std::move(execute_remote_fn),
      optimization_guide_logger, model_quality_uploader_service, config_params);
}

void OnDeviceModelServiceController::GetEstimatedPerformanceClass(
    GetEstimatedPerformanceClassCallback callback) {
  LaunchService();
  service_remote_->GetEstimatedPerformanceClass(base::BindOnce(
      [](GetEstimatedPerformanceClassCallback callback,
         on_device_model::mojom::PerformanceClass performance_class) {
        std::move(callback).Run(performance_class);
      },
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                  std::nullopt)));
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

mojo::Remote<on_device_model::mojom::TextSafetyModel>&
OnDeviceModelServiceController::GetTextSafetyModelRemote(
    const on_device_model::TextSafetyLoaderParams& params) {
  if (ts_model_remote_) {
    return ts_model_remote_;
  }
  LaunchService();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&on_device_model::LoadTextSafetyParams, params),
      base::BindOnce(&OnDeviceModelServiceController::OnTextSafetyParamsLoaded,
                     weak_ptr_factory_.GetWeakPtr(),
                     ts_model_remote_.BindNewPipeAndPassReceiver()));
  ts_model_remote_.reset_on_disconnect();  // Maybe track disconnects?
  ts_model_remote_.reset_on_idle_timeout(
      features::GetOnDeviceModelIdleTimeout());
  return ts_model_remote_;
}

void OnDeviceModelServiceController::OnTextSafetyParamsLoaded(
    mojo::PendingReceiver<on_device_model::mojom::TextSafetyModel> model,
    on_device_model::mojom::TextSafetyModelParamsPtr params) {
  LaunchService();
  service_remote_->LoadTextSafetyModel(std::move(params), std::move(model));
}

void OnDeviceModelServiceController::MaybeCreateBaseModelRemote(
    const on_device_model::ModelAssetPaths& model_paths) {
  if (base_model_remote_) {
    return;
  }
  LaunchService();
  // We want the service to start while loading the model assets, so set a
  // longish idle timeout to make sure it doesn't get shut down.
  service_remote_.reset_on_idle_timeout(base::Minutes(1));
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
  if (!service_remote_) {
    // Close the files on a background thread.
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::DoNothingWithBoundArgs(std::move(assets)));
    return;
  }
  auto params = on_device_model::mojom::LoadModelParams::New();
  params->assets = std::move(assets);
  params->max_tokens = GetTokenLimits().max_tokens;
  params->adaptation_ranks = features::GetOnDeviceModelAllowedAdaptationRanks();
  service_remote_->LoadModel(
      std::move(params), std::move(model),
      base::BindOnce(&OnDeviceModelServiceController::OnLoadModelResult,
                     weak_ptr_factory_.GetWeakPtr()));
  // Now that the model has been loaded, the idle timeout is no longer needed.
  service_remote_.reset_on_idle_timeout(base::TimeDelta());
}

void OnDeviceModelServiceController::SetLanguageDetectionModel(
    base::optional_ref<const ModelInfo> model_info) {
  if (!model_info.has_value()) {
    language_detection_model_path_.reset();
    return;
  }
  ts_model_remote_.reset();  // The remote's assets are outdated.
  language_detection_model_path_ = model_info->GetModelFilePath();
}

void OnDeviceModelServiceController::MaybeUpdateSafetyModel(
    base::optional_ref<const ModelInfo> model_info) {
  auto new_info = SafetyModelInfo::Load(model_info);
  if (!new_info) {
    safety_model_info_.reset();
    return;
  }
  ts_model_remote_.reset();  // The remote's assets are outdated.
  safety_model_info_ = std::move(new_info);
}

on_device_model::ModelAssetPaths
OnDeviceModelServiceController::PopulateModelPaths() {
  on_device_model::ModelAssetPaths model_paths;
  model_paths.weights = model_metadata_->model_path().Append(kWeightsFile);
  return model_paths;
}

on_device_model::TextSafetyLoaderParams
OnDeviceModelServiceController::PopulateTextSafetyParams() const {
  on_device_model::TextSafetyLoaderParams params;
  // Populate the model paths even if they are not needed for the current
  // feature, since the base model remote could be used for subsequent features.
  if (safety_model_info_) {
    params.ts_paths.emplace();
    params.ts_paths->data = safety_model_info_->GetDataPath();
    params.ts_paths->sp_model = safety_model_info_->GetSpModelPath();
  }
  if (language_detection_model_path_) {
    params.language_paths.emplace();
    params.language_paths->model = *language_detection_model_path_;
  }
  return params;
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

void OnDeviceModelServiceController::OnLoadModelResult(
    on_device_model::mojom::LoadModelResult result) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelLoadResult",
      ConvertToOnDeviceModelLoadResult(result));
  switch (result) {
    case on_device_model::mojom::LoadModelResult::kGpuBlocked:
      access_controller_->OnGpuBlocked();
      model_adaptation_controllers_.clear();
      base_model_remote_.reset();
      break;
    case on_device_model::mojom::LoadModelResult::kSuccess:
      break;
    case on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary:
      break;
  }
}

void OnDeviceModelServiceController::OnBaseModelDisconnected() {
  model_adaptation_controllers_.clear();
  base_model_remote_.reset();
  access_controller_->OnDisconnectedFromRemote();
  FinishValidation(OnDeviceModelValidationResult::kServiceCrash);
}

void OnDeviceModelServiceController::OnBaseModelRemoteIdle() {
  model_adaptation_controllers_.clear();
  base_model_remote_.reset();
}

void OnDeviceModelServiceController::OnModelAdaptationRemoteDisconnected(
    ModelBasedCapabilityKey feature,
    ModelRemoteDisconnectReason reason) {
  switch (reason) {
    case ModelRemoteDisconnectReason::kGpuBlocked:
      access_controller_->OnGpuBlocked();
      break;
    case ModelRemoteDisconnectReason::kDisconncted:
      access_controller_->OnDisconnectedFromRemote();
      break;
    case ModelRemoteDisconnectReason::kModelLoadFailed:
    case ModelRemoteDisconnectReason::kRemoteIdle:
      break;
  }
  model_adaptation_controllers_.erase(feature);
}

OnDeviceModelServiceController::OnDeviceModelClient::OnDeviceModelClient(
    ModelBasedCapabilityKey feature,
    base::WeakPtr<OnDeviceModelServiceController> controller,
    const on_device_model::TextSafetyLoaderParams& ts_params,
    const on_device_model::ModelAssetPaths& model_paths,
    base::optional_ref<const on_device_model::AdaptationAssetPaths>
        adaptation_assets)
    : feature_(feature),
      controller_(controller),
      model_paths_(model_paths),
      ts_params_(ts_params),
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

mojo::Remote<on_device_model::mojom::TextSafetyModel>&
OnDeviceModelServiceController::OnDeviceModelClient::
    GetTextSafetyModelRemote() {
  return controller_->GetTextSafetyModelRemote(ts_params_);
}

void OnDeviceModelServiceController::OnDeviceModelClient::
    OnResponseCompleted() {
  if (controller_) {
    controller_->access_controller_->OnResponseCompleted();
  }
}

void OnDeviceModelServiceController::OnDeviceModelClient::OnSessionTimedOut() {
  if (controller_) {
    controller_->access_controller_->OnSessionTimedOut();
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
  DCHECK(features::internal::IsOnDeviceModelEnabled(feature));
  model_availability_change_observers_[feature].AddObserver(observer);
}

void OnDeviceModelServiceController::
    RemoveOnDeviceModelAvailabilityChangeObserver(
        ModelBasedCapabilityKey feature,
        OnDeviceModelAvailabilityObserver* observer) {
  DCHECK(features::internal::IsOnDeviceModelEnabled(feature));
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
