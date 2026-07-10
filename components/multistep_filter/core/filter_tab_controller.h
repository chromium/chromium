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
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics_tracker.h"

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
// ### Ownership & Lifecycle of Owned Components
//
// This controller is owned by a tab-scoped helper (e.g.
// `ContentFilterNavigationObserver`) and is destroyed when the tab is closed.
// It manages the following components:
//
// 1. **Tab-Scoped Helpers (Owned)**:
//    * `FilterExtractor` (instantiated here): Extracts annotations from the
//      webpage undergoing navigation. Its lifecycle is tied to this tab.
//    * `FilterSuggestionGenerator` (instantiated here): Generates filter
//      suggestions for this tab.
//    * Both helpers are destroyed with this controller, safely cancelling their
//      associated operations.
//
// 2. **Tab-Scoped UI (Referenced)**:
//    * `MultistepFilterUiDelegate`: Interface implemented by the tab's UI layer
//      (e.g., `FilterUiController`). The controller holds a reference to it
//      to show/clear suggestions in the tab. The delegate is guaranteed to
//      outlive this controller because their common owner (e.g.,
//      `ContentFilterNavigationObserver`) declares the delegate before this
//      controller, ensuring this controller is destroyed first.
//
// 3. **Profile-Scoped Dependencies (Referenced)**:
//    * References to `MultistepFilterService`, `FilterStore`, and
//      `AnnotationIndexClient` are passed to the constructor. These are
//      shared services owned by the `Profile` and live independently of this
//      tab's lifecycle.
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
                      MultistepFilterUiDelegate* delegate,
                      FilterStore* filter_store,
                      AnnotationIndexClient* annotation_client);

  FilterTabController(const FilterTabController&) = delete;
  FilterTabController& operator=(const FilterTabController&) = delete;

  // Virtual for testing.
  virtual ~FilterTabController();

  // Called when a tab navigation finishes. Initiates URL and privacy validation
  // checks, and potentially triggers extraction and suggestion flows for
  // multistep filter.
  // Virtual for testing.
  virtual void OnNavigationFinished(const FilterNavigationMetadata& metadata);

  // Called when the user interacts with the suggestion UI.
  void OnSuggestionShown(const UrlFilterSuggestion& suggestion);
  void OnSuggestionReopened();
  void OnUserDecision(SuggestionUserDecision decision);

  base::WeakPtr<FilterTabController> GetWeakPtr();

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

  // Profile-scoped dependencies. Guaranteed to outlive this class by the
  // owner.
  const raw_ref<MultistepFilterService> service_;
  const raw_ref<FilterStore> filter_store_;
  const raw_ref<AnnotationIndexClient> annotation_client_;
  raw_ptr<MultistepFilterLogRouter> log_router_;

  // Tab-scoped UI dependencies (not owned by this controller).
  // Bound to the tab UI lifetime. Guaranteed to outlive this class by the
  // owner.
  const raw_ref<MultistepFilterUiDelegate> delegate_;

  // Set and used exclusively in tests. Not owned.
  raw_ptr<ObserverForTest> observer_for_test_ = nullptr;

  // Tab-scoped dependencies. Owned by this controller.
  std::unique_ptr<FilterExtractor> filter_extractor_;
  std::unique_ptr<FilterSuggestionGenerator> filter_suggestion_generator_;
  MultistepFilterMetricsTracker metrics_tracker_;

  // This should be kept at the end so that it is the first member to be
  // destroyed. This factory is also invalidated on every new navigation to
  // abort pending async tasks for the previous page.
  base::WeakPtrFactory<FilterTabController> weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_FILTER_TAB_CONTROLLER_H_
