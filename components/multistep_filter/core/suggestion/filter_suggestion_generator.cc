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

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/switches.h"
#include "url/url_constants.h"

namespace multistep_filter {

namespace {

void LogServerRequestSent(MultistepFilterLogRouter* log_router,
                          int64_t navigation_id,
                          std::string_view host,
                          size_t annotation_count) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerRequestSent, host)
      << LogDetail{"annotation_count", static_cast<int>(annotation_count)};
}

void LogServerResponseReceived(MultistepFilterLogRouter* log_router,
                               int64_t navigation_id,
                               std::string_view host,
                               size_t candidate_count) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseReceived, host)
      << LogDetail{"candidate_count", static_cast<int>(candidate_count)};
}

void LogSuggestionSuppressed(MultistepFilterLogRouter* log_router,
                             int64_t navigation_id,
                             std::string_view host,
                             std::string_view reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionSuppressed, host)
      << LogDetail{"reason", std::string(reason)};
}

void LogSuggestionGenerated(MultistepFilterLogRouter* log_router,
                            int64_t navigation_id,
                            std::string_view host,
                            const UrlFilterSuggestion& suggestion) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionGenerated, host)
      << LogDetail{"valid", true}
      << LogDetail{"filters_count",
                   static_cast<int>(suggestion.attribute_ui_labels.size())}
      << LogDetail{"suggestion", suggestion.ToString()};
}


void LogNoRelevantAnnotations(MultistepFilterLogRouter* log_router,
                              int64_t navigation_id,
                              std::string_view host) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kNoRelevantAnnotations, host);
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
    std::vector<std::string> supported_task_types,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
    int64_t navigation_id) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  base::ScopedClosureRunner failure_callback(
      base::BindOnce(std::move(split_callback.first), std::nullopt));

  const base::Time min_creation_time =
      base::Time::Now() - kMultistepFilterSessionDuration.Get();
  // TODO(crbug.com/493485174): Filter supported task types to only include
  // filtering tasks.
  filter_store_->GetAnnotationsForTasksSortedByCreationTimestamp(
      std::move(supported_task_types),
      base::BindOnce(
          &FilterSuggestionGenerator::OnAllAnnotationsFetched,
          weak_ptr_factory_.GetWeakPtr(), url, std::move(split_callback.second),
          std::move(failure_callback), navigation_id),
      kMultistepFilterSuggestionMaxCandidates.Get(), min_creation_time);
}

void FilterSuggestionGenerator::OnAllAnnotationsFetched(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
        success_callback,
    base::ScopedClosureRunner failure_callback,
    int64_t navigation_id,
    std::vector<FilterAnnotation> all_annotations) {

  if (all_annotations.empty()) {
    LogNoRelevantAnnotations(log_router_, navigation_id, url.GetHost());
    return;
  }

  // Suppress suggestions if the latest annotation is for the same domain and
  // within the throttle duration.
  if (!all_annotations.empty() &&
      all_annotations.front().source_host == url.GetHost() &&
      base::Time::Now() - all_annotations.front().creation_timestamp <
          kSameDomainSuggestionSuppressionDuration.Get()) {
    LogSuggestionSuppressed(log_router_, navigation_id, url.GetHost(),
                            "recent_extraction");
    return;
  }

  LogServerRequestSent(log_router_, navigation_id, url.GetHost(),
                       all_annotations.size());

  base::span<const FilterAnnotation> all_annotations_span = all_annotations;
  annotation_index_client_->GetFilterSuggestionCandidates(
      url, all_annotations_span,
      base::BindOnce(
          &FilterSuggestionGenerator::OnFilterSuggestionCandidatesFetched,
          weak_ptr_factory_.GetWeakPtr(), url, std::move(success_callback),
          std::move(failure_callback), std::move(all_annotations),
          navigation_id),
      navigation_id);
}

