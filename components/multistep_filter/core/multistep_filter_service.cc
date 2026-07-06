// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/multistep_filter_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "components/history/core/browser/history_service.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/multistep_filter/core/verification/filter_application_verifier.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

void LogUrlEligibilityCheck(MultistepFilterLogRouter* log_router,
                            int64_t navigation_id,
                            std::string_view host,
                            bool signed_in,
                            bool url_keyed_data_collection_enabled,
                            bool history_sync_enabled) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kUrlEligibilityCheck, host)
      << LogDetail{"signed_in", signed_in}
      << LogDetail{"url_keyed_data_collection_enabled",
                   url_keyed_data_collection_enabled}
      << LogDetail{"history_sync_enabled", history_sync_enabled};
}

void LogExtractionStarted(MultistepFilterLogRouter* log_router,
                          int64_t navigation_id,
                          std::string_view host,
                          const GURL& url) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kAnnotationExtractionStarted, host)
      << LogDetail{"url", url.spec()};
}

void LogSuggestionGenerationStarted(MultistepFilterLogRouter* log_router,
                                    int64_t navigation_id,
                                    std::string_view host,
                                    const GURL& url) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionGenerationStarted, host)
      << LogDetail{"url", url.spec()};
}
void LogAnnotationsExpired(MultistepFilterLogRouter* log_router,
                           int64_t navigation_id,
                           std::string_view host,
                           std::optional<int64_t> count) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionCleared, host)
      << LogDetail{"success", count.has_value()}
      << LogDetail{"expired_count", static_cast<int>(count.value_or(0))};
}

void LogHistoryDeleted(MultistepFilterLogRouter* log_router,
                       bool is_all_history,
                       std::optional<int64_t> rows_deleted) {
  const std::string reason = is_all_history ? "Full history wipe requested"
                                            : "Partial history cleared";

  MULTISTEP_FILTER_LOG(log_router, 0, LogEventType::kSuggestionCleared,
                       "History")
      << LogDetail{"reason", reason}
      << LogDetail{"rows_deleted", static_cast<int>(rows_deleted.value_or(0))};
}

void LogExtractionFailed(MultistepFilterLogRouter* log_router,
                         int64_t navigation_id,
                         std::string_view host,
                         std::string_view reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kAnnotationsExtracted, host)
      << LogDetail{"success", false}
      << LogDetail{"reason", std::string(reason)};
}

void LogSuggestionSuppressed(MultistepFilterLogRouter* log_router,
                             int64_t navigation_id,
                             std::string_view host,
                             std::string_view reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionSuppressed, host)
      << LogDetail{"reason", std::string(reason)};
}

void LogSuggestionApplicationOutcome(
    MultistepFilterLogRouter* log_router,
    int64_t navigation_id,
    std::string_view host,
    const std::optional<UrlFilterSuggestion>& suggested_filters,
    const std::optional<FilterAnnotation>& extracted_annotation) {
  if (!suggested_filters) {
    return;
  }
  if (!extracted_annotation) {
    MULTISTEP_FILTER_LOG(log_router, navigation_id,
                         LogEventType::kSuggestionApplied, host)
        << LogDetail{"application_outcome", "error_no_extracted_annotations"};

    return;
  }
  const FilterApplicationVerifier::Result result =
      FilterApplicationVerifier::Verify(*suggested_filters,
                                        *extracted_annotation);

  switch (result.outcome) {
    case FilterApplicationVerifier::Result::Outcome::kNoExtractedAnnotations:
      MULTISTEP_FILTER_LOG(log_router, navigation_id,
                           LogEventType::kSuggestionApplied, host)
          << LogDetail{"application_outcome", "error_no_extracted_annotations"};
      break;
    case FilterApplicationVerifier::Result::Outcome::kCountMismatch:
      MULTISTEP_FILTER_LOG(log_router, navigation_id,
                           LogEventType::kSuggestionApplied, host)
          << LogDetail{"application_outcome", "error_filter_count_mismatch"};
      break;
    case FilterApplicationVerifier::Result::Outcome::kSuccess:
      MULTISTEP_FILTER_LOG(log_router, navigation_id,
                           LogEventType::kSuggestionApplied, host)
          << LogDetail{"application_outcome", "success"};
      break;
    case FilterApplicationVerifier::Result::Outcome::kAttributeMismatch:
      MULTISTEP_FILTER_LOG(log_router, navigation_id,
                           LogEventType::kSuggestionApplied, host)
          << LogDetail{"application_outcome", "error_attribute_mismatch"}
          << LogDetail{"missing_filter_keys",
                       base::JoinString(result.missing_keys, ", ")};
      break;
  }
}

}  // namespace

