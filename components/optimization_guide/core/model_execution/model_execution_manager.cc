// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_stream_receiver.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

namespace {

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

// Sets request data corresponding the feature's LogAiDataRequest.
template <typename FeatureType>
void SetExecutionRequestTemplate(
    proto::LogAiDataRequest& log_ai_request,
    const google::protobuf::MessageLite& request_metadata) {
  typename FeatureType::LoggingData* logging_data =
      FeatureType::GetLoggingData(log_ai_request);
  CHECK(logging_data);

  auto typed_request =
      static_cast<const FeatureType::Request&>(request_metadata);
  *(logging_data->mutable_request_data()) = typed_request;
}

// Sets response data corresponding the feature's LogAiDataRequest.
template <typename FeatureType>
void SetExecutionResponseTemplate(
    proto::LogAiDataRequest& log_ai_request,
    const google::protobuf::MessageLite& response_metadata) {
  typename FeatureType::LoggingData* logging_data =
      FeatureType::GetLoggingData(log_ai_request);
  CHECK(logging_data);

  auto typed_response =
      static_cast<const FeatureType::Response&>(response_metadata);
  *(logging_data->mutable_response_data()) = typed_response;
}

// Helper method matches feature to corresponding FeatureTypeMap to set
// LogAiDataRequest's request data.
void SetExecutionRequest(
    proto::ModelExecutionFeature feature,
    proto::LogAiDataRequest& log_ai_request,
    const google::protobuf::MessageLite& request_metadata) {
  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH:
      SetExecutionRequestTemplate<WallpaperSearchFeatureTypeMap>(
          log_ai_request, request_metadata);
      return;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION:
      SetExecutionRequestTemplate<TabOrganizationFeatureTypeMap>(
          log_ai_request, request_metadata);
      return;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE:
      SetExecutionRequestTemplate<ComposeFeatureTypeMap>(log_ai_request,
                                                         request_metadata);
      return;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      // Don't log any request data when the feature is not specified.
      NOTREACHED();
      return;
  }
}

// Helper method matches feature to corresponding FeatureTypeMap to set
// LogAiDataRequest's response data.
void SetExecutionResponse(
    proto::ModelExecutionFeature feature,
    proto::LogAiDataRequest& log_ai_request,
    const google::protobuf::MessageLite& response_metadata) {
  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH:
      SetExecutionResponseTemplate<WallpaperSearchFeatureTypeMap>(
          log_ai_request, response_metadata);
      return;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION:
      SetExecutionResponseTemplate<TabOrganizationFeatureTypeMap>(
          log_ai_request, response_metadata);
      return;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE:
      SetExecutionResponseTemplate<ComposeFeatureTypeMap>(log_ai_request,
                                                          response_metadata);
      return;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      // Don't log any response data when the feature is not specified.
      NOTREACHED();
      return;
  }
}

}  // namespace

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

ModelExecutionManager::ModelExecutionManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<OnDeviceModelServiceController>
        on_device_model_service_controller,
    OptimizationGuideLogger* optimization_guide_logger)
    : optimization_guide_logger_(optimization_guide_logger),
      model_execution_service_url_(net::AppendOrReplaceQueryParameter(
          GetModelExecutionServiceURL(),
          "key",
          features::GetOptimizationGuideServiceAPIKey())),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      oauth_scopes_(features::GetOAuthScopesForModelExecution()),
      on_device_model_service_controller_(
          std::move(on_device_model_service_controller)),
      on_device_model_execution_config_interpreter_(
          std::make_unique<OnDeviceModelExecutionConfigInterpreter>()) {
  auto model_path_override_switch =
      switches::GetOnDeviceModelExecutionOverride();
  if (model_path_override_switch) {
    auto file_path = StringToFilePath(*model_path_override_switch);
    if (file_path) {
      on_device_model_path_ = *file_path;
      on_device_model_execution_config_interpreter_->UpdateConfigWithFileDir(
          on_device_model_path_);
    }
  }
}

ModelExecutionManager::~ModelExecutionManager() = default;

void ModelExecutionManager::ExecuteModel(
    proto::ModelExecutionFeature feature,
    const google::protobuf::MessageLite& request_metadata,
    OptimizationGuideModelExecutionResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (active_model_execution_fetchers_.find(feature) !=
      active_model_execution_fetchers_.end()) {
    std::move(callback).Run(
        base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                ModelExecutionError::kGenericFailure)),
        nullptr);
    return;
  }

  // Set execution request in corresponding `log_ai_data_request`.
  auto log_ai_data_request = std::make_unique<proto::LogAiDataRequest>();
  SetExecutionRequest(feature, *log_ai_data_request.get(), request_metadata);

  auto fetcher_it = active_model_execution_fetchers_.emplace(
      std::piecewise_construct, std::forward_as_tuple(feature),
      std::forward_as_tuple(url_loader_factory_, model_execution_service_url_,
                            optimization_guide_logger_));
  fetcher_it.first->second.ExecuteModel(
      feature, identity_manager_, oauth_scopes_, request_metadata,
      base::BindOnce(&ModelExecutionManager::OnModelExecuteResponse,
                     weak_ptr_factory_.GetWeakPtr(), feature,
                     std::move(log_ai_data_request), std::move(callback)));
}

void ModelExecutionManager::OnModelExecuteResponse(
    proto::ModelExecutionFeature feature,
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
    OptimizationGuideModelExecutionResultCallback callback,
    base::expected<const proto::ExecuteResponse,
                   OptimizationGuideModelExecutionError> execute_response) {
  active_model_execution_fetchers_.erase(feature);
  if (!execute_response.has_value()) {
    std::move(callback).Run(base::unexpected(execute_response.error()),
                            nullptr);
    return;
  }
  if (!execute_response->has_response_metadata()) {
    std::move(callback).Run(
        base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                ModelExecutionError::kGenericFailure)),
        nullptr);
    return;
  }

  // Set execution response in corresponding `log_ai_data_request`.
  SetExecutionResponse(feature, *log_ai_data_request.get(),
                       execute_response->response_metadata());

  // Create corresponding log entry for `log_ai_data_request` to pass it with
  // the callback.
  // TODO(b/301301447): Send log entry when model execution was success but
  // contains allowed error status.
  std::unique_ptr<ModelQualityLogEntry> log_entry =
      std::make_unique<ModelQualityLogEntry>(std::move(log_ai_data_request));

  std::move(callback).Run(base::ok(execute_response->response_metadata()),
                          std::move(log_entry));
}

}  // namespace optimization_guide
