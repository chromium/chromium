// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/storage/filter_store.h"

namespace multistep_filter {

// TODO(crbug.com/483673955): Add telemetry for this class.
FilterSuggestionGenerator::FilterSuggestionGenerator(
    AnnotationIndexClient& annotation_index_client,
    FilterStore& filter_store)
    : annotation_index_client_(annotation_index_client),
      filter_store_(filter_store) {}

FilterSuggestionGenerator::~FilterSuggestionGenerator() = default;

// TODO(crbug.com/493495291): Guarantee that the callback is always invoked.
void FilterSuggestionGenerator::GenerateSuggestion(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback) {
  annotation_index_client_->GetSupportedTaskTypesForDomain(
      url.host(),
      base::BindOnce(&FilterSuggestionGenerator::OnSupportedTaskTypesFetched,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void FilterSuggestionGenerator::OnSupportedTaskTypesFetched(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
    std::optional<std::vector<std::string>> supported_task_types) {
  if (!supported_task_types || supported_task_types->empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Fetch annotations for multiple task types asynchronously from the
  // FilterStore. The BarrierCallback waits until all these individual queries
  // complete before aggregating and processing them in
  // `OnAllAnnotationsFetched()`.
  auto barrier_callback = base::BarrierCallback<std::vector<FilterAnnotation>>(
      supported_task_types->size(),
      base::BindOnce(&FilterSuggestionGenerator::OnAllAnnotationsFetched,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));

  // TODO(crbug.com/493485174): Filter supported task types to only include
  // filtering tasks.
  for (const std::string& task_type : *supported_task_types) {
    filter_store_->GetAnnotationsForTaskSortedByCreationTimestamp(
        task_type, barrier_callback);
  }
}

void FilterSuggestionGenerator::OnAllAnnotationsFetched(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
    std::vector<std::vector<FilterAnnotation>> filter_annotations) {
  std::vector<FilterAnnotation> all_annotations;
  for (std::vector<FilterAnnotation>& annotations_for_task_type :
       filter_annotations) {
    base::Extend(all_annotations, std::move(annotations_for_task_type));
  }

  if (all_annotations.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Sort the aggregated list of annotations by `creation_timestamp` in
  // descending order to prioritize the most recently created annotations when
  // limiting the number of candidates.
  std::ranges::sort(all_annotations,
                    [](const FilterAnnotation& a, const FilterAnnotation& b) {
                      return a.creation_timestamp > b.creation_timestamp;
                    });

  // Limit the number of candidates to bound the size of the payload sent to the
  // server.
  const size_t max_candidates = kMultistepFilterSuggestionMaxCandidates.Get();
  if (all_annotations.size() > max_candidates) {
    all_annotations.erase(all_annotations.begin() + max_candidates,
                          all_annotations.end());
  }

  annotation_index_client_->GetFilterSuggestionCandidates(
      url, all_annotations,
      base::BindOnce(
          &FilterSuggestionGenerator::OnFilterSuggestionCandidatesFetched,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FilterSuggestionGenerator::OnFilterSuggestionCandidatesFetched(
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
    std::optional<std::vector<FilterSuggestionCandidate>> candidates) {
  if (!candidates || candidates->empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  // TODO(crbug.com/493511925): For the time being, the first candidate is
  // chosen by default. Implement the logic to select the best execution
  // candidate.
  std::move(callback).Run(UrlFilterSuggestion(std::move(candidates->front())));
}

}  // namespace multistep_filter
