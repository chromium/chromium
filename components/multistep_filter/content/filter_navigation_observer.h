// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_NAVIGATION_OBSERVER_H_
#define COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_NAVIGATION_OBSERVER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace multistep_filter {

class MultistepFilterLogRouter;
class MultistepFilterService;
class MultistepFilterUiDelegate;

// Observes navigations to trigger Multistep Filter feature logic.
// This observer detects primary main frame navigations, clears existing
// suggestions, and requests new suggestions from the MultistepFilterService for
// eligible URLs.
class FilterNavigationObserver : public content::WebContentsObserver {
 public:
  FilterNavigationObserver(content::WebContents* web_contents,
                           MultistepFilterService* service,
                           MultistepFilterLogRouter* log_router,
                           std::unique_ptr<MultistepFilterUiDelegate> delegate);

  FilterNavigationObserver(const FilterNavigationObserver&) = delete;
  FilterNavigationObserver& operator=(const FilterNavigationObserver&) = delete;

  ~FilterNavigationObserver() override;

  // content::WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

 private:
  // The MultistepFilterService to use for generating
  // suggestions. This service must outlive this observer.
  // TODO (crbug.com/498901792): Use raw_ref for service when the feature is
  // completely launched.
  raw_ptr<MultistepFilterService> service_;

  // Log router for tracing navigation decisions.
  raw_ptr<MultistepFilterLogRouter> log_router_;

  // Delegate to provide contextual information and interact with the UI.
  std::unique_ptr<MultistepFilterUiDelegate> delegate_;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_NAVIGATION_OBSERVER_H_
