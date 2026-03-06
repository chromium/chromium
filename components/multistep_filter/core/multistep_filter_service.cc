// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/multistep_filter_service.h"

#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/gurl.h"

namespace multistep_filter {

MultistepFilterService::MultistepFilterService(
    std::unique_ptr<FilterSuggestionGenerator> filter_suggestion_generator,
    signin::IdentityManager* identity_manager)
    : filter_suggestion_generator_(std::move(filter_suggestion_generator)),
      identity_manager_(identity_manager) {}

MultistepFilterService::~MultistepFilterService() = default;

void MultistepFilterService::GenerateFilterSuggestions(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback) {
  // The Multistep Filter feature is only available for signed-in users.
  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // TODO crbug.com/484314324: Implement filter actor orchestration logic.
  if (filter_suggestion_generator_) {
    filter_suggestion_generator_->GenerateSuggestion(url, std::move(callback));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}
}  // namespace multistep_filter
