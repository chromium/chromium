// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/filter_tab_controller.h"

#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/multistep_filter/core/verification/filter_application_verifier.h"

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

void LogAnnotationExtractionStarted(MultistepFilterLogRouter* log_router,
                                    const FilterNavigationMetadata& metadata) {
  MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                       LogEventType::kAnnotationExtractionStarted,
                       metadata.url.GetHost());
}

void LogSuggestionSuppressed(MultistepFilterLogRouter* log_router,
                             const FilterNavigationMetadata& metadata,
                             std::string_view reason) {
  MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                       LogEventType::kSuggestionSuppressed,
                       metadata.url.GetHost())
      << LogDetail{"reason", std::string(reason)};
}

void LogSuggestionGenerationStarted(MultistepFilterLogRouter* log_router,
                                    const FilterNavigationMetadata& metadata) {
  MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                       LogEventType::kSuggestionGenerationStarted,
                       metadata.url.GetHost());
}
void LogSuggestionApplicationOutcome(
    MultistepFilterLogRouter* log_router,
    const FilterNavigationMetadata& metadata,
    const std::optional<UrlFilterSuggestion>& suggested_filters,
    const std::optional<FilterAnnotation>& extracted_annotation) {
  if (!suggested_filters) {
    return;
  }

  bool is_unsupported_scheme = !metadata.is_cryptographic_scheme &&
                               !metadata.is_http_allowed_for_testing;
  if (metadata.is_error_page_navigation || is_unsupported_scheme) {
    MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                         LogEventType::kSuggestionApplied,
                         metadata.url.GetHost())
        << LogDetail{"application_outcome", "failure"}
        << LogDetail{"is_error_page", metadata.is_error_page_navigation}
        << LogDetail{"net_error_code", metadata.net_error_code}
        << LogDetail{"http_response_code", metadata.http_response_code};
    return;
  }

  if (!extracted_annotation) {
    MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                         LogEventType::kSuggestionApplied,
                         metadata.url.GetHost())
        << LogDetail{"application_outcome", "error_no_extracted_annotations"};
    return;
  }
  const FilterApplicationVerifier::Result result =
      FilterApplicationVerifier::Verify(*suggested_filters,
                                        *extracted_annotation);

  switch (result.outcome) {
    case FilterApplicationVerifier::Result::Outcome::kNoExtractedAnnotations:
      MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                           LogEventType::kSuggestionApplied,
                           metadata.url.GetHost())
          << LogDetail{"application_outcome", "error_no_extracted_annotations"};
      break;
    case FilterApplicationVerifier::Result::Outcome::kCountMismatch:
      MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                           LogEventType::kSuggestionApplied,
                           metadata.url.GetHost())
          << LogDetail{"application_outcome", "error_filter_count_mismatch"};
      break;
    case FilterApplicationVerifier::Result::Outcome::kSuccess:
      MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                           LogEventType::kSuggestionApplied,
                           metadata.url.GetHost())
          << LogDetail{"application_outcome", "success"};
      break;
    case FilterApplicationVerifier::Result::Outcome::kAttributeMismatch:
      MULTISTEP_FILTER_LOG(log_router, metadata.navigation_id,
                           LogEventType::kSuggestionApplied,
                           metadata.url.GetHost())
          << LogDetail{"application_outcome", "error_attribute_mismatch"}
          << LogDetail{"missing_filter_keys",
                       base::JoinString(result.missing_keys, ", ")};
      break;
  }
}
}  // namespace

FilterTabController::FilterTabController(
    MultistepFilterService* service,
    MultistepFilterLogRouter* log_router,
    MultistepFilterUiDelegate* delegate,
    FilterStore* filter_store,
    AnnotationIndexClient* annotation_client)
    : service_(CHECK_DEREF(service)),
      filter_store_(CHECK_DEREF(filter_store)),
      annotation_client_(CHECK_DEREF(annotation_client)),
      log_router_(log_router),
      delegate_(CHECK_DEREF(delegate)) {
  filter_extractor_ = std::make_unique<FilterExtractor>(
      *annotation_client_, *filter_store_, log_router_);
  filter_suggestion_generator_ = std::make_unique<FilterSuggestionGenerator>(
      *annotation_client_, *filter_store_, log_router_);
}

FilterTabController::~FilterTabController() = default;

