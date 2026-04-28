// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/storage/filter_store.h"

namespace multistep_filter {

namespace {

void LogServerRequestSent(MultistepFilterLogRouter* log_router,
                          int64_t navigation_id,
                          std::string_view domain,
                          size_t annotation_count) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerRequestSent, domain)
      << LogDetail{"annotation_count", static_cast<int>(annotation_count)};
}

void LogServerResponseReceived(MultistepFilterLogRouter* log_router,
                               int64_t navigation_id,
                               std::string_view domain,
                               size_t candidate_count) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseReceived, domain)
      << LogDetail{"candidate_count", static_cast<int>(candidate_count)};
}

void LogSuggestionSuppressed(MultistepFilterLogRouter* log_router,
                             int64_t navigation_id,
                             std::string_view domain,
                             std::string_view reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionSuppressed, domain)
      << LogDetail{"reason", std::string(reason)};
}

void LogSuggestionGenerated(MultistepFilterLogRouter* log_router,
                            int64_t navigation_id,
                            std::string_view domain,
                            const UrlFilterSuggestion& suggestion) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionGenerated, domain)
      << LogDetail{"suggestion", suggestion.ToString()};
}

void LogNoSupportedTasks(MultistepFilterLogRouter* log_router,
                         int64_t navigation_id,
                         std::string_view domain,
                         std::string_view reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kNoSupportedTasks, domain)
      << LogDetail{"reason", std::string(reason)};
}

void LogNoRelevantAnnotations(MultistepFilterLogRouter* log_router,
                              int64_t navigation_id,
                              std::string_view domain) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kNoRelevantAnnotations, domain);
}

}  // namespace

// TODO(crbug.com/483673955): Add telemetry for this class.
FilterSuggestionGenerator::FilterSuggestionGenerator(
    AnnotationIndexClient& annotation_index_client,
    FilterStore& filter_store,
    MultistepFilterLogRouter* log_router)
    : annotation_index_client_(annotation_index_client),
      filter_store_(filter_store),
      log_router_(log_router) {}

FilterSuggestionGenerator::~FilterSuggestionGenerator() = default;

void FilterSuggestionGenerator::GenerateSuggestion(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
    int64_t navigation_id,
    std::string_view domain) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  base::ScopedClosureRunner failure_callback(
      base::BindOnce(std::move(split_callback.first), std::nullopt));

  annotation_index_client_->GetSupportedTaskTypesForDomain(
      domain,
      base::BindOnce(
          &FilterSuggestionGenerator::OnSupportedTaskTypesFetched,
          weak_ptr_factory_.GetWeakPtr(), url, std::move(split_callback.second),
          std::move(failure_callback), navigation_id, std::string(domain)));
}

void FilterSuggestionGenerator::OnSupportedTaskTypesFetched(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
        success_callback,
    base::ScopedClosureRunner failure_callback,
    int64_t navigation_id,
    std::string_view domain,
    std::optional<std::vector<std::string>> supported_task_types) {
  if (!supported_task_types || supported_task_types->empty()) {
    LogNoSupportedTasks(log_router_, navigation_id, domain,
                        !supported_task_types ? "fetch_failed" : "empty_list");
    return;
  }

  // Fetch annotations for multiple task types asynchronously from the
  // FilterStore. The BarrierCallback waits until all these individual queries
  // complete before aggregating and processing them in
  // `OnAllAnnotationsFetched()`.
  auto barrier_callback = base::BarrierCallback<std::vector<FilterAnnotation>>(
      supported_task_types->size(),
      base::BindOnce(&FilterSuggestionGenerator::OnAllAnnotationsFetched,
                     weak_ptr_factory_.GetWeakPtr(), url,
                     std::move(success_callback), std::move(failure_callback),
                     navigation_id, std::string(domain)));

  // TODO(crbug.com/493485174): Filter supported task types to only include
  // filtering tasks.
  for (const std::string& task_type : *supported_task_types) {
    filter_store_->GetAnnotationsForTaskSortedByCreationTimestamp(
        task_type, barrier_callback);
  }
}

