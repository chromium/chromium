// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/multistep_filter_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "components/history/core/browser/history_service.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

void LogUrlEligibilityCheck(MultistepFilterLogRouter* log_router,
                            int64_t navigation_id,
                            std::string_view domain,
                            bool signed_in,
                            bool url_allowed,
                            bool consent_enabled) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kUrlEligibilityCheck, domain)
      << LogDetail{"signed_in", signed_in}
      << LogDetail{"url_allowed", url_allowed}
      << LogDetail{"consent_enabled", consent_enabled};
}

void LogExtractionStarted(MultistepFilterLogRouter* log_router,
                          int64_t navigation_id,
                          std::string_view domain,
                          const GURL& url) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kAnnotationExtractionStarted, domain)
      << LogDetail{"url", url.spec()};
}

void LogSuggestionGenerationStarted(MultistepFilterLogRouter* log_router,
                                    int64_t navigation_id,
                                    std::string_view domain,
                                    const GURL& url) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionGenerationStarted, domain)
      << LogDetail{"url", url.spec()};
}
void LogAnnotationsExpired(MultistepFilterLogRouter* log_router,
                           int64_t navigation_id,
                           std::string_view domain,
                           std::optional<int64_t> count) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kSuggestionCleared, domain)
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

}  // namespace

MultistepFilterService::MultistepFilterService(Params params)
    : annotation_index_client_(std::move(params.annotation_index_client)),
      filter_store_(std::move(params.filter_store)),
      identity_manager_(params.identity_manager),
      consent_helper_(std::move(params.consent_helper)),
      log_router_(params.log_router) {
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

void MultistepFilterService::ExtractAnnotation(int64_t navigation_id,
                                               const GURL& url) {
  const std::string domain = GetEtldPlusOne(url);
  const bool signed_in = IsUserSignedIn();
  const bool consent_enabled = IsUrlKeyedDataCollectionEnabled();
  const bool url_allowed = signed_in && consent_enabled && IsUrlAllowed(url);

  LogUrlEligibilityCheck(log_router_, navigation_id, domain, signed_in,
                         url_allowed, consent_enabled);

  if (!url_allowed) {
    if (observer_for_test_) {
      observer_for_test_->OnExtractionFinished(std::nullopt);
    }
    return;
  }

  LogExtractionStarted(log_router_, navigation_id, domain, url);

  filter_extractor_->ExtractAnnotationFromUrl(
      url,
      base::BindOnce(&MultistepFilterService::OnExtractionFinished,
                     weak_ptr_factory_.GetWeakPtr()),
      navigation_id, domain);
}

void MultistepFilterService::GenerateFilterSuggestions(
    int64_t navigation_id,
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback) {
  if (callback.is_null()) {
    return;
  }

  const std::string domain = GetEtldPlusOne(url);
  const bool signed_in = IsUserSignedIn();
  const bool consent_enabled = IsUrlKeyedDataCollectionEnabled();
  const bool url_allowed = signed_in && consent_enabled && IsUrlAllowed(url);

  LogUrlEligibilityCheck(log_router_, navigation_id, domain, signed_in,
                         url_allowed, consent_enabled);

  if (!url_allowed) {
    if (observer_for_test_) {
      observer_for_test_->OnSuggestionGenerated(std::nullopt);
    }
    std::move(callback).Run(std::nullopt);
    return;
  }

  LogSuggestionGenerationStarted(log_router_, navigation_id, domain, url);

  filter_suggestion_generator_->GenerateSuggestion(
      url,
      base::BindOnce(&MultistepFilterService::OnSuggestionGenerated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      navigation_id, domain);
}

void MultistepFilterService::OnExtractionFinished(
    std::optional<base::Uuid> annotation_id) {
  if (observer_for_test_) {
    observer_for_test_->OnExtractionFinished(annotation_id);
  }
}

void MultistepFilterService::DeleteAnnotationsForTask(
    std::string_view task_type,
    int64_t navigation_id,
    std::string_view domain) {
  filter_store_->DeleteAnnotationsForTask(
      std::string(task_type),
      base::BindOnce(&LogAnnotationsExpired, log_router_, navigation_id,
                     std::string(domain)));
}

void MultistepFilterService::OnSuggestionGenerated(
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
    std::optional<UrlFilterSuggestion> suggestion) {
  if (observer_for_test_) {
    observer_for_test_->OnSuggestionGenerated(suggestion);
  }
  std::move(callback).Run(std::move(suggestion));
}

bool MultistepFilterService::IsUserSignedIn() const {
  return identity_manager_ &&
         identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

bool MultistepFilterService::IsUrlKeyedDataCollectionEnabled() const {
  return consent_helper_ && consent_helper_->IsEnabled();
}

void MultistepFilterService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    filter_store_->ClearData();
    LogHistoryDeleted(log_router_, /*is_all_history=*/true, std::nullopt);
    return;
  }

  std::vector<std::string> deleted_domains;
  for (const history::URLRow& url_row : deletion_info.deleted_rows()) {
    deleted_domains.push_back(GetEtldPlusOne(url_row.url()));
  }

  // If the time range is invalid (e.g., when specific URLs are deleted from
  // history), fall back to clearing the domains for all time. Reusing the
  // existing parameterized query with minimum/maximum boundaries avoids the
  // need to compile and index a separate no-time-range SQL query.
  base::Time begin_time = deletion_info.time_range().IsValid()
                              ? deletion_info.time_range().begin()
                              : base::Time();
  base::Time end_time = deletion_info.time_range().IsValid()
                            ? deletion_info.time_range().end()
                            : base::Time::Max();

  filter_store_->DeleteAnnotationsForDomains(
      std::move(deleted_domains), begin_time, end_time,
      base::BindOnce(&LogHistoryDeleted, log_router_,
                     /*is_all_history=*/false));
}

}  // namespace multistep_filter
