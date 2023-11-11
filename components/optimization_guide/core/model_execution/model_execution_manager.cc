// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_stream_receiver.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

namespace {

class ScopedModelExecutionResponseLogger {
 public:
  ScopedModelExecutionResponseLogger(
      proto::ModelExecutionFeature feature,
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
        << "OnModelExecutionResponse - Feature : "
        << proto::ModelExecutionFeature_Name(feature_) << " " << message_;
  }

  void set_message(const std::string& message) { message_ = message; }

 private:
  proto::ModelExecutionFeature feature_;
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

  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_logger_)
        << "ExecuteModel: " << proto::ModelExecutionFeature_Name(feature);
    switch (feature) {
      case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED: {
        break;
      }
      case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE: {
        // TOOD(b/309486492): Add logging for request/response for compose.
        break;
      }
      case proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION: {
        proto::Any any;
        any.set_type_url(request_metadata.GetTypeName());
        request_metadata.SerializeToString(any.mutable_value());
        auto tab_request = optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::TabOrganizationRequest>(any);
        std::string titles = "";
        for (const auto& tab : tab_request->tabs()) {
          titles += base::StringPrintf("%s\"%s\"", titles.empty() ? "" : ",",
                                       tab.title().c_str());
        }
        OPTIMIZATION_GUIDE_LOGGER(
            optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
            optimization_guide_logger_)
            << "TabOrganization Request: "
            << base::StringPrintf("{\"titles\" : [%s]}", titles.c_str());

        break;
      }
      case proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH: {
        // TOOD(b/309486807): Add logging for request/response for wallpapers.
        break;
      }
    }
  }

  auto fetcher_it = active_model_execution_fetchers_.emplace(
      std::piecewise_construct, std::forward_as_tuple(feature),
      std::forward_as_tuple(url_loader_factory_, model_execution_service_url_,
                            optimization_guide_logger_));
  fetcher_it.first->second.ExecuteModel(
      feature, identity_manager_, oauth_scopes_, request_metadata,
      base::BindOnce(&ModelExecutionManager::OnModelExecuteResponse,
                     weak_ptr_factory_.GetWeakPtr(), feature,
                     std::move(callback)));
}

void ModelExecutionManager::OnModelExecuteResponse(
    proto::ModelExecutionFeature feature,
    OptimizationGuideModelExecutionResultCallback callback,
    base::expected<const proto::ExecuteResponse,
                   OptimizationGuideModelExecutionError> execute_response) {
  active_model_execution_fetchers_.erase(feature);
  ScopedModelExecutionResponseLogger scoped_logger(feature,
                                                   optimization_guide_logger_);
  if (!execute_response.has_value()) {
    scoped_logger.set_message("Error: No Response");
    std::move(callback).Run(base::unexpected(execute_response.error()),
                            nullptr);
    return;
  }

  if (execute_response->has_error_message()) {
    scoped_logger.set_message(base::StringPrintf(
        "Error: %s", execute_response->error_message().c_str()));
  }

  if (!execute_response->has_response_metadata()) {
    scoped_logger.set_message("Error: No Response Metadata");
    std::move(callback).Run(
        base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                ModelExecutionError::kGenericFailure)),
        nullptr);
    return;
  }

  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    switch (feature) {
      case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED: {
        break;
      }
      case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE: {
        // TOOD(b/309486492): Add logging for request/response for compose.
        break;
      }
      case proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION: {
        std::string message = "";
        auto tab_response = optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::TabOrganizationResponse>(
            execute_response->response_metadata());
        message += "Response: [";
        int group_cnt = 0;
        for (const auto& tab_organization : tab_response->tab_organizations()) {
          std::string tab_titles = "";
          for (const auto& tab : tab_organization.tabs()) {
            tab_titles +=
                base::StringPrintf("%s\" %s \"", tab_titles.empty() ? "" : ",",
                                   tab.title().c_str());
          }
          message += base::StringPrintf(
              "%s{"
              "\"label\": \"%s\", "
              "\"tabs\": [%s] }",
              group_cnt > 0 ? "," : "", tab_organization.label().c_str(),
              tab_titles.c_str());
          group_cnt += 1;
        }
        message += "]";
        scoped_logger.set_message(message);
        break;
      }
      case proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH: {
        // TOOD(b/309486807): Add logging for request/response for wallpapers.
        break;
      }
    }
  }
  std::move(callback).Run(base::ok(execute_response->response_metadata()),
                          nullptr);
}

}  // namespace optimization_guide
