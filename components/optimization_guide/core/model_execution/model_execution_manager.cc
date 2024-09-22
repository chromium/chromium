// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

namespace {

const std::string& ProtoName(ModelBasedCapabilityKey feature) {
  return proto::ModelExecutionFeature_Name(
      ToModelExecutionFeatureProto(feature));
}

class ScopedModelExecutionResponseLogger {
 public:
  ScopedModelExecutionResponseLogger(
      ModelBasedCapabilityKey feature,
      OptimizationGuideLogger* optimization_guide_logger)
      : feature_(feature),
        optimization_guide_logger_(optimization_guide_logger) {}

  ~ScopedModelExecutionResponseLogger() {
    if (!optimization_guide_logger_->ShouldEnableDebugLogs()) {
      return;
    }
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_logger_)
        << "OnModelExecutionResponse - Feature : " << ProtoName(feature_) << " "
        << message_;
  }

  void set_message(const std::string& message) { message_ = message; }

 private:
  ModelBasedCapabilityKey feature_;
  std::string message_;

  // Not owned. Guaranteed to outlive |this| scoped object.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
};

// Returns the URL endpoint for the model execution service.
GURL GetModelExecutionServiceURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kOptimizationGuideServiceModelExecutionURL)) {
    return GURL(command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceModelExecutionURL));
  }
  return GURL(kOptimizationGuideServiceModelExecutionDefaultURL);
}

void RecordSessionUsedRemoteExecutionHistogram(ModelBasedCapabilityKey feature,
                                               bool is_remote) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.SessionUsedRemoteExecution.",
           GetStringNameForModelExecutionFeature(feature)}),
      is_remote);
}

void RecordModelExecutionResultHistogram(ModelBasedCapabilityKey feature,
                                         bool result) {
  base::UmaHistogramBoolean(
      base::StrCat({"OptimizationGuide.ModelExecution.Result.",
                    GetStringNameForModelExecutionFeature(feature)}),
      result);
}

void NoOpExecuteRemoteFn(
    ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite& request,
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
    OptimizationGuideModelExecutionResultCallback callback) {
  std::move(callback).Run(
      base::unexpected(
          OptimizationGuideModelExecutionError::FromModelExecutionError(
              OptimizationGuideModelExecutionError::ModelExecutionError::
                  kGenericFailure)),
      nullptr);
}

std::map<ModelBasedCapabilityKey, OnDeviceModelAdaptationLoader>
GetRequiredModelAdaptationLoaders(
    OptimizationGuideModelProvider* model_provider,
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager,
    PrefService* local_state,
    base::WeakPtr<OnDeviceModelServiceController>
        on_device_model_service_controller) {
  std::map<ModelBasedCapabilityKey, OnDeviceModelAdaptationLoader> loaders;
  for (const auto feature : kAllModelBasedCapabilityKeys) {
    if (!features::internal::IsOnDeviceModelEnabled(feature) ||
        !features::internal::IsOnDeviceModelAdaptationEnabled(feature)) {
      continue;
    }
    loaders.emplace(
        std::piecewise_construct, std::forward_as_tuple(feature),
        std::forward_as_tuple(
            feature, model_provider, on_device_component_state_manager,
            local_state,
            base::BindRepeating(
                &OnDeviceModelServiceController::MaybeUpdateModelAdaptation,
                on_device_model_service_controller, feature)));
  }
  return loaders;
}

}  // namespace

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

ModelExecutionManager::ModelExecutionManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_state,
    signin::IdentityManager* identity_manager,
    scoped_refptr<OnDeviceModelServiceController>
        on_device_model_service_controller,
    OptimizationGuideModelProvider* model_provider,
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager,
    OptimizationGuideLogger* optimization_guide_logger,
    base::WeakPtr<ModelQualityLogsUploaderService>
        model_quality_uploader_service)
    : model_quality_uploader_service_(model_quality_uploader_service),
      optimization_guide_logger_(optimization_guide_logger),
      model_execution_service_url_(net::AppendOrReplaceQueryParameter(
          GetModelExecutionServiceURL(),
          "key",
          features::GetOptimizationGuideServiceAPIKey())),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      model_adaptation_loaders_(GetRequiredModelAdaptationLoaders(
          model_provider,
          on_device_component_state_manager,
          local_state,
          on_device_model_service_controller
              ? on_device_model_service_controller->GetWeakPtr()
              : nullptr)),
      model_provider_(model_provider),
      on_device_model_service_controller_(
          std::move(on_device_model_service_controller)) {
  if (!model_provider_ && !on_device_model_service_controller_) {
    return;
  }
  if (!features::ShouldUseTextSafetyClassifierModel()) {
    return;
  }
  if (GetGenAILocalFoundationalModelEnterprisePolicySettings(local_state) !=
      model_execution::prefs::
          GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed) {
    return;
  }

  did_register_for_supplementary_on_device_models_ = true;
  model_provider_->AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_TEXT_SAFETY,
      /*model_metadata=*/std::nullopt, this);
  model_provider_->AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
      /*model_metadata=*/std::nullopt, this);
}

