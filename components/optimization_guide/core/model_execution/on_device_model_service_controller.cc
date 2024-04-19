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
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/safety_model_info.h"
#include "components/optimization_guide/core/model_execution/session_impl.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

namespace {

class ScopedEligibilityReasonLogger {
 public:
  explicit ScopedEligibilityReasonLogger(ModelBasedCapabilityKey feature)
      : feature_(feature) {}
  ~ScopedEligibilityReasonLogger() {
    CHECK_NE(reason_, OnDeviceModelEligibilityReason::kUnknown);
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.",
             GetStringNameForModelExecutionFeature(feature_)}),
        reason_);
  }

  void set_reason(OnDeviceModelEligibilityReason reason) { reason_ = reason; }

 private:
  ModelBasedCapabilityKey feature_;

  OnDeviceModelEligibilityReason reason_ =
      OnDeviceModelEligibilityReason::kUnknown;
};

OnDeviceModelLoadResult ConvertToOnDeviceModelLoadResult(
    on_device_model::mojom::LoadModelResult result) {
  switch (result) {
    case on_device_model::mojom::LoadModelResult::kSuccess:
      return OnDeviceModelLoadResult::kSuccess;
    case on_device_model::mojom::LoadModelResult::kGpuBlocked:
      return OnDeviceModelLoadResult::kGpuBlocked;
    case on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary:
      return OnDeviceModelLoadResult::kFailedToLoadLibrary;
  }
}

