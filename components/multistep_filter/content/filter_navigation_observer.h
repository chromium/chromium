// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_NAVIGATION_OBSERVER_H_
#define COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_NAVIGATION_OBSERVER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace multistep_filter {

class MultistepFilterService;

// Observes navigations to trigger Multistep Filter feature logic.
// This observer detects primary main frame navigations, clears existing
// suggestions, and requests new suggestions from the MultistepFilterService for
// eligible URLs.
class FilterNavigationObserver : public content::WebContentsObserver {
 public:
  // Interface to interact with the UI controller.
  class UiDelegate {
   public:
    virtual ~UiDelegate() = default;
    virtual void ClearSuggestion() = 0;
    virtual base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
    GetSuggestionCallback() = 0;
  };

  FilterNavigationObserver(content::WebContents* web_contents,
                           MultistepFilterService* service,
                           std::unique_ptr<UiDelegate> delegate);

  FilterNavigationObserver(const FilterNavigationObserver&) = delete;
  FilterNavigationObserver& operator=(const FilterNavigationObserver&) = delete;

  ~FilterNavigationObserver() override;

  // content::WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // The MultistepFilterService to use for generating suggestions.
  // This service must outlive this observer.
  raw_ptr<MultistepFilterService> service_;

  // Delegate to interact with the UI.
  std::unique_ptr<UiDelegate> delegate_;

  base::WeakPtrFactory<FilterNavigationObserver> weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_NAVIGATION_OBSERVER_H_
