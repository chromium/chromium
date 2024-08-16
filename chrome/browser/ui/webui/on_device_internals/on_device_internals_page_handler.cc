// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_page_handler.h"

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/on_device_model/public/cpp/buildflags.h"
#include "services/on_device_model/public/cpp/model_assets.h"

#if BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"
#endif

namespace {
#if !BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
on_device_model::ModelAssets LoadModelAssets(const base::FilePath& model_path) {
  // This WebUI currently provides no way to dynamically configure the expected
  // output dimension of the TS model. Since the model is in flux and its output
  // dimension can change, it would be easy to accidentally load an incompatible
  // model and crash the service. Hence we omit TS model assets for now.
  on_device_model::ModelAssetPaths model_paths;
  if (base::DirectoryExists(model_path)) {
    model_paths.weights = model_path.Append(optimization_guide::kWeightsFile);
  } else {
    model_paths.weights = model_path;
  }
  return on_device_model::LoadModelAssets(model_paths);
}
#endif
}  // namespace

OnDeviceInternalsPageHandler::OnDeviceInternalsPageHandler(
    mojo::PendingReceiver<mojom::OnDeviceInternalsPageHandler> receiver,
    mojo::PendingRemote<mojom::OnDeviceInternalsPage> page,
    OptimizationGuideLogger* optimization_guide_logger)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      optimization_guide_logger_(optimization_guide_logger) {
  if (optimization_guide_logger_) {
    optimization_guide_logger_->AddObserver(this);
  }
}

OnDeviceInternalsPageHandler::~OnDeviceInternalsPageHandler() {
  if (optimization_guide_logger_) {
    optimization_guide_logger_->RemoveObserver(this);
  }
}

void OnDeviceInternalsPageHandler::LoadModel(
    const base::FilePath& model_path,
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
#if BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
  // We treat the file path as a UUID on ChromeOS.
  base::Uuid uuid = base::Uuid::ParseLowercase(model_path.value());
  if (!uuid.is_valid()) {
    std::move(callback).Run(
        on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }
  GetService().LoadPlatformModel(uuid, std::move(model), mojo::NullRemote(),
                                 std::move(callback));
#else
  // Warm the service while assets load in the background.
  std::ignore = GetService();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LoadModelAssets, model_path),
      base::BindOnce(&OnDeviceInternalsPageHandler::OnModelAssetsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(model),
                     std::move(callback)));
#endif
}

OnDeviceInternalsPageHandler::Service&
OnDeviceInternalsPageHandler::GetService() {
  if (!service_) {
#if BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
    ash::mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosOdmlService, std::nullopt,
        service_.BindNewPipeAndPassReceiver().PassPipe());
#else
    content::ServiceProcessHost::Launch<
        on_device_model::mojom::OnDeviceModelService>(
        service_.BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName("On-Device Model Service")
            .Pass());
#endif
    service_.reset_on_disconnect();
  }
  return *service_.get();
}

#if !BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
void OnDeviceInternalsPageHandler::OnModelAssetsLoaded(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback,
    on_device_model::ModelAssets assets) {
  auto params = on_device_model::mojom::LoadModelParams::New();
  params->assets = std::move(assets);
  params->max_tokens = 4096;
  GetService().LoadModel(std::move(params), std::move(model),
                         std::move(callback));
}
#endif

void OnDeviceInternalsPageHandler::GetEstimatedPerformanceClass(
    GetEstimatedPerformanceClassCallback callback) {
  GetService().GetEstimatedPerformanceClass(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback),
          on_device_model::mojom::PerformanceClass::kError));
}

void OnDeviceInternalsPageHandler::OnLogMessageAdded(
    base::Time event_time,
    optimization_guide_common::mojom::LogSource log_source,
    const std::string& source_file,
    int source_line,
    const std::string& message) {
  if (log_source ==
      optimization_guide_common::mojom::LogSource::MODEL_EXECUTION) {
    page_->OnLogMessageAdded(event_time, source_file, source_line, message);
  }
}
