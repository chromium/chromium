// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/content/filter_navigation_observer.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

// Internal structure to hold navigation properties for easier logic processing.
struct NavigationMetadata {
  GURL url;
  GURL prev_url;
  bool is_valid_http_or_https_navigation;
  bool is_error_page_navigation;
  bool has_user_gesture;
  bool was_filter_initiated_navigation;
  bool is_same_document_navigation;

  explicit NavigationMetadata(content::NavigationHandle* handle)
      : url(handle->GetURL()),
        prev_url(handle->GetPreviousPrimaryMainFrameURL()),
        is_valid_http_or_https_navigation(url.SchemeIsHTTPOrHTTPS()),
        is_error_page_navigation(handle->IsErrorPage()),
        has_user_gesture(handle->HasUserGesture()),
        was_filter_initiated_navigation(
            FilterInitiatedNavigationMarker::GetForNavigationHandle(*handle) !=
            nullptr),
        is_same_document_navigation(handle->IsSameDocument()) {}
};

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
  if (!service_) {
    return;
  }

  // We only care about committed navigations in the primary main frame.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  NavigationMetadata metadata(navigation_handle);

  // Avoid clearing suggestions for same-document navigations or same-URL
  // re-commits (including reloads). These are often intermediate states during
  // page load or explicit user refreshes where we want to preserve the current
  // suggestion UI.
  bool is_same_page =
      metadata.is_same_document_navigation || metadata.url == metadata.prev_url;
  if (!is_same_page) {
    delegate_->ClearSuggestion();
  }

  // Only process valid web content (HTTP/S, non-error).
  // Allow same-document navigations as they often represent Single Page
  // Application (SPA) state changes, but ignore other re-commits.
  if (metadata.is_error_page_navigation ||
      !metadata.is_valid_http_or_https_navigation ||
      (metadata.url == metadata.prev_url &&
       !metadata.is_same_document_navigation)) {
    return;
  }

  // Ensure the interaction was intentional by the user (e.g., a search button
  // click, omnibox navigation, or bookmark). This avoids extracting from
  // automatic client-side redirects.
  if (metadata.has_user_gesture) {
    service_->ExtractAnnotation(metadata.url);
  }

  // Prevent showing suggestions for same-site navigations to avoid spamming
  // the user, and don't re-trigger if the navigation was already initiated by
  // the filter UI.
  if (metadata.was_filter_initiated_navigation ||
      IsSameDomainOrHost(metadata.url, metadata.prev_url)) {
    return;
  }

  if (delegate_->ShouldSuppressSuggestions(metadata.url)) {
    delegate_->OnSuggestionGenerated(std::nullopt);
    return;
  }

  service_->GenerateFilterSuggestions(
      metadata.url,
      base::BindOnce(&MultistepFilterUiDelegate::OnSuggestionGenerated,
                     delegate_->GetWeakPtr()));
}

void FilterNavigationObserver::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  delegate_->ClearSuggestion();
}

}  // namespace multistep_filter
