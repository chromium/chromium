// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_SUGGESTION_FILTER_SUGGESTION_GENERATOR_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_SUGGESTION_FILTER_SUGGESTION_GENERATOR_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "url/gurl.h"

namespace multistep_filter {

class AnnotationIndexClient;
class FilterStore;
class MultistepFilterLogRouter;
struct FilterAnnotation;
struct FilterSuggestionCandidate;

// Responsible for orchestrating the suggestion generation process for a given
// URL. This class is owned by the `MultistepFilterService` and shares its
// lifecycle.
class FilterSuggestionGenerator {
 public:
  FilterSuggestionGenerator(AnnotationIndexClient& annotation_index_client,
                            FilterStore& filter_store,
                            MultistepFilterLogRouter* log_router);
  virtual ~FilterSuggestionGenerator();

  FilterSuggestionGenerator(const FilterSuggestionGenerator&) = delete;
  FilterSuggestionGenerator& operator=(const FilterSuggestionGenerator&) =
      delete;

  // Evaluates `url` to determine if a filter suggestion is applicable, and
  // invokes `callback` with a suggestion if one is generated, or with
  // std::nullopt if no suggestion is applicable. It is guaranteed that
  // one of `[success|failure]_callback` will be called.
  //
  // The generation process follows these steps:
  // 1) Query the server via the `AnnotationIndexClient` to determine the
  //    supported tasks for the current domain.
  // 2) On the first server response (`OnSupportedTaskTypesFetched()`), query
  //    the `FilterStore` to retrieve relevant historical user annotations.
  // 3) On the store response (`OnAllAnnotationsFetched()`), query the server
  //    via the `AnnotationIndexClient` a second time to evaluate these
  //    candidates and generate concrete filter suggestions.
  // 4) On the second server response (`OnFilterSuggestionCandidatesFetched()`),
  //    invoke the `callback` with the first suggestion if available, or
  //    std::nullopt otherwise.
  virtual void GenerateSuggestion(
      const GURL& url,
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
      int64_t navigation_id,
      std::string_view domain);

 private:
  // See documentation of `GenerateSuggestion()` for more details.
  void OnSupportedTaskTypesFetched(
      const GURL& url,
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
          success_callback,
      base::ScopedClosureRunner failure_callback,
      int64_t navigation_id,
      std::string_view domain,
      std::optional<std::vector<std::string>> supported_task_types);
  void OnAllAnnotationsFetched(
      const GURL& url,
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
          success_callback,
      base::ScopedClosureRunner failure_callback,
      int64_t navigation_id,
      std::string_view domain,
      std::vector<std::vector<FilterAnnotation>> filter_annotations);
  void OnFilterSuggestionCandidatesFetched(
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
          success_callback,
      base::ScopedClosureRunner failure_callback,
      std::vector<FilterAnnotation> annotations,
      int64_t navigation_id,
      std::string_view domain,
      std::optional<std::vector<FilterSuggestionCandidate>> candidates);

  // The client used to fetch supported task types and URL filter suggestions.
  // This is a non-owning reference. The lifetime of the `AnnotationIndexClient`
  // object is managed by the `MultistepFilterService` instance that owns this
  // `FilterSuggestionGenerator`.
  const base::raw_ref<AnnotationIndexClient> annotation_index_client_;

  // The store used to retrieve past annotations. This is a non-owning
  // reference. The lifetime of the `FilterStore` object is managed by the
  // `MultistepFilterService` instance that owns this
  // `FilterSuggestionGenerator`.
  const base::raw_ref<FilterStore> filter_store_;

  // Log router for the internals page.
  const raw_ptr<MultistepFilterLogRouter> log_router_;

  // This should be kept at the end so that it is the first member to be
  // destroyed.
  base::WeakPtrFactory<FilterSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_SUGGESTION_FILTER_SUGGESTION_GENERATOR_H_
