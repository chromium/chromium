// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/optimization_guide_annotation_index_client.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_conversion_util.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

using ::optimization_guide::OptimizationGuideDecider;
using ::optimization_guide::OptimizationGuideDecision;
using ::optimization_guide::OptimizationMetadata;
using ::optimization_guide::proto::OptimizationType;
using ::optimization_guide::proto::OptimizationType_Name;

void LogServerRequestFailed(MultistepFilterLogRouter* log_router,
                            int64_t navigation_id,
                            std::string_view host,
                            std::string_view failure_reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerRequestFailed, host)
      << LogDetail("failure_reason", std::string(failure_reason));
}

void LogServerRequestSentFilterTasksSupported(
    MultistepFilterLogRouter* log_router,
    int64_t navigation_id,
    std::string_view host) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerRequestSent, host)
      << LogDetail(
             "request_type",
             OptimizationType_Name(OptimizationType::FILTER_TASKS_SUPPORTED));
}

void LogServerRequestSentFilterExtractAttributes(
    MultistepFilterLogRouter* log_router,
    int64_t navigation_id,
    const GURL& url) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerRequestSent, url.host())
      << LogDetail(
             "request_type",
             OptimizationType_Name(OptimizationType::FILTER_EXTRACT_ATTRIBUTES))
      << LogDetail("source_raw_url", url.spec());
}

void LogServerResponseReceived(MultistepFilterLogRouter* log_router,
                               int64_t navigation_id,
                               std::string_view host,
                               bool is_success) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseReceived, host)
      << LogDetail("is_success", is_success);
}

void LogServerResponseReceivedWithSupportedTasks(
    MultistepFilterLogRouter* log_router,
    int64_t navigation_id,
    std::string_view host,
    base::span<const std::string> supported_tasks) {
  std::string supported_tasks_str;
  int supported_tasks_count = 0;
  if (!supported_tasks.empty()) {
    supported_tasks_str =
        base::StrCat({"[", base::JoinString(supported_tasks, ", "), "]"});
    supported_tasks_count = static_cast<int>(supported_tasks.size());
  }

  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseReceived, host)
      << LogDetail("is_success", true)
      << LogDetail("supported_tasks_count", supported_tasks_count)
      << LogDetail("supported_tasks", supported_tasks_str);
}

void LogServerResponseReceivedWithFilterAnnotation(
    MultistepFilterLogRouter* log_router,
    int64_t navigation_id,
    std::string_view host,
    const std::optional<FilterAnnotation>& result) {
  std::string extracted_annotation_str;
  int extracted_attributes_count = 0;
  if (result.has_value()) {
    extracted_annotation_str = result->ToString();
    extracted_attributes_count = static_cast<int>(result->attributes.size());
  }

  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseReceived, host)
      << LogDetail("is_success", true)
      << LogDetail("extracted_attributes_count", extracted_attributes_count)
      << LogDetail("extracted_annotation", extracted_annotation_str);
}

void LogServerResponseMalformed(MultistepFilterLogRouter* log_router,
                                int64_t navigation_id,
                                std::string_view host,
                                std::string_view failure_reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseMalformed, host)
      << LogDetail("failure_reason", std::string(failure_reason));
}

}  // namespace

// static
std::unique_ptr<OptimizationGuideAnnotationIndexClient>
OptimizationGuideAnnotationIndexClient::Create(
    OptimizationGuideDecider* optimization_guide_decider,
    MultistepFilterLogRouter* log_router) {
  return std::make_unique<OptimizationGuideAnnotationIndexClient>(
      optimization_guide_decider, log_router);
}

OptimizationGuideAnnotationIndexClient::OptimizationGuideAnnotationIndexClient(
    OptimizationGuideDecider* optimization_guide_decider,
    MultistepFilterLogRouter* log_router)
    : optimization_guide_decider_(optimization_guide_decider),
      log_router_(log_router) {
  RegisterOptimizationTypes();
}

OptimizationGuideAnnotationIndexClient::
    ~OptimizationGuideAnnotationIndexClient() = default;

void OptimizationGuideAnnotationIndexClient::GetFilterSuggestionCandidates(
    const GURL& url,
    base::span<const FilterAnnotation> filter_annotations,
    base::OnceCallback<
        void(std::optional<std::vector<FilterSuggestionCandidate>>)> callback,
    int64_t navigation_id) {
  // TODO(crbug.com/522751288): Implement this method.
  std::move(callback).Run(std::nullopt);
}

