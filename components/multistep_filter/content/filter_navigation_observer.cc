// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/content/filter_navigation_observer.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace multistep_filter {

using content::NavigationHandle;

namespace {

// Internal structure to hold navigation properties for easier logic processing.
struct NavigationMetadata {
  GURL url;
  GURL prev_url;
  // Caching etld_plus_one upfront avoids multiple expensive public suffix list
  // registry lookups and repeated heap string allocations during the hot
  // logging path.
  std::string etld_plus_one;
  bool is_valid_http_or_https_navigation;
  bool is_error_page_navigation;
  bool has_user_gesture;
  bool was_filter_initiated_navigation;
  bool is_same_document_navigation;

  explicit NavigationMetadata(NavigationHandle* handle)
      : url(handle->GetURL()),
        prev_url(handle->GetPreviousPrimaryMainFrameURL()),
        etld_plus_one(GetEtldPlusOne(url)),
        is_valid_http_or_https_navigation(url.SchemeIsHTTPOrHTTPS()),
        is_error_page_navigation(handle->IsErrorPage()),
        has_user_gesture(handle->HasUserGesture()),
        was_filter_initiated_navigation(
            FilterInitiatedNavigationMarker::GetForNavigationHandle(*handle) !=
            nullptr),
        is_same_document_navigation(handle->IsSameDocument()) {}
};

void LogNavigationStarted(MultistepFilterLogRouter* log_router,
                          int64_t navigation_id,
                          std::string_view domain) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kNavigationStarted, domain);
}

void LogSuggestionCleared(MultistepFilterLogRouter* log_router,
                          int64_t navigation_id,
                          std::string_view domain) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionCleared, domain);
}

void LogUrlEligibilityCheck(MultistepFilterLogRouter* log_router,
                            int64_t navigation_id,
                            std::string_view domain,
                            bool eligible,
                            std::string_view reason = "") {
  if (reason.empty()) {
    MULTISTEP_FILTER_LOG(log_router, navigation_id,
                         LogEventType::kUrlEligibilityCheck, domain)
        << LogDetail{"eligible", eligible};
  } else {
    MULTISTEP_FILTER_LOG(log_router, navigation_id,
                         LogEventType::kUrlEligibilityCheck, domain)
        << LogDetail{"eligible", eligible}
        << LogDetail{"reason", std::string(reason)};
  }
}

void LogAnnotationExtractionStarted(MultistepFilterLogRouter* log_router,
                                    int64_t navigation_id,
                                    std::string_view domain) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kAnnotationExtractionStarted, domain);
}

void LogExtractionEligibilityCheck(MultistepFilterLogRouter* log_router,
                                   int64_t navigation_id,
                                   std::string_view domain,
                                   bool eligible,
                                   std::string_view reason = "") {
  if (reason.empty()) {
    MULTISTEP_FILTER_LOG(log_router, navigation_id,
                         LogEventType::kUrlEligibilityCheck, domain)
        << LogDetail{"extraction_eligible", eligible};
  } else {
    MULTISTEP_FILTER_LOG(log_router, navigation_id,
                         LogEventType::kUrlEligibilityCheck, domain)
        << LogDetail{"extraction_eligible", eligible}
        << LogDetail{"reason", std::string(reason)};
  }
}

void LogSuggestionSuppressed(MultistepFilterLogRouter* log_router,
                             int64_t navigation_id,
                             std::string_view domain,
                             std::string_view reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionSuppressed, domain)
      << LogDetail{"reason", std::string(reason)};
}

void LogSuggestionGenerationStarted(MultistepFilterLogRouter* log_router,
                                    int64_t navigation_id,
                                    std::string_view domain) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionGenerationStarted, domain);
}

}  // namespace

FilterNavigationObserver::FilterNavigationObserver(
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

FilterNavigationObserver::~FilterNavigationObserver() = default;

void FilterNavigationObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!service_) {
    return;
  }

  // We only care about committed navigations in the primary main frame.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  NavigationMetadata metadata(navigation_handle);
  int64_t navigation_id = navigation_handle->GetNavigationId();
  LogNavigationStarted(log_router_, navigation_id, metadata.etld_plus_one);

  // Avoid clearing suggestions for same-document navigations or same-URL
  // re-commits (including reloads). These are often intermediate states during
  // page load or explicit user refreshes where we want to preserve the current
  // suggestion UI.
  bool is_same_page =
      metadata.is_same_document_navigation || metadata.url == metadata.prev_url;
  if (!is_same_page) {
    LogSuggestionCleared(log_router_, navigation_id, metadata.etld_plus_one);
    delegate_->ClearSuggestion();
  }

  // Only process valid web content (HTTP/S, non-error).
  // Allow same-document navigations as they often represent Single Page
  // Application (SPA) state changes, but ignore other re-commits.
  if (metadata.is_error_page_navigation) {
    LogUrlEligibilityCheck(log_router_, navigation_id, metadata.etld_plus_one,
                           /*eligible=*/false, "error_page");
    return;
  }

  if (!metadata.is_valid_http_or_https_navigation) {
    LogUrlEligibilityCheck(log_router_, navigation_id, metadata.etld_plus_one,
                           /*eligible=*/false, "non_http_or_https");
    return;
  }

  if (metadata.url == metadata.prev_url &&
      !metadata.is_same_document_navigation) {
    LogUrlEligibilityCheck(log_router_, navigation_id, metadata.etld_plus_one,
                           /*eligible=*/false, "same_url_non_same_document");
    return;
  }

  LogUrlEligibilityCheck(log_router_, navigation_id, metadata.etld_plus_one,
                         /*eligible=*/true);

  // Ensure the interaction was intentional by the user (e.g., a search button
  // click, omnibox navigation, or bookmark). This avoids extracting from
  // automatic client-side redirects.
  if (metadata.has_user_gesture) {
    LogAnnotationExtractionStarted(log_router_, navigation_id,
                                   metadata.etld_plus_one);
    service_->ExtractAnnotation(navigation_id, metadata.url);
  } else {
    LogExtractionEligibilityCheck(log_router_, navigation_id,
                                  metadata.etld_plus_one, /*eligible=*/false,
                                  "no_user_gesture_for_extraction");
  }

  // Prevent showing suggestions for same-site navigations to avoid spamming
  // the user, and don't re-trigger if the navigation was already initiated by
  // the filter UI.
  if (metadata.was_filter_initiated_navigation ||
      IsSameDomainOrHost(metadata.url, metadata.prev_url)) {
    LogSuggestionSuppressed(log_router_, navigation_id, metadata.etld_plus_one,
                            metadata.was_filter_initiated_navigation
                                ? "filter_initiated"
                                : "same_domain");
    return;
  }

  if (delegate_->ShouldSuppressSuggestions(metadata.url)) {
    LogSuggestionSuppressed(log_router_, navigation_id, metadata.etld_plus_one,
                            "delegate_suppressed");
    delegate_->OnSuggestionGenerated(std::nullopt);
    return;
  }

  LogSuggestionGenerationStarted(log_router_, navigation_id,
                                 metadata.etld_plus_one);
  service_->GenerateFilterSuggestions(
      navigation_id, metadata.url,
      base::BindOnce(&MultistepFilterUiDelegate::OnSuggestionGenerated,
                     delegate_->GetWeakPtr()));
}

void FilterNavigationObserver::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  delegate_->ClearSuggestion();
}

}  // namespace multistep_filter
