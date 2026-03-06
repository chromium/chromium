// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"

#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "url/gurl.h"

namespace multistep_filter {

FilterSuggestionGenerator::FilterSuggestionGenerator() = default;

FilterSuggestionGenerator::~FilterSuggestionGenerator() = default;

void FilterSuggestionGenerator::GenerateSuggestion(
    const GURL& url,
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback) {
  // TODO crbug.com/489001569: Implement core filter actor suggestion logic.
  NOTIMPLEMENTED();
  std::move(callback).Run(std::nullopt);
}

}  // namespace multistep_filter
