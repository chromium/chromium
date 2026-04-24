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
      *annotation_index_client_, *filter_store_);
  filter_suggestion_generator_ = std::make_unique<FilterSuggestionGenerator>(
      *annotation_index_client_, *filter_store_);
}

MultistepFilterService::~MultistepFilterService() = default;

void MultistepFilterService::ExtractAnnotation(int64_t navigation_id,
                                               const GURL& url) {
  if (!IsUserSignedIn()) {
    MULTISTEP_FILTER_LOG(log_router_, navigation_id,
                         LogEventType::kUrlEligibilityCheck,
                         GetEtldPlusOne(url))
        << LogDetail{"signed_in", false} << LogDetail{"url_allowed", false};
    if (observer_for_test_) {
      observer_for_test_->OnExtractionFinished(std::nullopt);
    }
    return;
  }

  if (!IsUrlAllowed(url)) {
    MULTISTEP_FILTER_LOG(log_router_, navigation_id,
                         LogEventType::kUrlEligibilityCheck,
                         GetEtldPlusOne(url))
        << LogDetail{"signed_in", true} << LogDetail{"url_allowed", false};
    if (observer_for_test_) {
      observer_for_test_->OnExtractionFinished(std::nullopt);
    }
    return;
  }

  MULTISTEP_FILTER_LOG(log_router_, navigation_id,
                       LogEventType::kAnnotationExtractionStarted,
                       GetEtldPlusOne(url))
      << LogDetail{"url", url.spec()};

  filter_extractor_->ExtractAnnotationFromUrl(
      url, base::BindOnce(&MultistepFilterService::OnExtractionFinished,
                          weak_ptr_factory_.GetWeakPtr(), navigation_id, url));
}

void MultistepFilterService::GenerateFilterSuggestions(
    int64_t navigation_id,
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback) {
  if (callback.is_null()) {
    return;
  }

  if (!IsUserSignedIn()) {
    MULTISTEP_FILTER_LOG(log_router_, navigation_id,
                         LogEventType::kUrlEligibilityCheck,
                         GetEtldPlusOne(url))
        << LogDetail{"signed_in", false} << LogDetail{"url_allowed", false};
    if (observer_for_test_) {
      observer_for_test_->OnSuggestionGenerated(std::nullopt);
    }
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (!IsUrlAllowed(url)) {
    MULTISTEP_FILTER_LOG(log_router_, navigation_id,
                         LogEventType::kUrlEligibilityCheck,
                         GetEtldPlusOne(url))
        << LogDetail{"signed_in", true} << LogDetail{"url_allowed", false};
    if (observer_for_test_) {
      observer_for_test_->OnSuggestionGenerated(std::nullopt);
    }
    std::move(callback).Run(std::nullopt);
    return;
  }

  MULTISTEP_FILTER_LOG(log_router_, navigation_id,
                       LogEventType::kSuggestionGenerationStarted,
                       GetEtldPlusOne(url))
      << LogDetail{"url", url.spec()};

  filter_suggestion_generator_->GenerateSuggestion(
      url, base::BindOnce(&MultistepFilterService::OnSuggestionGenerated,
                          weak_ptr_factory_.GetWeakPtr(), navigation_id, url,
                          std::move(callback)));
}

void MultistepFilterService::OnExtractionFinished(
    int64_t navigation_id,
    const GURL& url,
    std::optional<base::Uuid> annotation_id) {
  MULTISTEP_FILTER_LOG(log_router_, navigation_id,
                       LogEventType::kAnnotationsExtracted, GetEtldPlusOne(url))
      << LogDetail{"success", annotation_id.has_value()};
  if (observer_for_test_) {
    observer_for_test_->OnExtractionFinished(annotation_id);
  }
}

void MultistepFilterService::OnSuggestionGenerated(
    int64_t navigation_id,
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
    std::optional<UrlFilterSuggestion> suggestion) {
  MULTISTEP_FILTER_LOG(log_router_, navigation_id,
                       LogEventType::kSuggestionGenerated, GetEtldPlusOne(url))
      << LogDetail{"success", suggestion.has_value()};
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
