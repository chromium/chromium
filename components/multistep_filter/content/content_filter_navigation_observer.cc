// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/content/content_filter_navigation_observer.h"

#include <optional>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/data_models/filter_navigation_metadata.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
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
  metadata.was_filter_initiated_navigation =
      FilterInitiatedNavigationMarker::GetForNavigationHandle(*handle) !=
      nullptr;
  metadata.applied_suggestion =
      metadata.was_filter_initiated_navigation
          ? FilterInitiatedNavigationMarker::GetForNavigationHandle(*handle)
                ->suggestion()
          : std::nullopt;
  metadata.is_same_document_navigation = handle->IsSameDocument();
  return metadata;
}

void LogNavigationStarted(MultistepFilterLogRouter* log_router,
                          int64_t navigation_id,
                          std::string_view host) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kNavigationStarted, host);
}

void LogSuggestionCleared(MultistepFilterLogRouter* log_router,
                          int64_t navigation_id,
                          std::string_view host) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionCleared, host);
}

void LogUrlEligibilityCheck(MultistepFilterLogRouter* log_router,
                            int64_t navigation_id,
                            std::string_view host,
                            bool eligible,
                            std::string_view reason = "") {
  if (reason.empty()) {
    MULTISTEP_FILTER_LOG(log_router, navigation_id,
                         LogEventType::kUrlEligibilityCheck, host)
        << LogDetail{"eligible", eligible};
  } else {
    MULTISTEP_FILTER_LOG(log_router, navigation_id,
                         LogEventType::kUrlEligibilityCheck, host)
        << LogDetail{"eligible", eligible}
        << LogDetail{"reason", std::string(reason)};
  }
}

void LogAnnotationExtractionStarted(MultistepFilterLogRouter* log_router,
                                    int64_t navigation_id,
                                    std::string_view host) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kAnnotationExtractionStarted, host);
}

void LogSuggestionSuppressed(MultistepFilterLogRouter* log_router,
                             int64_t navigation_id,
                             std::string_view host,
                             std::string_view reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionSuppressed, host)
      << LogDetail{"reason", std::string(reason)};
}

void LogSuggestionGenerationStarted(MultistepFilterLogRouter* log_router,
                                    int64_t navigation_id,
                                    std::string_view host) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionGenerationStarted, host);
}

}  // namespace

ContentFilterNavigationObserver::ContentFilterNavigationObserver(
    content::WebContents* web_contents,
    MultistepFilterService* service,
    MultistepFilterLogRouter* log_router,
    std::unique_ptr<MultistepFilterUiDelegate> delegate)
    : content::WebContentsObserver(web_contents),
      service_(service),
      log_router_(log_router),
      delegate_(std::move(delegate)) {
  CHECK(delegate_);
}

ContentFilterNavigationObserver::~ContentFilterNavigationObserver() = default;

void ContentFilterNavigationObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!service_) {
    return;
  }

  // We only care about committed navigations in the outermost primary main
  // frame.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->IsInOutermostMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  FilterNavigationMetadata metadata =
      CreateFilterNavigationMetadata(navigation_handle);
  int64_t navigation_id = metadata.navigation_id;
  LogNavigationStarted(log_router_, navigation_id, metadata.url.GetHost());

  // Avoid clearing suggestions for same-document navigations or same-URL
  // re-commits (including reloads). These are often intermediate states during
  // page load or explicit user refreshes where we want to preserve the current
  // suggestion UI.
  bool is_same_page =
      metadata.is_same_document_navigation || metadata.url == metadata.prev_url;
  if (!is_same_page) {
    LogSuggestionCleared(log_router_, navigation_id, metadata.url.GetHost());
    delegate_->ClearSuggestion();
  }

  // Only process valid web content (HTTP/S, non-error).
  // Allow same-document navigations as they often represent Single Page
  // Application (SPA) state changes, but ignore other re-commits.
  bool is_unsupported_scheme = !metadata.is_cryptographic_scheme &&
                               !metadata.is_http_allowed_for_testing;

  // Note that the `service_` is supposed to log the successful or failed
  // application of a filter. Therefore, it is important that all early exits
  // that happen despite a real navigation are reported to `service_`.
  if (metadata.is_error_page_navigation || is_unsupported_scheme) {
    service_->NetworkStatusPreventedExtraction(
        navigation_id, metadata.url, metadata.applied_suggestion,
        is_unsupported_scheme, metadata.net_error_code,
        metadata.http_response_code);
  }

  if (metadata.is_error_page_navigation) {
    LogUrlEligibilityCheck(log_router_, navigation_id, metadata.url.GetHost(),
                           /*eligible=*/false, "error_page");
    return;
  }

  if (is_unsupported_scheme) {
    LogUrlEligibilityCheck(log_router_, navigation_id, metadata.url.GetHost(),
                           /*eligible=*/false, "non_cryptographic");
    return;
  }

  if (metadata.url == metadata.prev_url &&
      !metadata.is_same_document_navigation) {
    LogUrlEligibilityCheck(log_router_, navigation_id, metadata.url.GetHost(),
                           /*eligible=*/false, "same_url_non_same_document");
    return;
  }

  // Ensure the interaction was intentional by the user (e.g., a search button
  // click, omnibox navigation, or bookmark). This avoids extracting from or
  // showing suggestions on automatic client-side redirects.
  if (!metadata.has_user_gesture) {
    LogUrlEligibilityCheck(log_router_, navigation_id, metadata.url.GetHost(),
                           /*eligible=*/false, "no_user_gesture");
    return;
  }

  LogAnnotationExtractionStarted(log_router_, navigation_id,
                                 metadata.url.GetHost());
  service_->ExtractAnnotation(navigation_id, metadata.url,
                              metadata.applied_suggestion);

  // Don't re-trigger if the navigation was already initiated by
  // the filter UI.
  if (metadata.was_filter_initiated_navigation) {
    LogSuggestionSuppressed(log_router_, navigation_id, metadata.url.GetHost(),
                            "filter_initiated_navigation");
    return;
  }

  LogSuggestionGenerationStarted(log_router_, navigation_id,
                                 metadata.url.GetHost());
  service_->GenerateFilterSuggestions(
      navigation_id, metadata.url,
      base::BindOnce(&MultistepFilterUiDelegate::OnSuggestionGenerated,
                     delegate_->GetWeakPtr()));
}

void ContentFilterNavigationObserver::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  delegate_->ClearSuggestion();
}

}  // namespace multistep_filter