MultistepFilterService::MultistepFilterService(Params params)
    : annotation_index_client_(std::move(params.annotation_index_client)),
      filter_store_(std::move(params.filter_store)),
      identity_manager_(params.identity_manager),
      consent_helper_(std::move(params.consent_helper)),
      log_router_(params.log_router),
      pref_service_(params.pref_service),
      sync_service_(params.sync_service) {
  CHECK(annotation_index_client_);
  CHECK(filter_store_);
  filter_extractor_ = std::make_unique<FilterExtractor>(
      *annotation_index_client_, *filter_store_, log_router_);
  filter_suggestion_generator_ = std::make_unique<FilterSuggestionGenerator>(
      *annotation_index_client_, *filter_store_, log_router_);

  if (params.history_service) {
    history_service_observation_.Observe(params.history_service);
  }
}

MultistepFilterService::~MultistepFilterService() = default;

void MultistepFilterService::Shutdown() {
  history_service_observation_.Reset();
}

void MultistepFilterService::RecordSuggestionImpression() {
  if (!pref_service_) {
    return;
  }
  RecordImpression(pref_service_);
}

void MultistepFilterService::RecordUserInteractionWithSuggestion(
    SuggestionUserDecision decision) {
  if (!pref_service_) {
    return;
  }
  RecordUserInteraction(pref_service_, decision);
}

void MultistepFilterService::ExtractAnnotation(
    int64_t navigation_id,
    const GURL& url,
    std::optional<UrlFilterSuggestion> applied_suggestion) {
  if (!HasUserProvidedConsent(navigation_id, url.GetHost())) {
    OnExtractionFinished(applied_suggestion, url.GetHost(), navigation_id,
                         std::nullopt);
    return;
  }

  GetSupportedTaskForUrl(
      url,
      base::BindOnce(&MultistepFilterService::OnUrlAllowedForExtraction,
                     weak_ptr_factory_.GetWeakPtr(), url, navigation_id,
                     std::move(applied_suggestion)),
      navigation_id);
}

void MultistepFilterService::OnUrlAllowedForExtraction(
    const GURL& url,
    int64_t navigation_id,
    std::optional<UrlFilterSuggestion> applied_suggestion,
    std::vector<std::string> supported_task_types) {
  if (supported_task_types.empty()) {
    LogExtractionFailed(log_router_, navigation_id, url.GetHost(),
                        "no_supported_tasks");
    OnExtractionFinished(applied_suggestion, url.GetHost(), navigation_id,
                         std::nullopt);
    return;
  }

  LogExtractionStarted(log_router_, navigation_id, url.GetHost(), url);

  filter_extractor_->ExtractAnnotationFromUrl(
      url,
      base::BindOnce(&MultistepFilterService::OnExtractionFinished,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(applied_suggestion), url.GetHost(),
                     navigation_id),
      navigation_id);
}

void MultistepFilterService::OnExtractionFinished(
    std::optional<UrlFilterSuggestion> applied_suggestion,
    std::string host,
    int64_t navigation_id,
    std::optional<FilterAnnotation> annotation) {
  LogSuggestionApplicationOutcome(log_router_, navigation_id, host,
                                  applied_suggestion, annotation);
  if (observer_for_test_) {
    observer_for_test_->OnExtractionFinished(
        annotation ? std::optional(annotation->id) : std::nullopt);
  }
}

void MultistepFilterService::GenerateFilterSuggestions(
    int64_t navigation_id,
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback) {
  if (callback.is_null()) {
    return;
  }

  if (!HasUserProvidedConsent(navigation_id, url.GetHost())) {
    if (observer_for_test_) {
      observer_for_test_->OnSuggestionGenerated(std::nullopt);
    }
    std::move(callback).Run(std::nullopt);
    return;
  }

  GetSupportedTaskForUrl(
      url,
      base::BindOnce(&MultistepFilterService::OnUrlAllowedForSuggestion,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback),
                     navigation_id),
      navigation_id);
}