void FilterSuggestionGenerator::OnAllAnnotationsFetched(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
        success_callback,
    base::ScopedClosureRunner failure_callback,
    int64_t navigation_id,
    std::string_view domain,
    std::vector<std::vector<FilterAnnotation>> filter_annotations) {
  std::vector<FilterAnnotation> all_annotations;
  for (std::vector<FilterAnnotation>& annotations_for_task_type :
       filter_annotations) {
    base::Extend(all_annotations, std::move(annotations_for_task_type));
  }

  if (all_annotations.empty()) {
    LogNoRelevantAnnotations(log_router_, navigation_id, domain);
    return;
  }

  // Sort the aggregated list of annotations by `creation_timestamp` in
  // descending order to prioritize the most recently created annotations when
  // limiting the number of candidates.
  std::ranges::sort(all_annotations, std::ranges::greater(),
                    &FilterAnnotation::creation_timestamp);

  // Limit the number of candidates to bound the size of the payload sent to the
  // server.
  const size_t max_candidates = kMultistepFilterSuggestionMaxCandidates.Get();
  if (all_annotations.size() > max_candidates) {
    all_annotations.erase(all_annotations.begin() + max_candidates,
                          all_annotations.end());
  }

  LogServerRequestSent(log_router_, navigation_id, domain,
                       all_annotations.size());

  base::span<const FilterAnnotation> all_annotations_span = all_annotations;
  annotation_index_client_->GetFilterSuggestionCandidates(
      url, all_annotations_span,
      base::BindOnce(
          &FilterSuggestionGenerator::OnFilterSuggestionCandidatesFetched,
          weak_ptr_factory_.GetWeakPtr(), std::move(success_callback),
          std::move(failure_callback), std::move(all_annotations),
          navigation_id, std::string(domain)));
}

void FilterSuggestionGenerator::OnFilterSuggestionCandidatesFetched(
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
        success_callback,
    base::ScopedClosureRunner failure_callback,
    std::vector<FilterAnnotation> annotations,
    int64_t navigation_id,
    std::string_view domain,
    std::optional<std::vector<FilterSuggestionCandidate>> candidates) {
  LogServerResponseReceived(log_router_, navigation_id, domain,
                            candidates ? candidates->size() : 0);

  if (!candidates || candidates->empty()) {
    LogSuggestionSuppressed(log_router_, navigation_id, domain,
                            !candidates ? "fetch_failed" : "no_candidates");
    return;
  }
  // TODO(crbug.com/493511925): For the time being, the first candidate is
  // chosen by default. Implement the logic to select the best execution
  // candidate.
  FilterSuggestionCandidate& candidate = candidates->front();

  auto matching_annotation_it = std::ranges::find(
      annotations, candidate.filter_annotation_id, &FilterAnnotation::id);
  if (matching_annotation_it == annotations.end()) {
    LogSuggestionSuppressed(log_router_, navigation_id, domain,
                            "annotation_not_found");
    return;
  }

  std::vector<FilterAttributeUiLabel> attribute_ui_labels;
  for (FilterSuggestionCandidateAttribute& candidate_attribute :
       candidate.attributes) {
    auto it = std::ranges::find_if(
        matching_annotation_it->attributes,
        [&](const FilterAttribute& annotation_attribute) {
          return annotation_attribute.key == candidate_attribute.key;
        });
    if (it != matching_annotation_it->attributes.end()) {
      attribute_ui_labels.emplace_back(std::move(candidate_attribute),
                                       std::move(*it));
    }
  }

  // Suggestion generation succeeded, reset `failure_callback` as to not notify
  // otherwise.
  failure_callback.ReplaceClosure(base::DoNothing());

  UrlFilterSuggestion suggestion(
      std::move(candidate.navigation_url),
      base::UTF8ToUTF16(matching_annotation_it->source_domain),
      matching_annotation_it->creation_timestamp,
      std::move(attribute_ui_labels));
  LogSuggestionGenerated(log_router_, navigation_id, domain, suggestion);
  std::move(success_callback).Run(std::move(suggestion));
}

}  // namespace multistep_filter
