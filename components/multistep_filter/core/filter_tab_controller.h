// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_FILTER_TAB_CONTROLLER_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_FILTER_TAB_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_navigation_metadata.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

namespace multistep_filter {

class AnnotationIndexClient;
class FilterExtractor;
class FilterStore;
class FilterSuggestionGenerator;
class MultistepFilterLogRouter;
class MultistepFilterService;
class MultistepFilterUiDelegate;

// Controls the lifecycle of filter suggestions and annotations for a single
// tab. It reacts to navigation events, validates URL eligibility and privacy
// consent, and orchestrates the extraction and suggestion generation cascade.
//
// This class is owned by `ContentFilterNavigationObserver` (or
// platform-specific tab helpers) which is responsible for notifying it of
// committed main-frame navigations via `OnNavigationFinished`. Since it holds a
// weak pointer factory for async suggestion/extraction callbacks, its
// destruction cleanly cancels any pending tasks when the tab is closed or
// discarded.
class FilterTabController {
 public:
  // Observer interface used exclusively in tests to capture completion
  // callbacks.
  class ObserverForTest {
   public:
    virtual ~ObserverForTest() = default;
    virtual void OnSuggestionGeneratedForTest(
        std::optional<UrlFilterSuggestion> suggestion) = 0;
    virtual void OnExtractionFinishedForTest(
        std::optional<base::Uuid> annotation_id) = 0;
  };

  // Constructs a FilterTabController bound to the provided service and UI
  // delegate.
  FilterTabController(MultistepFilterService* service,
                      MultistepFilterLogRouter* log_router,
                      base::WeakPtr<MultistepFilterUiDelegate> delegate,
                      FilterStore* filter_store,
                      AnnotationIndexClient* annotation_client);

  FilterTabController(const FilterTabController&) = delete;
  FilterTabController& operator=(const FilterTabController&) = delete;

  ~FilterTabController();

  // Called when a tab navigation finishes. Initiates URL and privacy validation
  // checks, and potentially triggers extraction and suggestion flows for
  // multistep filter.
  void OnNavigationFinished(const FilterNavigationMetadata& metadata);

 private:
  friend class FilterTabControllerTestApi;

  void OnSupportedTasksFetched(
      const FilterNavigationMetadata& metadata,
      base::ScopedClosureRunner extraction_runner_fallback,
      base::ScopedClosureRunner generation_runner_fallback,
      std::vector<std::string> supported_task_types);

  void OnExtractionFinished(const FilterNavigationMetadata& metadata,
                            std::optional<FilterAnnotation> annotation);
  void OnSuggestionGenerated(std::optional<UrlFilterSuggestion> suggestion);

  const raw_ref<MultistepFilterService> service_;
  const raw_ref<FilterStore> filter_store_;
  const raw_ref<AnnotationIndexClient> annotation_client_;
  raw_ptr<MultistepFilterLogRouter> log_router_;
  raw_ptr<ObserverForTest> observer_for_test_ = nullptr;
  base::WeakPtr<MultistepFilterUiDelegate> delegate_;

  std::unique_ptr<FilterExtractor> filter_extractor_;
  std::unique_ptr<FilterSuggestionGenerator> filter_suggestion_generator_;

  // This should be kept at the end so that it is the first member to be
  // destroyed.
  base::WeakPtrFactory<FilterTabController> weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_FILTER_TAB_CONTROLLER_H_
