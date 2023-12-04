// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/model_execution/session_impl.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

namespace {

class ScopedEligibilityReasonLogger {
 public:
  explicit ScopedEligibilityReasonLogger(proto::ModelExecutionFeature feature)
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
  proto::ModelExecutionFeature feature_;

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

}  // namespace

OnDeviceModelServiceController::OnDeviceModelServiceController(
    std::unique_ptr<OnDeviceModelAccessController> access_controller)
    : access_controller_(std::move(access_controller)) {}

OnDeviceModelServiceController::~OnDeviceModelServiceController() = default;

void OnDeviceModelServiceController::Init(
    const base::FilePath& model_path,
    std::unique_ptr<OnDeviceModelExecutionConfigInterpreter>
        config_interpreter) {
  CHECK(model_path_.empty());
  model_path_ = model_path;
  config_interpreter_ = std::move(config_interpreter);
  config_interpreter_->UpdateConfigWithFileDir(model_path_);
}

void OnDeviceModelServiceController::Init() {
  auto model_path_override_switch =
      switches::GetOnDeviceModelExecutionOverride();
  if (model_path_override_switch) {
    auto file_path = StringToFilePath(*model_path_override_switch);
    if (file_path) {
      Init(*file_path,
           std::make_unique<OnDeviceModelExecutionConfigInterpreter>());
    }
  }
}

std::unique_ptr<OptimizationGuideModelExecutor::Session>
OnDeviceModelServiceController::CreateSession(
    proto::ModelExecutionFeature feature,
    ExecuteRemoteFn execute_remote_fn,
    OptimizationGuideLogger* optimization_guide_logger) {
  ScopedEligibilityReasonLogger logger(feature);
  if (!base::FeatureList::IsEnabled(
          features::kOptimizationGuideOnDeviceModel)) {
    logger.set_reason(OnDeviceModelEligibilityReason::kFeatureNotEnabled);
    return nullptr;
  }
  if (model_path_.empty()) {
    logger.set_reason(OnDeviceModelEligibilityReason::kModelNotAvailable);
    return nullptr;
  }
  if (!config_interpreter_->HasConfigForFeature(feature)) {
    logger.set_reason(
        OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature);
    return nullptr;
  }
  OnDeviceModelEligibilityReason reason =
      access_controller_->ShouldStartNewSession();
  logger.set_reason(reason);
  if (reason != OnDeviceModelEligibilityReason::kSuccess) {
    return nullptr;
  }
  CHECK_EQ(reason, OnDeviceModelEligibilityReason::kSuccess);

  return std::make_unique<SessionImpl>(
      base::BindRepeating(&OnDeviceModelServiceController::StartMojoSession,
                          weak_ptr_factory_.GetWeakPtr()),
      feature, config_interpreter_.get(), weak_ptr_factory_.GetWeakPtr(),
      std::move(execute_remote_fn), optimization_guide_logger);
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

void OnDeviceModelServiceController::StartMojoSession(
    mojo::PendingReceiver<on_device_model::mojom::Session> session) {
  if (!model_remote_) {
    LaunchService();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&on_device_model::LoadModelAssets, model_path_,
                       model_path_),
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
  model_remote_->StartSession(std::move(session));
}

void OnDeviceModelServiceController::OnModelAssetsLoaded(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    on_device_model::ModelAssets assets) {
  if (!service_remote_) {
    // Close the files on a background thread.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::DoNothingWithBoundArgs(std::move(assets)));
    return;
  }
  // TODO(b/302402959): Choose max_tokens based on device.
  int max_tokens = features::GetOnDeviceModelMaxTokensForContext() +
                   features::GetOnDeviceModelMaxTokensForExecute() +
                   features::GetOnDeviceModelMaxTokensForOutput();
  service_remote_->LoadModel(
      on_device_model::mojom::LoadModelParams::New(std::move(assets),
                                                   max_tokens),
      std::move(model),
      base::BindOnce(&OnDeviceModelServiceController::OnLoadModelResult,
                     weak_ptr_factory_.GetWeakPtr()));
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

bool OnDeviceModelServiceController::ShouldStartNewSession() const {
  return access_controller_->ShouldStartNewSession() ==
         OnDeviceModelEligibilityReason::kSuccess;
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

}  // namespace optimization_guide
