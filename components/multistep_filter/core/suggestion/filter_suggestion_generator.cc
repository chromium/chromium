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
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/storage/filter_store.h"

namespace multistep_filter {

// TODO(crbug.com/483673955): Add telemetry for this class.
FilterSuggestionGenerator::FilterSuggestionGenerator(
    AnnotationIndexClient& annotation_index_client,
    FilterStore& filter_store)
    : annotation_index_client_(annotation_index_client),
      filter_store_(filter_store) {}

FilterSuggestionGenerator::~FilterSuggestionGenerator() = default;

void FilterSuggestionGenerator::GenerateSuggestion(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  base::ScopedClosureRunner failure_callback(
      base::BindOnce(std::move(split_callback.first), std::nullopt));

  annotation_index_client_->GetSupportedTaskTypesForDomain(
      GetEtldPlusOne(url),
      base::BindOnce(&FilterSuggestionGenerator::OnSupportedTaskTypesFetched,
                     weak_ptr_factory_.GetWeakPtr(), url,
                     std::move(split_callback.second),
                     std::move(failure_callback)));
}

void FilterSuggestionGenerator::OnSupportedTaskTypesFetched(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
        success_callback,
    base::ScopedClosureRunner failure_callback,
    std::optional<std::vector<std::string>> supported_task_types) {
  if (!supported_task_types || supported_task_types->empty()) {
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
                     std::move(success_callback), std::move(failure_callback)));

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
    std::vector<std::vector<FilterAnnotation>> filter_annotations) {
  std::vector<FilterAnnotation> all_annotations;
  for (std::vector<FilterAnnotation>& annotations_for_task_type :
       filter_annotations) {
    base::Extend(all_annotations, std::move(annotations_for_task_type));
  }

  if (all_annotations.empty()) {
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

  base::span<const FilterAnnotation> all_annotations_span = all_annotations;
  annotation_index_client_->GetFilterSuggestionCandidates(
      url, all_annotations_span,
      base::BindOnce(
          &FilterSuggestionGenerator::OnFilterSuggestionCandidatesFetched,
          weak_ptr_factory_.GetWeakPtr(), std::move(success_callback),
          std::move(failure_callback), std::move(all_annotations)));
}

void FilterSuggestionGenerator::OnFilterSuggestionCandidatesFetched(
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
        success_callback,
    base::ScopedClosureRunner failure_callback,
    std::vector<FilterAnnotation> annotations,
    std::optional<std::vector<FilterSuggestionCandidate>> candidates) {
  if (!candidates || candidates->empty()) {
    return;
  }
  // TODO(crbug.com/493511925): For the time being, the first candidate is
  // chosen by default. Implement the logic to select the best execution
  // candidate.
  FilterSuggestionCandidate& candidate = candidates->front();

  auto matching_annotation_it = std::ranges::find(
      annotations, candidate.filter_annotation_id, &FilterAnnotation::id);
  if (matching_annotation_it == annotations.end()) {
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

  std::move(success_callback)
      .Run(UrlFilterSuggestion(
          std::move(candidate.navigation_url),
          base::UTF8ToUTF16(matching_annotation_it->source_domain),
          matching_annotation_it->creation_timestamp,
          std::move(attribute_ui_labels)));
}

}  // namespace multistep_filter
