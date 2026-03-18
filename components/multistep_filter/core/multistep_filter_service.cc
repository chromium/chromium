// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/multistep_filter_service.h"

#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/gurl.h"

namespace multistep_filter {

MultistepFilterService::MultistepFilterService(
    std::unique_ptr<AnnotationIndexClient> annotation_index_client,
    std::unique_ptr<FilterStore> filter_store,
    std::unique_ptr<FilterExtractor> filter_extractor,
    std::unique_ptr<FilterSuggestionGenerator> filter_suggestion_generator,
    signin::IdentityManager* identity_manager)
    : annotation_index_client_(std::move(annotation_index_client)),
      filter_store_(std::move(filter_store)),
      filter_extractor_(std::move(filter_extractor)),
      filter_suggestion_generator_(std::move(filter_suggestion_generator)),
      identity_manager_(identity_manager) {
  CHECK(annotation_index_client_);
  CHECK(filter_store_);
  CHECK(filter_extractor_);
  CHECK(filter_suggestion_generator_);
}

MultistepFilterService::~MultistepFilterService() = default;

void MultistepFilterService::ExtractAnnotation(const GURL& url) {
  // Extract filter annotations for signed-in users only.
  if (IsUserSignedIn() && IsUrlAllowed(url)) {
    filter_extractor_->ExtractAnnotationFromUrl(url);
  }
}

void MultistepFilterService::GenerateFilterSuggestions(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback) {
  if (!callback) {
    return;
  }

  // Generate filter suggestions for signed-in users only.
  if (IsUserSignedIn() && IsUrlAllowed(url)) {
    filter_suggestion_generator_->GenerateSuggestion(url, std::move(callback));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

bool MultistepFilterService::IsUserSignedIn() const {
  return identity_manager_ &&
         identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

}  // namespace multistep_filter
