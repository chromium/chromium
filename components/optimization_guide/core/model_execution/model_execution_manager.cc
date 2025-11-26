// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include <optional>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_fetcher_impl.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
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

void RecordModelExecutionResultHistogram(ModelBasedCapabilityKey feature,
                                         bool result) {
  base::UmaHistogramBoolean(
      base::StrCat({"OptimizationGuide.ModelExecution.Result.",
                    GetStringNameForModelExecutionFeature(feature)}),
      result);
}

// The maximum number of parallel `ExecuteModel()` calls allowed for the
// `feature`. Must be at least 1.
// If a new model execution request exceeds this limited, the oldest pending
// execution is cancelled.
size_t GetMaxParallelFeatureExecutions(ModelBasedCapabilityKey feature) {
  switch (feature) {
    case ModelBasedCapabilityKey::kCompose:
    case ModelBasedCapabilityKey::kTabOrganization:
    case ModelBasedCapabilityKey::kWallpaperSearch:
    case ModelBasedCapabilityKey::kTest:
    case ModelBasedCapabilityKey::kHistorySearch:
    case ModelBasedCapabilityKey::kBlingPrototyping:
    case ModelBasedCapabilityKey::kPasswordChangeSubmission:
    case ModelBasedCapabilityKey::kEnhancedCalendar:
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
    case ModelBasedCapabilityKey::kWalletablePassExtraction:
    case ModelBasedCapabilityKey::kAmountExtraction:
    case ModelBasedCapabilityKey::kIosSmartTabGrouping:
      return 1;
    case ModelBasedCapabilityKey::kFormsClassifications:
      // Since there can be multiple forms on a single page, multiple parallel
      // executions are allowed for `kFormsClassifications`.
      return 10;
  }
}

}  // namespace

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

ModelExecutionManager::ModelExecutionManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<Delegate> delegate,
    OptimizationGuideLogger* optimization_guide_logger,
    base::WeakPtr<ModelQualityLogsUploaderService>
        model_quality_uploader_service)
    : model_quality_uploader_service_(model_quality_uploader_service),
      optimization_guide_logger_(optimization_guide_logger),
      model_execution_service_url_(net::AppendOrReplaceQueryParameter(
          switches::GetModelExecutionServiceURL(),
          "key",
          features::GetOptimizationGuideServiceAPIKey())),
      delegate_(std::move(delegate)),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {}

ModelExecutionManager::~ModelExecutionManager() = default;

void ModelExecutionManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Invalidate the weak pointers before clearing the active fetchers, which
  // will cause the drop all the model execution consumer callbacks, and avoid
  // all processing during destructor.
  weak_ptr_factory_.InvalidateWeakPtrs();
  active_model_execution_fetchers_.clear();
}

void ModelExecutionManager::AddExecutionResultForTesting(
    ModelBasedCapabilityKey feature,
    OptimizationGuideModelExecutionResult result) {
  test_execution_results_.insert({feature, std::move(result)});
}

