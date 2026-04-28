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
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

void LogUrlEligibilityCheck(MultistepFilterLogRouter* log_router,
                            int64_t navigation_id,
                            std::string_view domain,
                            bool signed_in,
                            bool url_allowed) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kUrlEligibilityCheck, domain)
      << LogDetail{"signed_in", signed_in}
      << LogDetail{"url_allowed", url_allowed};
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

}  // namespace

MultistepFilterService::MultistepFilterService(
    std::unique_ptr<AnnotationIndexClient> annotation_index_client,
    std::unique_ptr<FilterStore> filter_store,
    signin::IdentityManager* identity_manager,
    MultistepFilterLogRouter* log_router)
    : annotation_index_client_(std::move(annotation_index_client)),
      filter_store_(std::move(filter_store)),
      identity_manager_(identity_manager),
      log_router_(log_router) {
  CHECK(annotation_index_client_);
  CHECK(filter_store_);
  filter_extractor_ = std::make_unique<FilterExtractor>(
      *annotation_index_client_, *filter_store_, log_router_);
  filter_suggestion_generator_ = std::make_unique<FilterSuggestionGenerator>(
      *annotation_index_client_, *filter_store_, log_router_);
}

MultistepFilterService::~MultistepFilterService() = default;

void MultistepFilterService::ExtractAnnotation(int64_t navigation_id,
                                               const GURL& url) {
  const std::string domain = GetEtldPlusOne(url);
  const bool signed_in = IsUserSignedIn();
  const bool url_allowed = signed_in && IsUrlAllowed(url);

  LogUrlEligibilityCheck(log_router_, navigation_id, domain, signed_in,
                         url_allowed);

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
  const bool url_allowed = signed_in && IsUrlAllowed(url);

  LogUrlEligibilityCheck(log_router_, navigation_id, domain, signed_in,
                         url_allowed);

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

}  // namespace multistep_filter
