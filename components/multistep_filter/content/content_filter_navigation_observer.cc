// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/content/content_filter_navigation_observer.h"

#include <optional>

#include "base/check.h"
#include "base/command_line.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/data_models/filter_navigation_metadata.h"
#include "components/multistep_filter/core/filter_tab_controller.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace multistep_filter {

using content::NavigationHandle;

namespace {

// Internal structure to hold navigation properties for easier logic processing.
FilterNavigationMetadata CreateFilterNavigationMetadata(
    NavigationHandle* handle) {
  FilterNavigationMetadata metadata;
  metadata.navigation_id = handle->GetNavigationId();
  metadata.ukm_source_id = handle->GetNextPageUkmSourceId();
  metadata.navigation_start_time = handle->NavigationStart();
  metadata.navigation_finish_time =
      handle->GetNavigationHandleTiming().navigation_did_commit_time;
  metadata.url = handle->GetURL();
  metadata.prev_url = handle->GetPreviousPrimaryMainFrameURL();
  metadata.is_cryptographic_scheme = metadata.url.SchemeIsCryptographic();
  metadata.is_http_allowed_for_testing =
      metadata.url.SchemeIs(url::kHttpScheme) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMultistepFilterAllowHttpForTesting);
  metadata.net_error_code = handle->GetNetErrorCode();
  metadata.http_response_code =
      handle->GetResponseHeaders()
          ? handle->GetResponseHeaders()->response_code()
          : 200;
  metadata.is_error_page_navigation = handle->IsErrorPage() ||
                                      metadata.net_error_code != 0 ||
                                      (metadata.http_response_code >= 400 &&
                                       metadata.http_response_code <= 599);
  metadata.has_user_gesture = handle->HasUserGesture();
  const FilterInitiatedNavigationMarker* marker =
      FilterInitiatedNavigationMarker::GetForNavigationHandle(*handle);
  metadata.was_filter_initiated_navigation = (marker != nullptr);
  metadata.applied_suggestion = marker ? marker->suggestion() : std::nullopt;
  metadata.is_same_document_navigation = handle->IsSameDocument();
  metadata.is_back_navigation =
      handle->IsHistory() && handle->GetNavigationEntryOffset() < 0;
  ui::PageTransition transition = handle->GetPageTransition();
  metadata.is_navigation_from_omnibox_or_bookmarks =
      ((transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0) ||
      ui::PageTransitionCoreTypeIs(transition,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  return metadata;
}

}  // namespace

ContentFilterNavigationObserver::ContentFilterNavigationObserver(
    content::WebContents* web_contents,
    MultistepFilterService* service,
    MultistepFilterLogRouter* log_router,
    std::unique_ptr<MultistepFilterUiDelegate> delegate)
    : content::WebContentsObserver(web_contents),
      delegate_(std::move(delegate)) {
  CHECK(delegate_);
  CHECK(service);
  tab_controller_ = std::make_unique<FilterTabController>(
      service, log_router, delegate_.get(), service->GetFilterStore(),
      service->GetAnnotationIndexClient());
}

ContentFilterNavigationObserver::~ContentFilterNavigationObserver() = default;

void ContentFilterNavigationObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // We only care about committed navigations in the outermost primary main
  // frame.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->IsInOutermostMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  FilterNavigationMetadata metadata =
      CreateFilterNavigationMetadata(navigation_handle);
  tab_controller_->OnNavigationFinished(metadata);
}

void ContentFilterNavigationObserver::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  delegate_->ClearSuggestion();
}

}  // namespace multistep_filter