ModelExecutionManager::~ModelExecutionManager() {
  if (did_register_for_supplementary_on_device_models_) {
    model_provider_->RemoveObserverForOptimizationTargetModel(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_TEXT_SAFETY, this);
    model_provider_->RemoveObserverForOptimizationTargetModel(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
        this);
  }
}

void ModelExecutionManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Invalidate the weak pointers before clearing the active fetchers, which
  // will cause the drop all the model execution consumer callbacks, and avoid
  // all processing during destructor.
  weak_ptr_factory_.InvalidateWeakPtrs();
  active_model_execution_fetchers_.clear();
}

void ModelExecutionManager::ExecuteModel(
    ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite& request_metadata,
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
    OptimizationGuideModelExecutionResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto previous_fetcher_it = active_model_execution_fetchers_.find(feature);
  if (previous_fetcher_it != active_model_execution_fetchers_.end()) {
    // Cancel the existing fetcher and let the new one continue.
    active_model_execution_fetchers_.erase(previous_fetcher_it);
    RecordModelExecutionResultHistogram(feature, false);
    CHECK(active_model_execution_fetchers_.find(feature) ==
          active_model_execution_fetchers_.end());
  }

  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_logger_)
        << "ExecuteModel: " << ProtoName(feature);
    switch (feature) {
      case ModelBasedCapabilityKey::kTabOrganization: {
        proto::Any any;
        any.set_type_url(request_metadata.GetTypeName());
        request_metadata.SerializeToString(any.mutable_value());
        auto tab_request = optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::TabOrganizationRequest>(any);
        std::string tabs = "";
        for (const auto& tab : tab_request->tabs()) {
          tabs += base::StringPrintf("%s\"%s\"", tabs.empty() ? "" : ",",
                                     tab.title().c_str());
        }
        OPTIMIZATION_GUIDE_LOGGER(
            optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
            optimization_guide_logger_)
            << "TabOrganization Request: "
            << base::StringPrintf(
                   "{\"model_strategy\": \"%s\", \"tabs\" : [%s]}",
                   optimization_guide::proto::
                       TabOrganizationRequest_TabOrganizationModelStrategy_Name(
                           tab_request->model_strategy()),
                   tabs.c_str());

        break;
      }
      default: {
        break;
      }
    }
  }

  // Create log request if not already provided.
  if (!log_ai_data_request) {
    log_ai_data_request = std::make_unique<proto::LogAiDataRequest>();
  }

  // Set execution request in corresponding `log_ai_data_request`.
  SetExecutionRequest(feature, *log_ai_data_request.get(), request_metadata);

  auto fetcher_it = active_model_execution_fetchers_.emplace(
      std::piecewise_construct, std::forward_as_tuple(feature),
      std::forward_as_tuple(url_loader_factory_, model_execution_service_url_,
                            optimization_guide_logger_));
  fetcher_it.first->second.ExecuteModel(
      feature, identity_manager_, request_metadata,
      base::BindOnce(&ModelExecutionManager::OnModelExecuteResponse,
                     weak_ptr_factory_.GetWeakPtr(), feature,
                     std::move(log_ai_data_request), std::move(callback)));
}

bool ModelExecutionManager::CanCreateOnDeviceSession(
    ModelBasedCapabilityKey feature,
    OnDeviceModelEligibilityReason* on_device_model_eligibility_reason) {
  if (!on_device_model_service_controller_) {
    if (on_device_model_eligibility_reason) {
      *on_device_model_eligibility_reason =
          OnDeviceModelEligibilityReason::kFeatureNotEnabled;
    }
    return false;
  }

  OnDeviceModelEligibilityReason reason =
      on_device_model_service_controller_->CanCreateSession(feature);
  if (on_device_model_eligibility_reason) {
    *on_device_model_eligibility_reason = reason;
  }
  return reason == OnDeviceModelEligibilityReason::kSuccess;
}

std::unique_ptr<OptimizationGuideModelExecutor::Session>
ModelExecutionManager::StartSession(
    ModelBasedCapabilityKey feature,
    const std::optional<SessionConfigParams>& config_params) {
  SessionConfigParams::ExecutionMode execution_mode =
      config_params ? config_params->execution_mode
                    : SessionConfigParams::ExecutionMode::kDefault;
  ExecuteRemoteFn execute_fn =
      execution_mode == SessionConfigParams::ExecutionMode::kOnDeviceOnly
          ? base::BindRepeating(&NoOpExecuteRemoteFn)
          : base::BindRepeating(&ModelExecutionManager::ExecuteModel,
                                weak_ptr_factory_.GetWeakPtr());
  if (on_device_model_service_controller_ &&
      execution_mode != SessionConfigParams::ExecutionMode::kServerOnly) {
    auto session = on_device_model_service_controller_->CreateSession(
        feature, execute_fn, optimization_guide_logger_->GetWeakPtr(),
        model_quality_uploader_service_, config_params);
    if (session) {
      RecordSessionUsedRemoteExecutionHistogram(feature, /*is_remote=*/false);
      return session;
    }
  }

  if (execution_mode == SessionConfigParams::ExecutionMode::kOnDeviceOnly) {
    return nullptr;
  }

  RecordSessionUsedRemoteExecutionHistogram(feature, /*is_remote=*/true);
  return std::make_unique<SessionImpl>(
      feature, std::nullopt, std::move(execute_fn),
      optimization_guide_logger_->GetWeakPtr(), model_quality_uploader_service_,
      config_params);
}

