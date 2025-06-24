// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_page_handler.h"

#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_page.mojom.h"
#include "components/optimization_guide/core/delivery/prediction_manager.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_manager.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/on_device_model/ml/performance_class.h"
#include "services/on_device_model/public/cpp/buildflags.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

#if BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"
#endif

namespace on_device_internals {

namespace {

using optimization_guide::model_execution::prefs::localstate::
    kLastUsageByFeature;
using optimization_guide::model_execution::prefs::localstate::
    kOnDeviceModelCrashCount;

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

  if (optimization_guide::features::ForceCpuBackendForOnDeviceModel()) {
    model_paths.cache =
        model_paths.weights.AddExtension(FILE_PATH_LITERAL("cache"));
  }

  return on_device_model::LoadModelAssets(model_paths);
}
#endif

base::flat_map<std::string, std::string> GetCriteria(
    const optimization_guide::OnDeviceModelComponentStateManager::DebugState&
        debug_state) {
  auto* criteria = debug_state.criteria_.get();
  base::flat_map<std::string, std::string> mojom_criteria;
  if (criteria == nullptr) {
    return mojom_criteria;
  }
  mojom_criteria["device capable"] = base::ToString(criteria->device_capable);
  mojom_criteria["on device feature recently used"] =
      base::ToString(criteria->on_device_feature_recently_used);
  mojom_criteria["enabled by feature"] =
      base::ToString(criteria->enabled_by_feature);
  mojom_criteria["enabled by enterprise policy"] =
      base::ToString(criteria->enabled_by_enterprise_policy);
  mojom_criteria["out of retention"] =
      base::ToString(criteria->out_of_retention);
  mojom_criteria["is already installing"] =
      base::ToString(criteria->is_already_installing);

  // Disk criteria, needs to show what's available vs. required when not met.
  std::string disk_space_string =
      base::ToString(criteria->disk_space_available);
  if (!criteria->disk_space_available) {
    int disk_space_required_mb = optimization_guide::features::
        GetDiskSpaceRequiredInMbForOnDeviceModelInstall();
    int disk_space_available_mb =
        debug_state.disk_space_available_ / (1024 * 1024);
    disk_space_string = base::StrCat(
        {" (", base::NumberToString(disk_space_available_mb),
         " MiB available, ", base::NumberToString(disk_space_required_mb),
         " MiB required)"});
  }
  mojom_criteria["disk space available"] = disk_space_string;
  return mojom_criteria;
}

// Returns the minimum VRAM, in MiB, required to satisfy the currently active
// performance class requirement.
uint64_t GetMinimumVramRequired() {
  std::string perf_classes_string =
      optimization_guide::features::kPerformanceClassListForOnDeviceModel.Get();

  if (optimization_guide::IsPerformanceClassCompatible(
          perf_classes_string,
          optimization_guide::OnDeviceModelPerformanceClass::kVeryLow)) {
    return 0ul;
  } else if (optimization_guide::IsPerformanceClassCompatible(
                 perf_classes_string,
                 optimization_guide::OnDeviceModelPerformanceClass::kLow) ||
             optimization_guide::IsPerformanceClassCompatible(
                 perf_classes_string,
                 optimization_guide::OnDeviceModelPerformanceClass::kMedium)) {
    return ml::GetLowRamThresholdMb();
  } else {
    return ml::GetHighRamThresholdMb();
  }
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
#if BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
  // We treat the file path as a UUID on ChromeOS.
  base::Uuid uuid = base::Uuid::ParseLowercase(model_path.value());
  if (!uuid.is_valid()) {
    std::move(callback).Run(
        on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary,
        on_device_model::Capabilities());
    return;
  }
  GetService().LoadPlatformModel(
      uuid, std::move(model), mojo::NullRemote(),
      base::BindOnce(
          [](LoadModelCallback callback,
             on_device_model::mojom::LoadModelResult result) {
            std::move(callback).Run(result, on_device_model::Capabilities());
          },
          std::move(callback)));
#else
  // Warm the service while assets load in the background.
  std::ignore = GetService();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LoadModelAssets, model_path),
      base::BindOnce(&PageHandler::OnModelAssetsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(model),
                     std::move(callback), performance_hint));
#endif
}

PageHandler::Service& PageHandler::GetService() {
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
void PageHandler::OnModelAssetsLoaded(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback,
    ml::ModelPerformanceHint performance_hint,
    on_device_model::ModelAssets assets) {
  on_device_model::ModelFile weights = assets.weights;

  auto params = on_device_model::mojom::LoadModelParams::New();
  params->assets = std::move(assets);
  params->backend_type =
      optimization_guide::features::ForceCpuBackendForOnDeviceModel()
          ? ml::ModelBackendType::kCpuBackend
          : ml::ModelBackendType::kGpuBackend;
  params->max_tokens = 4096;
  params->performance_hint = performance_hint;
  GetService().LoadModel(
      std::move(params), std::move(model),
      base::BindOnce(&PageHandler::OnModelLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(weights)));
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
#endif

void PageHandler::GetDevicePerformanceInfo(
    GetDevicePerformanceInfoCallback callback) {
#if BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
  GetService().GetEstimatedPerformanceClass(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              [](GetDevicePerformanceInfoCallback callback,
                 on_device_model::mojom::PerformanceClass performance_class) {
                auto perf_info =
                    on_device_model::mojom::DevicePerformanceInfo::New();
                perf_info->performance_class = performance_class;
                std::move(callback).Run(std::move(perf_info));
              },
              std::move(callback)),
          on_device_model::mojom::PerformanceClass::kError));
#else
  GetService().GetDevicePerformanceInfo(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback),
          on_device_model::mojom::DevicePerformanceInfo::New()));
