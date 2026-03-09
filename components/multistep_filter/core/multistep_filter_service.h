// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"

class GURL;

namespace signin {
class IdentityManager;
}

namespace multistep_filter {

// Service to orchestrate Multistep Filter.
//
// Based on the user's past navigation history, this service stores and provides
// suggestions for filters to the user. It acts as the central
// coordinator for the Multistep Filter feature, managing the lifecycle of
// related components like the FilterSuggestionGenerator.
class MultistepFilterService : public KeyedService {
 public:
  MultistepFilterService(
      std::unique_ptr<FilterSuggestionGenerator> filter_suggestion_generator,
      signin::IdentityManager* identity_manager);
  MultistepFilterService(const MultistepFilterService&) = delete;
  MultistepFilterService& operator=(const MultistepFilterService&) = delete;

  ~MultistepFilterService() override;

  // Generates a filter suggestion for `url`. Based on URL analysis, the
  // suggestion may be stored for later use. Returns the result via `callback`.
  virtual void GenerateFilterSuggestions(
      const GURL& url,
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback);

  FilterSuggestionGenerator* filter_suggestion_generator() {
    return filter_suggestion_generator_.get();
  }

 private:
  std::unique_ptr<FilterSuggestionGenerator> filter_suggestion_generator_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_H_