void OptimizationGuideAnnotationIndexClient::GetSupportedTasks(
    const GURL& url,
    base::OnceCallback<void(std::vector<std::string>)> callback,
    int64_t navigation_id) {
  if (!optimization_guide_decider_) {
    LogServerRequestFailed(log_router_, navigation_id, url.host(),
                           "optimization_guide_decider_null");
    std::move(callback).Run({});
    return;
  }

  LogServerRequestSentFilterTasksSupported(log_router_, navigation_id,
                                           url.host());

  optimization_guide_decider_->CanApplyOptimization(
      url, OptimizationType::FILTER_TASKS_SUPPORTED,
      base::BindOnce(&OptimizationGuideAnnotationIndexClient::
                         OnFilterTasksSupportedDecision,
                     weak_ptr_factory_.GetWeakPtr(), url, navigation_id,
                     std::move(callback)));
}

void OptimizationGuideAnnotationIndexClient::ExtractFilterAnnotation(
    const GURL& url,
    base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
    int64_t navigation_id) {
  if (!optimization_guide_decider_) {
    LogServerRequestFailed(log_router_, navigation_id, url.host(),
                           "optimization_guide_decider_null");
    std::move(callback).Run(std::nullopt);
    return;
  }

  LogServerRequestSentFilterExtractAttributes(log_router_, navigation_id, url);

  optimization_guide_decider_->CanApplyOptimization(
      url, OptimizationType::FILTER_EXTRACT_ATTRIBUTES,
      base::BindOnce(&OptimizationGuideAnnotationIndexClient::
                         OnFilterExtractAttributesDecision,
                     weak_ptr_factory_.GetWeakPtr(), url, navigation_id,
                     std::move(callback)));
}

void OptimizationGuideAnnotationIndexClient::OnFilterTasksSupportedDecision(
    const GURL& url,
    int64_t navigation_id,
    base::OnceCallback<void(std::vector<std::string>)> callback,
    OptimizationGuideDecision decision,
    const OptimizationMetadata& metadata) {
  const bool is_success = decision == OptimizationGuideDecision::kTrue;
  if (!is_success || !metadata.any_metadata().has_value()) {
    LogServerResponseReceived(log_router_, navigation_id, url.host(),
                              is_success);
    std::move(callback).Run({});
    return;
  }

  std::optional<GetSupportedTasksResponse> response =
      metadata.ParsedMetadata<GetSupportedTasksResponse>();
  if (!response) {
    LogServerResponseMalformed(log_router_, navigation_id, url.host(),
                               "parsing_failed");
    std::move(callback).Run({});
    return;
  }

  std::vector<std::string> supported_tasks = ToSupportedTasks(*response);
  LogServerResponseReceivedWithSupportedTasks(
      log_router_, navigation_id, url.host(),
      supported_tasks);

  std::move(callback).Run(std::move(supported_tasks));
}

void OptimizationGuideAnnotationIndexClient::OnFilterExtractAttributesDecision(
    const GURL& url,
    int64_t navigation_id,
    base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
    OptimizationGuideDecision decision,
    const OptimizationMetadata& metadata) {
  bool is_success = decision == OptimizationGuideDecision::kTrue;
  if (!is_success || !metadata.any_metadata().has_value()) {
    LogServerResponseReceived(log_router_, navigation_id, url.host(),
                              is_success);
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<ExtractTaskAttributesResponse> response =
      metadata.ParsedMetadata<ExtractTaskAttributesResponse>();
  if (!response) {
    LogServerResponseMalformed(log_router_, navigation_id, url.host(),
                               "parsing_failed");
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<FilterAnnotation> annotation =
      ToFilterAnnotation(url, *response);
  LogServerResponseReceivedWithFilterAnnotation(log_router_, navigation_id,
                                                url.host(), annotation);
  std::move(callback).Run(std::move(annotation));
}

void OptimizationGuideAnnotationIndexClient::RegisterOptimizationTypes() {
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {OptimizationType::FILTER_TASKS_SUPPORTED,
         OptimizationType::FILTER_EXTRACT_ATTRIBUTES});
  }
}

}  // namespace multistep_filter