proto::OnDeviceModelVersions GetModelVersions(
    const OnDeviceModelMetadata& model_metadata,
    const SafetyModelInfo* safety_model_info) {
  proto::OnDeviceModelVersions versions;
  versions.mutable_on_device_model_service_version()->set_component_version(
      model_metadata.version());

  if (safety_model_info) {
    versions.set_text_safety_model_version(safety_model_info->GetVersion());
  }
  return versions;
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

std::unique_ptr<OptimizationGuideModelExecutor::Session>
OnDeviceModelServiceController::CreateSession(
    ModelBasedCapabilityKey feature,
    ExecuteRemoteFn execute_remote_fn,
    base::WeakPtr<OptimizationGuideLogger> optimization_guide_logger,
    base::WeakPtr<ModelQualityLogsUploaderService>
        model_quality_uploader_service,
    const std::optional<SessionConfigParams>& config_params) {
  if (on_device_component_state_manager_) {
    on_device_component_state_manager_->OnDeviceEligibleFeatureUsed();
  }
  ScopedEligibilityReasonLogger logger(feature);
  if (!base::FeatureList::IsEnabled(
          features::kOptimizationGuideOnDeviceModel)) {
    logger.set_reason(OnDeviceModelEligibilityReason::kFeatureNotEnabled);
    return nullptr;
  }
  if (!model_metadata_) {
    logger.set_reason(OnDeviceModelEligibilityReason::kModelNotAvailable);
    return nullptr;
  }

  on_device_model::ModelAssetPaths model_paths;
  model_paths.sp_model = model_metadata_->model_path().Append(kSpModelFile);
  model_paths.model = model_metadata_->model_path().Append(kModelFile);
  model_paths.weights = model_metadata_->model_path().Append(kWeightsFile);

  std::optional<proto::FeatureTextSafetyConfiguration> safety_config;
  if (!safety_model_info_ && features::GetOnDeviceModelMustUseSafetyModel()) {
    logger.set_reason(OnDeviceModelEligibilityReason::kSafetyModelNotAvailable);
    return nullptr;
  }
  if (safety_model_info_) {
    safety_config =
        safety_model_info_->GetConfig(ToModelExecutionFeatureProto(feature));
    if (!safety_config && features::GetOnDeviceModelMustUseSafetyModel()) {
      logger.set_reason(
          OnDeviceModelEligibilityReason::kSafetyConfigNotAvailableForFeature);
      return nullptr;
    }

    if (safety_config) {
      model_paths.ts_data = safety_model_info_->GetDataPath();
      model_paths.ts_sp_model = safety_model_info_->GetSpModelPath();

      if (!safety_config->allowed_languages().empty()) {
        if (language_detection_model_path_) {
          model_paths.language_detection_model =
              *language_detection_model_path_;
        } else if (features::GetOnDeviceModelMustUseSafetyModel()) {
          logger.set_reason(OnDeviceModelEligibilityReason::
                                kLanguageDetectionModelNotAvailable);
          return nullptr;
        }
      }
    }
  }

  scoped_refptr<const OnDeviceModelFeatureAdapter> adapter =
      model_metadata_->GetAdapter(ToModelExecutionFeatureProto(feature));
  if (!adapter) {
    logger.set_reason(
        OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature);
    return nullptr;
  }

  if (feature == ModelBasedCapabilityKey::kCompose &&
      !base::FeatureList::IsEnabled(
          features::kOptimizationGuideComposeOnDeviceEval)) {
    logger.set_reason(
        OnDeviceModelEligibilityReason::kFeatureExecutionNotEnabled);
    return nullptr;
  }
  OnDeviceModelEligibilityReason reason =
      access_controller_->ShouldStartNewSession();
  logger.set_reason(reason);
  if (reason != OnDeviceModelEligibilityReason::kSuccess) {
    return nullptr;
  }
  CHECK_EQ(reason, OnDeviceModelEligibilityReason::kSuccess);

  SessionImpl::OnDeviceOptions opts;
  opts.model_client = std::make_unique<OnDeviceModelClient>(
      weak_ptr_factory_.GetWeakPtr(), model_paths);
  opts.model_versions =
      GetModelVersions(*model_metadata_, safety_model_info_.get());
  opts.adapter = std::move(adapter);
  opts.safety_cfg = SafetyConfig(safety_config);

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
    on_device_model::ModelAssetPaths model_paths) {
  if (!model_remote_) {
    LaunchService();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&on_device_model::LoadModelAssets, model_paths),
        base::BindOnce(&OnDeviceModelServiceController::OnModelAssetsLoaded,
                       weak_ptr_factory_.GetWeakPtr(),
                       model_remote_.BindNewPipeAndPassReceiver()));
    model_remote_.set_disconnect_handler(
        base::BindOnce(&OnDeviceModelServiceController::OnDisconnected,
                       base::Unretained(this)));
    model_remote_.set_idle_handler(
        features::GetOnDeviceModelIdleTimeout(),
        base::BindRepeating(&OnDeviceModelServiceController::OnRemoteIdle,
                            base::Unretained(this)));
  }
  return model_remote_;
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
  // TODO(b/302402959): Choose max_tokens based on device.
  int max_tokens = features::GetOnDeviceModelMaxTokensForContext() +
                   features::GetOnDeviceModelMaxTokensForExecute() +
                   features::GetOnDeviceModelMaxTokensForOutput();
  auto params = on_device_model::mojom::LoadModelParams::New();
  params->assets = std::move(assets);
  params->max_tokens = max_tokens;
  if (safety_model_info_) {
    params->ts_dimension = safety_model_info_->num_output_categories();
  }
  service_remote_->LoadModel(
      std::move(params), std::move(model),
      base::BindOnce(&OnDeviceModelServiceController::OnLoadModelResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OnDeviceModelServiceController::SetLanguageDetectionModel(
    base::optional_ref<const ModelInfo> model_info) {
  if (!model_info.has_value()) {
    language_detection_model_path_.reset();
    return;
  }

  language_detection_model_path_ = model_info->GetModelFilePath();
}

void OnDeviceModelServiceController::MaybeUpdateSafetyModel(
    base::optional_ref<const ModelInfo> model_info) {
  safety_model_info_ = SafetyModelInfo::Load(model_info);
}

void OnDeviceModelServiceController::UpdateModel(
    std::unique_ptr<OnDeviceModelMetadata> model_metadata) {
  model_remote_.reset();
  model_metadata_ = std::move(model_metadata);
}

void OnDeviceModelServiceController::OnLoadModelResult(
    on_device_model::mojom::LoadModelResult result) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelLoadResult",
      ConvertToOnDeviceModelLoadResult(result));
  switch (result) {
    case on_device_model::mojom::LoadModelResult::kGpuBlocked:
      access_controller_->OnGpuBlocked();
      model_remote_.reset();
      break;
    case on_device_model::mojom::LoadModelResult::kSuccess:
      break;
    case on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary:
      break;
  }
}

void OnDeviceModelServiceController::OnDisconnected() {
  model_remote_.reset();
  access_controller_->OnDisconnectedFromRemote();
}

void OnDeviceModelServiceController::ShutdownServiceIfNoModelLoaded() {
  if (!model_remote_) {
    service_remote_.reset();
  }
}

void OnDeviceModelServiceController::OnRemoteIdle() {
  service_remote_.reset();
  model_remote_.reset();
}

OnDeviceModelServiceController::OnDeviceModelClient::OnDeviceModelClient(
    base::WeakPtr<OnDeviceModelServiceController> controller,
    on_device_model::ModelAssetPaths model_paths)
    : controller_(controller), model_paths_(model_paths) {}

OnDeviceModelServiceController::OnDeviceModelClient::~OnDeviceModelClient() =
    default;

bool OnDeviceModelServiceController::OnDeviceModelClient::ShouldUse() {
  return controller_ &&
         controller_->access_controller_->ShouldStartNewSession() ==
             OnDeviceModelEligibilityReason::kSuccess;
}

mojo::Remote<on_device_model::mojom::OnDeviceModel>&
OnDeviceModelServiceController::OnDeviceModelClient::GetModelRemote() {
  return controller_->GetOrCreateModelRemote(model_paths_);
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

}  // namespace optimization_guide