void ModelExecutionManager::OnModelExecuteResponse(
    ModelBasedCapabilityKey feature,
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
    OptimizationGuideModelExecutionResultCallback callback,
    base::expected<const proto::ExecuteResponse,
                   OptimizationGuideModelExecutionError> execute_response) {
  active_model_execution_fetchers_.erase(feature);
  ScopedModelExecutionResponseLogger scoped_logger(feature,
                                                   optimization_guide_logger_);

  // Create corresponding log entry for `log_ai_data_request` to pass it with
  // the callback.
  std::unique_ptr<ModelQualityLogEntry> log_entry =
      std::make_unique<ModelQualityLogEntry>(std::move(log_ai_data_request),
                                             model_quality_uploader_service_);

  if (!execute_response.has_value()) {
    scoped_logger.set_message("Error: No Response");
    RecordModelExecutionResultHistogram(feature, false);
    auto error = execute_response.error();
    // TODO(b/350546291): move this logging code to a ModelExecute wrapper.
    log_entry->set_model_execution_error(error);
    std::move(callback).Run(base::unexpected(error), std::move(log_entry));
    return;
  }

  // Set the id if present.
  if (execute_response->has_server_execution_id()) {
    log_entry->set_model_execution_id(execute_response->server_execution_id());
  }

  if (execute_response->has_error_response()) {
    scoped_logger.set_message("Error: No Response Metadata");
    log_entry->set_error_response(execute_response->error_response());
    // For unallowed error states, don't log request data.
    auto error =
        OptimizationGuideModelExecutionError::FromModelExecutionServerError(
            execute_response->error_response());
    RecordModelExecutionResultHistogram(feature, false);
    base::UmaHistogramEnumeration(
        base::StrCat({"OptimizationGuide.ModelExecution.ServerError.",
                      GetStringNameForModelExecutionFeature(feature)}),
        error.error());
    // TODO(b/350546291): move this logging code to a ModelExecute wrapper.
    log_entry->set_model_execution_error(error);

    if (!error.ShouldLogModelQuality()) {
      log_entry = nullptr;
    }
    std::move(callback).Run(base::unexpected(error), std::move(log_entry));
    return;
  }

  if (!execute_response->has_response_metadata()) {
    scoped_logger.set_message("Error: No Response Metadata");
    RecordModelExecutionResultHistogram(feature, false);
    auto error = OptimizationGuideModelExecutionError::FromModelExecutionError(
        ModelExecutionError::kGenericFailure);
    // TODO(b/350546291): move this logging code to a ModelExecute wrapper.
    log_entry->set_model_execution_error(error);
    // Log the request in case response is not present by passing the
    // `log_entry`.
    std::move(callback).Run(base::unexpected(error), std::move(log_entry));
    return;
  }

  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_logger_)
        << "ExecuteModel Response: " << ProtoName(feature);
    switch (feature) {
      case ModelBasedCapabilityKey::kTabOrganization: {
        std::string message = "";
        auto tab_response = optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::TabOrganizationResponse>(
            execute_response->response_metadata());
        message += "Response: [";
        int group_cnt = 0;
        for (const auto& tab_group : tab_response->tab_groups()) {
          std::string tab_titles = "";
          for (const auto& tab : tab_group.tabs()) {
            tab_titles +=
                base::StringPrintf("%s\" %s \"", tab_titles.empty() ? "" : ",",
                                   tab.title().c_str());
          }
          message += base::StringPrintf(
              "%s{"
              "\"label\": \"%s\", "
              "\"tabs\": [%s] }",
              group_cnt > 0 ? "," : "", tab_group.label().c_str(),
              tab_titles.c_str());
          group_cnt += 1;
        }
        message += "]";
        scoped_logger.set_message(message);
        break;
      }
      default: {
        break;
      }
    }
  }

  // Set execution response in corresponding `log_ai_data_request`.
  SetExecutionResponse(feature, *(log_entry.get()->log_ai_data_request()),
                       execute_response->response_metadata());

  RecordModelExecutionResultHistogram(feature, true);
  std::move(callback).Run(base::ok(execute_response->response_metadata()),
                          std::move(log_entry));
}

void ModelExecutionManager::OnModelUpdated(
    proto::OptimizationTarget optimization_target,
    base::optional_ref<const ModelInfo> model_info) {
  switch (optimization_target) {
    case proto::OPTIMIZATION_TARGET_TEXT_SAFETY:
      if (on_device_model_service_controller_) {
        on_device_model_service_controller_->MaybeUpdateSafetyModel(model_info);
      }
      break;

    case proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION:
      if (on_device_model_service_controller_) {
        on_device_model_service_controller_->SetLanguageDetectionModel(
            model_info);
      }
      break;

    default:
      break;
  }
}

}  // namespace optimization_guide