#endif
}

void PageHandler::GetDefaultModelPath(GetDefaultModelPathCallback callback) {
  auto* component_manager =
      optimization_guide_keyed_service_->GetComponentManager();
  auto debug_state =
      component_manager->GetDebugState(base::PassKey<PageHandler>());

  if (!debug_state.state_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(debug_state.state_->GetInstallDirectory());
}

void PageHandler::OnLogMessageAdded(
    base::Time event_time,
    optimization_guide_common::mojom::LogSource log_source,
    const std::string& source_file,
    int source_line,
    const std::string& message) {
  if (log_source ==
          optimization_guide_common::mojom::LogSource::MODEL_EXECUTION ||
      log_source == optimization_guide_common::mojom::LogSource::BUILT_IN_AI) {
    page_->OnLogMessageAdded(event_time, source_file, source_line, message);
  }
}

void PageHandler::OnReceivedPerformanceInfoForPageData(
    PageHandler::GetPageDataCallback callback,
    on_device_model::mojom::DevicePerformanceInfoPtr performance_info) {
  auto data = mojom::PageData::New();
  data->performance_info = std::move(performance_info);

  auto* component_manager =
      optimization_guide_keyed_service_->GetComponentManager();
  auto debug_state =
      component_manager->GetDebugState(base::PassKey<PageHandler>());

  data->base_model = mojom::BaseModelState::New();
  data->base_model->state =
      base::StrCat({base::ToString(debug_state.status_),
                    debug_state.has_override_ ? " (Overridden)" : ""});

  if (debug_state.state_) {
    auto info = mojom::BaseModelInfo::New();
    info->file_path = debug_state.state_->GetInstallDirectory().AsUTF8Unsafe();
    info->component_version =
        debug_state.state_->GetComponentVersion().GetString();
    info->version = debug_state.state_->GetBaseModelSpec().model_version;
    info->name = debug_state.state_->GetBaseModelSpec().model_name;
    data->base_model->info = std::move(info);
  }

  data->base_model->registration_criteria = GetCriteria(debug_state);

  // Populate status for supplementary models.
  base::flat_map<std::string, bool> supp_models =
      optimization_guide_keyed_service_->GetPredictionManager()
          ->GetOnDeviceSupplementaryModelsInfoForWebUI();
  for (const auto& it : supp_models) {
    auto supp_model_mojom = mojom::SupplementaryModelInfo::New();
    supp_model_mojom->supp_model_name = it.first;
    supp_model_mojom->is_ready = it.second;
    data->supp_models.push_back(std::move(supp_model_mojom));
  }

  // Get crash counts
  PrefService* prefs = g_browser_process->local_state();
  data->model_crash_count = prefs->GetInteger(kOnDeviceModelCrashCount);
  data->max_model_crash_count =
      optimization_guide::features::GetOnDeviceModelCrashCountBeforeDisable();

  // Get data on feature adaptations.
  const base::flat_map<optimization_guide::ModelBasedCapabilityKey,
                       optimization_guide::OnDeviceModelAdaptationMetadata>&
      feature_adaptations =
          optimization_guide_keyed_service_->GetModelExecutionManager()
              ->GetOnDeviceModelServiceController()
              ->model_adaptation_metadata();
  const PrefService* local_state = g_browser_process->local_state();
  for (const auto feature : optimization_guide::kAllModelBasedCapabilityKeys) {
    if (!optimization_guide::features::internal::
            GetOptimizationTargetForCapability(feature)) {
      continue;
    }
    auto feature_adaptation_info = mojom::FeatureAdaptationInfo::New();
    feature_adaptation_info->feature_name = base::ToString(feature);
    feature_adaptation_info->feature_key = static_cast<int32_t>(feature);
    feature_adaptation_info->is_recently_used =
        WasOnDeviceEligibleFeatureRecentlyUsed(feature, *local_state);

    auto it = feature_adaptations.find(feature);
    if (it != feature_adaptations.end()) {
      feature_adaptation_info->version = it->second.version();
    } else {
      feature_adaptation_info->version = 0;
    }
    data->feature_adaptations.push_back(std::move(feature_adaptation_info));
  }
  data->min_vram_mb = GetMinimumVramRequired();

  std::move(callback).Run(std::move(data));
}

void PageHandler::GetPageData(PageHandler::GetPageDataCallback callback) {
  GetDevicePerformanceInfo(
      base::BindOnce(&PageHandler::OnReceivedPerformanceInfoForPageData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PageHandler::SetFeatureRecentlyUsedState(int feature_key,
                                              bool is_recently_used) {
  ::prefs::ScopedDictionaryPrefUpdate update(g_browser_process->local_state(),
                                             kLastUsageByFeature);
  std::string pref_key = base::NumberToString(
      static_cast<uint64_t>(optimization_guide::ToModelExecutionFeatureProto(
          static_cast<optimization_guide::ModelBasedCapabilityKey>(
              feature_key))));
  if (is_recently_used) {
    update->Set(pref_key, base::TimeToValue(base::Time::Now()));
  } else {
    update->Remove(pref_key);
  }
}

void PageHandler::DecodeBitmap(mojo_base::BigBuffer image_buffer,
                               DecodeBitmapCallback callback) {
  data_decoder::DecodeImageIsolated(
      base::span(image_buffer), data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(), std::move(callback));
}

void PageHandler::ResetModelCrashCount() {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(kOnDeviceModelCrashCount, 0);
}

}  // namespace on_device_internals
