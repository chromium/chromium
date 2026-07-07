// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/filter_tab_controller.h"

#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"

namespace multistep_filter {

namespace {

void LogSuggestionCleared(MultistepFilterLogRouter* log_router,
                          const FilterNavigationMetadata& metadata) {
  MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                       LogEventType::kSuggestionCleared,
                       metadata.url.GetHost());
}

void LogUrlEligibilityCheck(MultistepFilterLogRouter* log_router,
                            const FilterNavigationMetadata& metadata,
                            bool eligible,
                            std::string_view reason = "") {
  if (reason.empty()) {
    MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                         LogEventType::kUrlEligibilityCheck,
                         metadata.url.GetHost())
        << LogDetail{"eligible", eligible};
  } else {
    MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                         LogEventType::kUrlEligibilityCheck,
                         metadata.url.GetHost())
        << LogDetail{"eligible", eligible}
        << LogDetail{"reason", std::string(reason)};
  }
}

}  // namespace

FilterTabController::FilterTabController(
    MultistepFilterService* service,
    MultistepFilterLogRouter* log_router,
    base::WeakPtr<MultistepFilterUiDelegate> delegate)
    : service_(CHECK_DEREF(service)),
      log_router_(log_router),
      delegate_(delegate) {}

FilterTabController::~FilterTabController() = default;

void FilterTabController::OnNavigationFinished(
    const FilterNavigationMetadata& metadata) {
  // Set up ScopedClosureRunners to ensure that the test observer is always
  // notified of pipeline completion (with std::nullopt results) even if the
  // navigation is determined to be ineligible and returns early. This is an
  // invariant that tests rely on to verify early abort paths without hanging.
  base::ScopedClosureRunner extraction_runner(
      base::BindOnce(&FilterTabController::OnExtractionFinished,
                     weak_ptr_factory_.GetWeakPtr(), metadata, std::nullopt));
  base::ScopedClosureRunner generation_runner(
      base::BindOnce(&FilterTabController::OnSuggestionGenerated,
                     weak_ptr_factory_.GetWeakPtr(), std::nullopt));

  bool is_same_page =
      metadata.is_same_document_navigation || metadata.url == metadata.prev_url;
  if (!is_same_page && delegate_) {
    LogSuggestionCleared(log_router_, metadata);
    delegate_->ClearSuggestion();
  }

  if (metadata.is_error_page_navigation) {
    LogUrlEligibilityCheck(log_router_, metadata,
                           /*eligible=*/false, "error_page");
    return;
  }

  bool is_unsupported_scheme = !metadata.is_cryptographic_scheme &&
                               !metadata.is_http_allowed_for_testing;
  if (is_unsupported_scheme) {
    LogUrlEligibilityCheck(log_router_, metadata,
                           /*eligible=*/false, "non_cryptographic");
    return;
  }

  if (metadata.url == metadata.prev_url &&
      !metadata.is_same_document_navigation) {
    LogUrlEligibilityCheck(log_router_, metadata,
                           /*eligible=*/false, "same_url_non_same_document");
    return;
  }

  if (!metadata.has_user_gesture) {
    LogUrlEligibilityCheck(log_router_, metadata,
                           /*eligible=*/false, "no_user_gesture");
    return;
  }

  if (!service_->HasUserProvidedConsent(metadata.navigation_id,
                                        metadata.url.GetHost())) {
    LogUrlEligibilityCheck(log_router_, metadata,
                           /*eligible=*/false, "no_user_consent");
    return;
  }

  LogUrlEligibilityCheck(log_router_, metadata, /*eligible=*/true);
}

void FilterTabController::OnSuggestionGenerated(
    std::optional<UrlFilterSuggestion> suggestion) {
  if (delegate_) {
    delegate_->OnSuggestionGenerated(suggestion);
  }
  if (observer_for_test_) {
    observer_for_test_->OnSuggestionGeneratedForTest(suggestion);  // IN-TEST
  }
}

void FilterTabController::OnExtractionFinished(
    const FilterNavigationMetadata& metadata,
    std::optional<FilterAnnotation> annotation) {
  if (observer_for_test_) {
    observer_for_test_->OnExtractionFinishedForTest(  // IN-TEST
        annotation ? std::optional(annotation->id) : std::nullopt);
  }
}

}  // namespace multistep_filter
