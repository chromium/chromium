// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_page_handler.h"

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_page.mojom.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/on_device_model/public/cpp/buildflags.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/model_assets.h"

#if BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"
#endif

namespace on_device_internals {

namespace {

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

  // TODO(crbug.com/461547475): Determine whether weight caches should be used
  // for GPU or just CPU only.
  if (base::FeatureList::IsEnabled(
          on_device_model::features::kOnDeviceModelForceCpuBackend)) {
    model_paths.cache =
        model_paths.weights.AddExtension(FILE_PATH_LITERAL("cache"));
  }

  return on_device_model::LoadModelAssets(model_paths);
}

}  // namespace

PageHandler::PageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::Page> page,
    OptimizationGuideKeyedService* optimization_guide_keyed_service)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      optimization_guide_logger_(
          optimization_guide_keyed_service->GetOptimizationGuideLogger()),
      optimization_guide_keyed_service_(optimization_guide_keyed_service) {
  if (optimization_guide_logger_) {
    optimization_guide_logger_->AddObserver(this);
  }
}

PageHandler::~PageHandler() {
  if (optimization_guide_logger_) {
    optimization_guide_logger_->RemoveObserver(this);
  }
}

void PageHandler::LoadModel(
    const base::FilePath& model_path,
    ml::ModelPerformanceHint performance_hint,
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  // Warm the service while assets load in the background.
  std::ignore = GetService();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LoadModelAssets, model_path),
      base::BindOnce(&PageHandler::OnModelAssetsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(model),
                     std::move(callback), performance_hint));
}

void PageHandler::LoadPlatformModel(
    const base::FilePath& model_path,
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadPlatformModelCallback callback) {
#if BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
  // We treat the file path as a UUID on ChromeOS.
  base::Uuid uuid = base::Uuid::ParseLowercase(model_path.value());
  if (!uuid.is_valid()) {
    std::move(callback).Run(
        on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }
  GetPlatformService().LoadPlatformModel(
      uuid, std::move(model), mojo::NullRemote(), std::move(callback));
#else
  // Shouldn't be called.
  std::move(callback).Run(
      on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary);
#endif
}

PageHandler::Service& PageHandler::GetService() {
  if (!service_) {
    content::ServiceProcessHost::Launch<
        on_device_model::mojom::OnDeviceModelService>(
        service_.BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName("On-Device Model Service")
            .Pass());
    service_.reset_on_disconnect();
  }
  return *service_.get();
}

#if BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
PageHandler::PlatformService& PageHandler::GetPlatformService() {
  if (!platform_service_) {
    ash::mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosOdmlService, std::nullopt,
        platform_service_.BindNewPipeAndPassReceiver().PassPipe());
    platform_service_.reset_on_disconnect();
  }
  return *platform_service_.get();
}
#endif

void PageHandler::OnModelAssetsLoaded(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback,
    ml::ModelPerformanceHint performance_hint,
    on_device_model::ModelAssets assets) {
  on_device_model::ModelFile weights = assets.weights;

  auto params = on_device_model::mojom::LoadModelParams::New();
  params->assets = std::move(assets);
  params->backend_type =
      base::FeatureList::IsEnabled(
          on_device_model::features::kOnDeviceModelForceCpuBackend)
          ? ml::ModelBackendType::kCpuBackend
          : ml::ModelBackendType::kGpuBackend;
  params->max_tokens = 4096;
  params->performance_hint = performance_hint;
  GetService().LoadModel(
      std::move(params), std::move(model),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&PageHandler::OnModelLoaded,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         std::move(weights)),
          on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary));
}

void PageHandler::OnModelLoaded(
    LoadModelCallback callback,
    on_device_model::ModelFile weights,
    on_device_model::mojom::LoadModelResult result) {
  if (result != on_device_model::mojom::LoadModelResult::kSuccess) {
    std::move(callback).Run(result, on_device_model::Capabilities());
    return;
  }
  GetService().GetCapabilities(
      std::move(weights),
      base::BindOnce(std::move(callback),
                     on_device_model::mojom::LoadModelResult::kSuccess));
}

void PageHandler::GetDeviceAndPerformanceInfo(
    GetDeviceAndPerformanceInfoCallback callback) {
  GetService().GetDeviceAndPerformanceInfo(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback),
          on_device_model::mojom::DevicePerformanceInfo::New(),
          on_device_model::mojom::DeviceInfo::New()));
}

void PageHandler::GetDefaultModelPath(GetDefaultModelPathCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void PageHandler::OnLogMessageAdded(
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

void PageHandler::DecodeBitmap(mojo_base::BigBuffer image_buffer,
                              DecodeBitmapCallback callback) {
  data_decoder::DecodeImageIsolated(
      base::span(image_buffer), data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(), std::move(callback));
}

void PageHandler::BindModelBrokerDebug(
    mojo::PendingReceiver<optimization_guide::mojom::ModelBrokerDebug>
        receiver) {
#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
  optimization_guide_keyed_service_->GetGlobalState()
      .on_device_capability()
      .BindModelBrokerDebug(base::PassKey<PageHandler>(), std::move(receiver));
#endif
}

}  // namespace on_device_internals
