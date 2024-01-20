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
#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
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

void RecordSessionUsedRemoteExecutionHistogram(
    proto::ModelExecutionFeature feature,
    bool is_remote) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.SessionUsedRemoteExecution.",
           GetStringNameForModelExecutionFeature(feature)}),
      is_remote);
}

void RecordModelExecutionResultHistogram(proto::ModelExecutionFeature feature,
                                         bool result) {
  base::UmaHistogramBoolean(
      base::StrCat({"OptimizationGuide.ModelExecution.Result.",
                    GetStringNameForModelExecutionFeature(feature)}),
      result);
}

}  // namespace

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

ModelExecutionManager::ModelExecutionManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    scoped_refptr<OnDeviceModelServiceController>
        on_device_model_service_controller,
    OptimizationGuideLogger* optimization_guide_logger)
    : optimization_guide_logger_(optimization_guide_logger),
      model_execution_service_url_(net::AppendOrReplaceQueryParameter(
          GetModelExecutionServiceURL(),
          "key",
          features::GetOptimizationGuideServiceAPIKey())),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      on_device_model_service_controller_(
          std::move(on_device_model_service_controller)) {}

ModelExecutionManager::~ModelExecutionManager() = default;

void ModelExecutionManager::ExecuteModelWithStreaming(
    proto::ModelExecutionFeature feature,
    const google::protobuf::MessageLite& request_metadata,
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
    OptimizationGuideModelExecutionResultStreamingCallback callback) {
  ExecuteModel(
      feature, request_metadata, std::move(log_ai_data_request),
      base::BindOnce(
          [](OptimizationGuideModelExecutionResultStreamingCallback callback,
             OptimizationGuideModelExecutionResult result,
             std::unique_ptr<ModelQualityLogEntry> log_entry) {
            if (result.has_value()) {
              callback.Run(
                  StreamingResponse{
                      .response = *result,
                      .is_complete = true,
                  },
                  std::move(log_entry));
            } else {
              callback.Run(base::unexpected(result.error()),
                           std::move(log_entry));
            }
          },
          callback));
}

void ModelExecutionManager::ExecuteModel(
    proto::ModelExecutionFeature feature,
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
        << "ExecuteModel: " << proto::ModelExecutionFeature_Name(feature);
    switch (feature) {
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

std::unique_ptr<OptimizationGuideModelExecutor::Session>
ModelExecutionManager::StartSession(proto::ModelExecutionFeature feature) {
  ExecuteRemoteFn execute_fn =
      base::BindRepeating(&ModelExecutionManager::ExecuteModelWithStreaming,
                          base::Unretained(this));
  if (on_device_model_service_controller_) {
    auto session = on_device_model_service_controller_->CreateSession(
        feature, execute_fn, optimization_guide_logger_.get());
    if (session) {
      RecordSessionUsedRemoteExecutionHistogram(feature, /*is_remote=*/false);
      return session;
    }
  }

  RecordSessionUsedRemoteExecutionHistogram(feature, /*is_remote=*/true);
  return std::make_unique<SessionImpl>(base::DoNothing(), feature, nullptr,
                                       nullptr, std::move(execute_fn),
                                       optimization_guide_logger_.get());
}

void ModelExecutionManager::OnModelExecuteResponse(
    proto::ModelExecutionFeature feature,
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
    OptimizationGuideModelExecutionResultCallback callback,
    base::expected<const proto::ExecuteResponse,
                   OptimizationGuideModelExecutionError> execute_response) {
  active_model_execution_fetchers_.erase(feature);
  ScopedModelExecutionResponseLogger scoped_logger(feature,
                                                   optimization_guide_logger_);
  if (!execute_response.has_value()) {
    scoped_logger.set_message("Error: No Response");
    RecordModelExecutionResultHistogram(feature, false);
    std::move(callback).Run(base::unexpected(execute_response.error()),
                            nullptr);
    return;
  }

  // Create corresponding log entry for `log_ai_data_request` to pass it with
  // the callback.
  std::unique_ptr<ModelQualityLogEntry> log_entry =
      std::make_unique<ModelQualityLogEntry>(std::move(log_ai_data_request));

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
    if (!error.ShouldLogModelQuality()) {
      log_entry = nullptr;
    }
    RecordModelExecutionResultHistogram(feature, false);
    base::UmaHistogramEnumeration(
        base::StrCat({"OptimizationGuide.ModelExecution.ServerError.",
                      GetStringNameForModelExecutionFeature(feature)}),
        error.error());
    std::move(callback).Run(base::unexpected(error), std::move(log_entry));
    return;
  }

  if (!execute_response->has_response_metadata()) {
    scoped_logger.set_message("Error: No Response Metadata");
    RecordModelExecutionResultHistogram(feature, false);
    // Log the request in case response is not present by passing the
    // `log_entry`.
    std::move(callback).Run(
        base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                ModelExecutionError::kGenericFailure)),
        std::move(log_entry));
    return;
  }

  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_logger_)
        << "ExecuteModel Response: "
        << proto::ModelExecutionFeature_Name(feature);
    switch (feature) {
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

}  // namespace optimization_guide