void FilterSuggestionGenerator::OnFilterSuggestionCandidatesFetched(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
        success_callback,
    base::ScopedClosureRunner failure_callback,
    std::vector<FilterAnnotation> annotations,
    int64_t navigation_id,
    std::optional<std::vector<FilterSuggestionCandidate>> candidates) {
  LogServerResponseReceived(log_router_, navigation_id, url.GetHost(),
                            candidates ? candidates->size() : 0);

  if (!candidates || candidates->empty()) {
    LogSuggestionSuppressed(log_router_, navigation_id, url.GetHost(),
                            !candidates ? "fetch_failed" : "no_candidates");
    return;
  }
  // TODO(crbug.com/493511925): For the time being, the first candidate is
  // chosen by default. Implement the logic to select the best execution
  // candidate.
  FilterSuggestionCandidate& candidate = candidates->front();

  // Validate that the candidate URL uses a secure cryptographic scheme and
  // shares the same eTLD+1 as the triggering URL. A strict same-origin check is
  // avoided here because many websites use distinct subdomains for different
  // pages.
  const bool is_cryptographic =
      candidate.navigation_url.SchemeIsCryptographic();
  const bool is_http_allowed_for_testing =
      candidate.navigation_url.SchemeIs(url::kHttpScheme) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMultistepFilterAllowHttpForTesting);

  if (!is_cryptographic && !is_http_allowed_for_testing) {
    LogSuggestionSuppressed(log_router_, navigation_id, url.GetHost(),
                            "insecure_scheme");
    return;
  }

  if (GetEtldPlusOne(candidate.navigation_url) != GetEtldPlusOne(url)) {
    LogSuggestionSuppressed(log_router_, navigation_id, url.GetHost(),
                            "cross_domain");
    return;
  }

  if (IsUrlSubsumedBy(candidate.navigation_url, url)) {
    LogSuggestionSuppressed(log_router_, navigation_id, url.GetHost(), "subsumed");
    return;
  }

  auto matching_annotation_it = std::ranges::find(
      annotations, candidate.filter_annotation_id, &FilterAnnotation::id);
  if (matching_annotation_it == annotations.end()) {
    LogSuggestionSuppressed(log_router_, navigation_id, url.GetHost(),
                            "annotation_not_found");
    return;
  }

  std::vector<FilterAttributeUiLabel> attribute_ui_labels;
  for (FilterSuggestionCandidateAttribute& candidate_attribute :
       candidate.attributes) {
    auto matching_attribute_it =
        std::ranges::find(matching_annotation_it->attributes,
                          candidate_attribute.key, &FilterAttribute::key);
    if (matching_attribute_it == matching_annotation_it->attributes.end()) {
      continue;
    }
    attribute_ui_labels.emplace_back(std::move(candidate_attribute));
  }

  if (attribute_ui_labels.size() <= 1) {
    LogSuggestionSuppressed(log_router_, navigation_id, url.GetHost(),
                            "too_few_attributes");
    return;
  }

  if (base::TrimWhitespace(candidate.detailed_text, base::TRIM_ALL).empty() ||
      base::TrimWhitespace(candidate.short_text, base::TRIM_ALL).empty()) {
    LogSuggestionSuppressed(log_router_, navigation_id, url.GetHost(),
                            "missing_suggestion_message");
    return;
  }

  // Suggestion generation succeeded, reset `failure_callback` as to not notify
  // otherwise.
  failure_callback.ReplaceClosure(base::DoNothing());

  UrlFilterSuggestion suggestion(UrlFilterSuggestion::Params{
      .navigation_url = std::move(candidate.navigation_url),
      .source_host = base::UTF8ToUTF16(matching_annotation_it->source_host),
      .extraction_timestamp = matching_annotation_it->creation_timestamp,
      .attribute_ui_labels = std::move(attribute_ui_labels),
      .triggering_navigation_id = navigation_id,
      .triggering_host = url.GetHost(),
      .task_type = std::move(matching_annotation_it->task_type),
      .suggestion_message = std::move(candidate.detailed_text),
      .short_suggestion_message = std::move(candidate.short_text)});
  LogSuggestionGenerated(log_router_, navigation_id, url.GetHost(), suggestion);
  std::move(success_callback).Run(std::move(suggestion));
}


}  // namespace multistep_filter
