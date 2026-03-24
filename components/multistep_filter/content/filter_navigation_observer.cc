// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/content/filter_navigation_observer.h"

#include "base/check.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

bool ShouldIgnoreNavigation(content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsErrorPage()) {
    return true;
  }

  const GURL& url = navigation_handle->GetURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return true;
  }

  const GURL& prev_url = navigation_handle->GetPreviousPrimaryMainFrameURL();
  const bool is_reload =
      navigation_handle->GetReloadType() != content::ReloadType::NONE;

  if (is_reload || url == prev_url) {
    return true;
  }

  return false;
}

bool ShouldGenerateSuggestions(content::NavigationHandle* navigation_handle) {
  // If this navigation was triggered by the user accepting a suggestion,
  // do not generate a new suggestion for the resulting page.
  if (FilterInitiatedNavigationMarker::GetForNavigationHandle(
          *navigation_handle)) {
    return false;
  }

  // Do not trigger suggestion flow if the eTLD+1 of the new URL is the same
  // as the previous one. This also covers fragment-only changes and path
  // changes on the same site.
  return !IsSameDomainOrHost(
      navigation_handle->GetURL(),
      navigation_handle->GetPreviousPrimaryMainFrameURL());
}

}  // namespace

FilterNavigationObserver::FilterNavigationObserver(
    content::WebContents* web_contents,
    MultistepFilterService* service,
    std::unique_ptr<MultistepFilterUiDelegate> delegate)
    : content::WebContentsObserver(web_contents),
      service_(service),
      delegate_(std::move(delegate)) {
  CHECK(delegate_);
}

FilterNavigationObserver::~FilterNavigationObserver() = default;

void FilterNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // We only care about committed navigations in the primary main frame.
  // This includes page activations (BFCache restorations, prerender
  // activations) and same-document navigations (SPA transitions).
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // Clear suggestions for the old page.
  delegate_->ClearSuggestion();

  if (!service_ || ShouldIgnoreNavigation(navigation_handle)) {
    return;
  }

  const GURL& url = navigation_handle->GetURL();
  // We always extract annotations for valid navigations.
  service_->ExtractAnnotation(url);

  // We only show suggestions for "fresh" navigations to new sites.
  if (ShouldGenerateSuggestions(navigation_handle)) {
    service_->GenerateFilterSuggestions(url, delegate_->GetWeakPtr());
  }
}

void FilterNavigationObserver::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  delegate_->ClearSuggestion();
}

}  // namespace multistep_filter