void MultistepFilterService::OnUrlAllowedForSuggestion(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
    int64_t navigation_id,
    std::vector<std::string> supported_task_types) {
  if (supported_task_types.empty()) {
    LogSuggestionSuppressed(log_router_, navigation_id, url.GetHost(),
                            "no_supported_tasks");
    if (observer_for_test_) {
      observer_for_test_->OnSuggestionGenerated(std::nullopt);
    }
    std::move(callback).Run(std::nullopt);
    return;
  }

  LogSuggestionGenerationStarted(log_router_, navigation_id, url.GetHost(),
                                 url);

  filter_suggestion_generator_->GenerateSuggestion(
      url, std::move(supported_task_types),
      base::BindOnce(&MultistepFilterService::OnSuggestionGenerated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      navigation_id);
}

void MultistepFilterService::OnSuggestionGenerated(
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
    std::optional<UrlFilterSuggestion> suggestion) {
  if (observer_for_test_) {
    observer_for_test_->OnSuggestionGenerated(suggestion);
  }
  std::move(callback).Run(std::move(suggestion));
}

void MultistepFilterService::DeleteAnnotationsForTask(
    std::string_view task_type,
    int64_t navigation_id,
    std::string_view host) {
  filter_store_->DeleteAnnotationsForTask(
      std::string(task_type),
      base::BindOnce(&LogAnnotationsExpired, log_router_, navigation_id,
                     std::string(host)));
}

bool MultistepFilterService::HasUserProvidedConsent(int64_t navigation_id,
                                                    std::string_view host) {
  const bool signed_in = IsUserSignedIn();
  const bool url_keyed_data_collection_enabled =
      IsUrlKeyedDataCollectionEnabled();
  const bool history_sync_enabled = IsHistorySyncEnabled();
  const bool consent_enabled =
      signed_in && url_keyed_data_collection_enabled && history_sync_enabled;

  LogUrlEligibilityCheck(log_router_, navigation_id, host, signed_in,
                         url_keyed_data_collection_enabled,
                         history_sync_enabled);
  return consent_enabled;
}

void MultistepFilterService::GetSupportedTaskForUrl(
    const GURL& url,
    base::OnceCallback<void(std::vector<std::string>)> callback,
    int64_t navigation_id) {
  annotation_index_client_->GetSupportedTasks(url, std::move(callback),
                                              navigation_id);
}

bool MultistepFilterService::IsUserSignedIn() const {
  return identity_manager_ &&
         identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

bool MultistepFilterService::IsUrlKeyedDataCollectionEnabled() const {
  return consent_helper_ && consent_helper_->IsEnabled();
}

bool MultistepFilterService::IsHistorySyncEnabled() const {
  return sync_service_ &&
         sync_service_->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kHistory);
}

void MultistepFilterService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    filter_store_->ClearData();
    LogHistoryDeleted(log_router_, /*is_all_history=*/true, std::nullopt);
    return;
  }

  std::vector<std::string> deleted_hosts;
  for (const history::URLRow& url_row : deletion_info.deleted_rows()) {
    deleted_hosts.push_back(url_row.url().GetHost());
  }

  // If the time range is invalid (e.g., when specific URLs are deleted from
  // history), fall back to clearing the hosts for all time. Reusing the
  // existing parameterized query with minimum/maximum boundaries avoids the
  // need to compile and index a separate no-time-range SQL query.
  base::Time begin_time = deletion_info.time_range().IsValid()
                              ? deletion_info.time_range().begin()
                              : base::Time();
  base::Time end_time = deletion_info.time_range().IsValid()
                            ? deletion_info.time_range().end()
                            : base::Time::Max();

  filter_store_->DeleteAnnotationsForHosts(
      std::move(deleted_hosts), begin_time, end_time,
      base::BindOnce(&LogHistoryDeleted, log_router_,
                     /*is_all_history=*/false));
}

}  // namespace multistep_filter