void ModelExecutionManager::ExecuteModel(
    ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite& request_metadata,
    std::optional<base::TimeDelta> timeout,
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
    ModelExecutionServiceType service_type,
    OptimizationGuideModelExecutionResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (test_execution_results_.find(feature) != test_execution_results_.end()) {
    std::move(callback).Run(std::move(test_execution_results_[feature]),
                            nullptr);
    test_execution_results_.erase(feature);
    return;
  }

  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_logger_)
        << "ExecuteModel: " << ProtoName(feature);
    switch (feature) {
      case ModelBasedCapabilityKey::kTabOrganization: {
        proto::Any any = AnyWrapProto(request_metadata);
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

  ActiveFeatureExecutions& fetchers_for_feature =
      active_model_execution_fetchers_[feature];
  if (fetchers_for_feature.size() == GetMaxParallelFeatureExecutions(feature)) {
    // Cancel the fetcher with the smallest ID. Since IDs are assigned in
    // increasing order, this cancels the oldest one.
    fetchers_for_feature.erase(fetchers_for_feature.begin());
  }
  FetcherId fetcher_id = next_model_execution_fetcher_id++;
  // Currently only ZSS is supported by legion. Update or remove this CHECK when
  // other features are supported too.
  CHECK(service_type != ModelExecutionServiceType::kLegion ||
        feature == ModelBasedCapabilityKey::kZeroStateSuggestions)
      << feature;
  auto fetcher_it = fetchers_for_feature.emplace(
      fetcher_id, CreateModelExecutionFetcher(service_type));
  fetcher_it.first->second->ExecuteModel(
      feature, identity_manager_, request_metadata, timeout,
      base::BindOnce(&ModelExecutionManager::OnModelExecuteResponse,
                     weak_ptr_factory_.GetWeakPtr(), feature, fetcher_id,
                     std::move(log_ai_data_request), std::move(callback)));
}

std::unique_ptr<ModelExecutionFetcher>
ModelExecutionManager::CreateModelExecutionFetcher(
    ModelExecutionServiceType service_type) {
  switch (service_type) {
    case ModelExecutionServiceType::kDefault:
      return std::make_unique<ModelExecutionFetcherImpl>(
          url_loader_factory_, model_execution_service_url_,
          optimization_guide_logger_);
    case ModelExecutionServiceType::kLegion:
      CHECK(delegate_);
      return delegate_->CreateLegionFetcher();
  }
}

void ModelExecutionManager::OnModelExecuteResponse(
    ModelBasedCapabilityKey feature,
    FetcherId fetcher_id,
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
    OptimizationGuideModelExecutionResultCallback callback,
    base::expected<const proto::ExecuteResponse,
                   OptimizationGuideModelExecutionError> execute_response) {
  active_model_execution_fetchers_[feature].erase(fetcher_id);
  ScopedModelExecutionResponseLogger scoped_logger(feature,
                                                   optimization_guide_logger_);

  auto execution_info = std::make_unique<proto::ModelExecutionInfo>(
      log_ai_data_request->model_execution_info());
  // TODO(372535824): don't create a ModelQualityLogEntry here, just use
  // ModelExecutionInfo.
  // Create corresponding log entry for `log_ai_data_request` to pass it with
  // the callback.
  std::unique_ptr<ModelQualityLogEntry> log_entry =
      std::make_unique<ModelQualityLogEntry>(model_quality_uploader_service_);
  log_entry->log_ai_data_request()->MergeFrom(*log_ai_data_request);

  if (!execute_response.has_value()) {
    scoped_logger.set_message("Error: No Response");
    RecordModelExecutionResultHistogram(feature, false);
    auto error = execute_response.error();
    execution_info->set_model_execution_error_enum(
        static_cast<uint32_t>(error.error()));
    log_entry->set_model_execution_error(error);
    std::move(callback).Run(
        OptimizationGuideModelExecutionResult(base::unexpected(error),
                                              std::move(execution_info)),
        std::move(log_entry));
    return;
  }

  // Set the id if present.
  if (execute_response->has_server_execution_id()) {
    execution_info->set_execution_id(execute_response->server_execution_id());
    log_entry->set_model_execution_id(execute_response->server_execution_id());
  }

  if (execute_response->has_error_response()) {
    scoped_logger.set_message("Error: No Response Metadata");
    log_entry->set_error_response(execute_response->error_response());
    *execution_info->mutable_error_response() =
        execute_response->error_response();
    // For unallowed error states, don't log request data.
    auto error =
        OptimizationGuideModelExecutionError::FromModelExecutionServerError(
            execute_response->error_response());
    RecordModelExecutionResultHistogram(feature, false);
    base::UmaHistogramEnumeration(
        base::StrCat({"OptimizationGuide.ModelExecution.ServerError.",
                      GetStringNameForModelExecutionFeature(feature)}),
        error.error());
    log_entry->set_model_execution_error(error);
    execution_info->set_model_execution_error_enum(
        static_cast<uint32_t>(error.error()));

    if (!error.ShouldLogModelQuality()) {
      log_entry = nullptr;
      execution_info = nullptr;
    }
    std::move(callback).Run(
        OptimizationGuideModelExecutionResult(base::unexpected(error),
                                              std::move(execution_info)),
        std::move(log_entry));
    return;
  }

  if (!execute_response->has_response_metadata()) {
    scoped_logger.set_message("Error: No Response Metadata");
    RecordModelExecutionResultHistogram(feature, false);
    auto error = OptimizationGuideModelExecutionError::FromModelExecutionError(
        ModelExecutionError::kGenericFailure);
    log_entry->set_model_execution_error(error);
    execution_info->set_model_execution_error_enum(
        static_cast<uint32_t>(error.error()));
    // Log the request in case response is not present by passing the
    // `execution_info`.
    std::move(callback).Run(
        OptimizationGuideModelExecutionResult(base::unexpected(error),
                                              std::move(execution_info)),
        std::move(log_entry));
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

  RecordModelExecutionResultHistogram(feature, true);
  std::move(callback).Run(OptimizationGuideModelExecutionResult(
                              base::ok(execute_response->response_metadata()),
                              std::move(execution_info)),
                          std::move(log_entry));
}

}  // namespace optimization_guide
