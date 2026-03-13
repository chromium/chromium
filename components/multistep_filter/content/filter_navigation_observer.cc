// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/content/filter_navigation_observer.h"

#include "base/check.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace multistep_filter {

FilterNavigationObserver::FilterNavigationObserver(
    content::WebContents* web_contents,
    MultistepFilterService* service,
    std::unique_ptr<UiDelegate> delegate)
    : content::WebContentsObserver(web_contents),
      service_(service),
      delegate_(std::move(delegate)) {
  CHECK(delegate_);
}

FilterNavigationObserver::~FilterNavigationObserver() = default;

void FilterNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Track only committed, primary main frame navigations. Ignore downloads,
  // subframes, and same-document navigations (e.g. #foo).
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  delegate_->ClearSuggestion();

  if (navigation_handle->GetReloadType() != content::ReloadType::NONE ||
      navigation_handle->IsErrorPage() ||
      !navigation_handle->GetWebContents()) {
    return;
  }

  // If this navigation was triggered by the user accepting a suggestion,
  // do not generate a new suggestion for the resulting page.
  if (FilterInitiatedNavigationMarker::GetForNavigationHandle(
          *navigation_handle)) {
    return;
  }

  const GURL& url = navigation_handle->GetURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (service_) {
    service_->GenerateFilterSuggestions(url,
                                        delegate_->GetSuggestionCallback());
  }
}

}  // namespace multistep_filter
