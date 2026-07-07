// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/optimization_guide_annotation_index_client.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/map_util.h"
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
using ::optimization_guide::OptimizationGuideDecisionWithMetadata;
using ::optimization_guide::OptimizationMetadata;
using ::optimization_guide::proto::OptimizationType;
using ::optimization_guide::proto::OptimizationType_Name;
using ::optimization_guide::proto::RequestContext;
using ::optimization_guide::proto::RequestContextMetadata;

void LogServerRequestFailed(MultistepFilterLogRouter* log_router,
                            int64_t navigation_id,
                            std::string_view host,
                            std::string_view failure_reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerRequestFailed, host)
      << LogDetail("failure_reason", std::string(failure_reason));
}

void LogServerRequestSentWithFilterExecutionStrategy(
    MultistepFilterLogRouter* log_router,
    int64_t navigation_id,
    std::string_view host,
    base::span<const FilterAnnotation> filter_annotations) {
  std::vector<std::string> annotation_strings;
  annotation_strings.reserve(filter_annotations.size());
  for (const FilterAnnotation& annotation : filter_annotations) {
    annotation_strings.push_back(annotation.ToString());
  }
  std::string filter_annotations_str =
      base::StrCat({"[", base::JoinString(annotation_strings, ", "), "]"});

  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerRequestSent, host)
      << LogDetail("request_type",
                   std::string(OptimizationType_Name(
                       OptimizationType::FILTER_EXECUTION_STRATEGY)))
      << LogDetail("execution_candidate_count",
                   static_cast<int>(filter_annotations.size()))
      << LogDetail("execution_candidates", filter_annotations_str);
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

void LogServerResponseReceivedWithFilterExecutionStrategy(
    MultistepFilterLogRouter* log_router,
    int64_t navigation_id,
    std::string_view host,
    const std::optional<std::vector<FilterSuggestionCandidate>>& result) {
  std::string candidates_str;
  int candidates_count = 0;
  if (result.has_value()) {
    std::vector<std::string> candidate_strings;
    candidate_strings.reserve(result->size());
    for (const FilterSuggestionCandidate& candidate : *result) {
      candidate_strings.push_back(candidate.ToString());
    }
    candidates_str =
        base::StrCat({"[", base::JoinString(candidate_strings, ", "), "]"});
    candidates_count = static_cast<int>(result->size());
  }

  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseReceived, host)
      << LogDetail("is_success", true)
      << LogDetail("filter_suggestion_candidates_count", candidates_count)
      << LogDetail("filter_suggestion_candidates", candidates_str);
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
  if (!optimization_guide_decider_) {
    LogServerRequestFailed(log_router_, navigation_id, url.host(),
                           "optimization_guide_decider_null");
    std::move(callback).Run(std::nullopt);
    return;
  }

  RequestContextMetadata context_metadata =
      ToRequestContextMetadata(filter_annotations);

  LogServerRequestSentWithFilterExecutionStrategy(
      log_router_, navigation_id, url.host(), filter_annotations);

  auto shared_callback =
      base::MakeRefCounted<base::RefCountedData<base::OnceCallback<void(
          std::optional<std::vector<FilterSuggestionCandidate>>)>>>(
          std::move(callback));

  optimization_guide_decider_->CanApplyOptimizationOnDemand(
      {url}, {OptimizationType::FILTER_EXECUTION_STRATEGY},
      RequestContext::CONTEXT_FILTER_EXECUTION,
      base::BindRepeating(&OptimizationGuideAnnotationIndexClient::
                              OnFilterExecutionStrategyDecision,
                          weak_ptr_factory_.GetWeakPtr(), navigation_id,
                          shared_callback),
      context_metadata);
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

void OptimizationGuideAnnotationIndexClient::OnFilterExecutionStrategyDecision(
    int64_t navigation_id,
    scoped_refptr<base::RefCountedData<base::OnceCallback<
        void(std::optional<std::vector<FilterSuggestionCandidate>>)>>>
        shared_callback,
    const GURL& url,
    const base::flat_map<
        optimization_guide::proto::OptimizationType,
        optimization_guide::OptimizationGuideDecisionWithMetadata>& decisions) {
  if (shared_callback->data.is_null()) {
    LogServerResponseReceived(log_router_, navigation_id, url.host(),
                              /*is_success=*/false);
    return;
  }

  const auto* decision_with_metadata =
      base::FindOrNull(decisions, OptimizationType::FILTER_EXECUTION_STRATEGY);
  if (!decision_with_metadata) {
    LogServerResponseReceived(log_router_, navigation_id, url.host(),
                              /*is_success=*/false);
    std::move(shared_callback->data).Run(std::nullopt);
    return;
  }

  OptimizationMetadata metadata = decision_with_metadata->metadata;
  const bool is_success =
      decision_with_metadata->decision == OptimizationGuideDecision::kTrue;
  if (!is_success || !metadata.any_metadata().has_value()) {
    LogServerResponseReceived(log_router_, navigation_id, url.host(),
                              is_success);
    std::move(shared_callback->data).Run(std::nullopt);
    return;
  }

  std::optional<GetTaskExecutionStrategiesResponse> response =
      metadata.ParsedMetadata<GetTaskExecutionStrategiesResponse>();
  if (!response) {
    LogServerResponseMalformed(log_router_, navigation_id, url.host(),
                               "parsing_failed");
    std::move(shared_callback->data).Run(std::nullopt);
    return;
  }

  std::vector<FilterSuggestionCandidate> candidates =
      ToFilterSuggestionCandidates(*response);
  LogServerResponseReceivedWithFilterExecutionStrategy(
      log_router_, navigation_id, url.host(),
      std::optional<std::vector<FilterSuggestionCandidate>>(candidates));
  std::move(shared_callback->data).Run(std::move(candidates));
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
  LogServerResponseReceivedWithSupportedTasks(log_router_, navigation_id,
                                              url.host(), supported_tasks);

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