base::WeakPtr<FilterTabController> FilterTabController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FilterTabController::OnNavigationFinished(
    const FilterNavigationMetadata& metadata) {
  metrics_tracker_.OnNavigationFinished(metadata);
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Set up ScopedClosureRunners to ensure that the test observer is always
  // notified of pipeline completion (with std::nullopt results) even if the
  // navigation is determined to be ineligible and returns early. This is an
  // invariant that tests rely on to verify early abort paths without hanging.
  //
  // Exception: For same-url reloads (same_url_non_same_document), we explicitly
  // release these runners because we want to preserve any existing suggestion
  // rather than triggering the fallback cleanups.
  base::ScopedClosureRunner extraction_runner_fallback(
      base::BindOnce(&FilterTabController::OnExtractionFinished, GetWeakPtr(),
                     metadata, std::nullopt));
  base::ScopedClosureRunner generation_runner_fallback(base::BindOnce(
      &FilterTabController::OnSuggestionGenerated, GetWeakPtr(), std::nullopt));

  bool is_same_page =
      metadata.is_same_document_navigation || metadata.url == metadata.prev_url;
  if (!is_same_page) {
    LogSuggestionCleared(log_router_, metadata);
    delegate_->ClearSuggestion();
  }

  if (metadata.is_error_page_navigation) {
    LogUrlEligibilityCheck(log_router_, metadata, /*eligible=*/false,
                           "error_page");
    return;
  }

  bool is_unsupported_scheme = !metadata.is_cryptographic_scheme &&
                               !metadata.is_http_allowed_for_testing;
  if (is_unsupported_scheme) {
    LogUrlEligibilityCheck(log_router_, metadata, /*eligible=*/false,
                           "non_cryptographic");
    return;
  }

  if (metadata.url == metadata.prev_url &&
      !metadata.is_same_document_navigation) {
    LogUrlEligibilityCheck(log_router_, metadata, /*eligible=*/false,
                           "same_url_non_same_document");
    std::ignore = extraction_runner_fallback.Release();
    std::ignore = generation_runner_fallback.Release();
    return;
  }

  if (!metadata.has_user_gesture) {
    LogUrlEligibilityCheck(log_router_, metadata, /*eligible=*/false,
                           "no_user_gesture");
    return;
  }

  if (!service_->HasUserProvidedConsent(metadata.navigation_id,
                                        metadata.url.GetHost())) {
    LogUrlEligibilityCheck(log_router_, metadata, /*eligible=*/false,
                           "no_user_consent");
    return;
  }

  annotation_client_->GetSupportedTasks(
      metadata.url,
      base::BindOnce(&FilterTabController::OnSupportedTasksFetched,
                     GetWeakPtr(), metadata,
                     std::move(extraction_runner_fallback),
                     std::move(generation_runner_fallback)),
      metadata.navigation_id);
}

void FilterTabController::OnSuggestionShown(
    const UrlFilterSuggestion& suggestion) {
  service_->RecordSuggestionImpression();
  service_->DeleteAnnotationsForTask(suggestion.task_type,
                                     suggestion.triggering_navigation_id,
                                     suggestion.triggering_host);
  metrics_tracker_.OnSuggestionShown(suggestion);
}

void FilterTabController::OnSuggestionReopened() {
  metrics_tracker_.OnSuggestionReopened();
}

void FilterTabController::OnUserDecision(SuggestionUserDecision decision) {
  service_->RecordUserInteractionWithSuggestion(decision);
  metrics_tracker_.OnSuggestionUserInteraction(decision);
}

void FilterTabController::OnSupportedTasksFetched(
    const FilterNavigationMetadata& metadata,
    base::ScopedClosureRunner extraction_runner_fallback,
    base::ScopedClosureRunner generation_runner_fallback,
    std::vector<std::string> supported_task_types) {
  if (supported_task_types.empty()) {
    LogUrlEligibilityCheck(log_router_, metadata, /*eligible=*/false,
                           "no_supported_tasks");
    return;
  }

  LogAnnotationExtractionStarted(log_router_, metadata);
  filter_extractor_->ExtractAnnotationFromUrl(
      metadata.url,
      base::BindOnce(&FilterTabController::OnExtractionFinished, GetWeakPtr(),
                     metadata),
      metadata.navigation_id);
  std::ignore = extraction_runner_fallback.Release();

  if (metadata.was_filter_initiated_navigation) {
    LogSuggestionSuppressed(log_router_, metadata,
                            "filter_initiated_navigation");
    return;
  }

  LogSuggestionGenerationStarted(log_router_, metadata);
  filter_suggestion_generator_->GenerateSuggestion(
      metadata.url, supported_task_types,
      base::BindOnce(&FilterTabController::OnSuggestionGenerated, GetWeakPtr()),
      metadata.navigation_id);
  std::ignore = generation_runner_fallback.Release();
}

void FilterTabController::OnSuggestionGenerated(
    std::optional<UrlFilterSuggestion> suggestion) {
  if (suggestion) {
    delegate_->OnSuggestionGenerated(
        suggestion,
        MultistepFilterUiDelegate::SuggestionUiCallbacks{
            .on_suggestion_shown =
                base::BindOnce(&FilterTabController::OnSuggestionShown,
                               GetWeakPtr(), *suggestion),
            .on_suggestion_reopened = base::BindOnce(
                &FilterTabController::OnSuggestionReopened, GetWeakPtr()),
            .on_user_interaction = base::BindOnce(
                &FilterTabController::OnUserDecision, GetWeakPtr()),
        });
  } else {
    delegate_->OnSuggestionGenerated(std::nullopt, {});
  }
  if (observer_for_test_) {
    observer_for_test_->OnSuggestionGeneratedForTest(suggestion);  // IN-TEST
  }
}

void FilterTabController::OnExtractionFinished(
    const FilterNavigationMetadata& metadata,
    std::optional<FilterAnnotation> annotation) {
  LogSuggestionApplicationOutcome(log_router_, metadata,
                                  metadata.applied_suggestion, annotation);
  if (observer_for_test_) {
    observer_for_test_->OnExtractionFinishedForTest(  // IN-TEST
        annotation ? std::optional(annotation->id) : std::nullopt);
  }
}

}  // namespace multistep_filter
