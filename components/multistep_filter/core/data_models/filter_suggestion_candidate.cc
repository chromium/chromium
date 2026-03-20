// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"

#include <string>
#include <utility>
#include <vector>

#include "url/gurl.h"

namespace multistep_filter {

FilterSuggestionCandidateAttribute::FilterSuggestionCandidateAttribute(
    std::string key,
    std::string label)
    : key(std::move(key)), label(std::move(label)) {}

FilterSuggestionCandidate::FilterSuggestionCandidate(
    std::string filter_annotation_id,
    GURL navigation_url,
    std::vector<FilterSuggestionCandidateAttribute> attributes)
    : filter_annotation_id(std::move(filter_annotation_id)),
      navigation_url(std::move(navigation_url)),
      attributes(std::move(attributes)) {}

FilterSuggestionCandidate::FilterSuggestionCandidate(
    const FilterSuggestionCandidate&) = default;
FilterSuggestionCandidate::FilterSuggestionCandidate(
    FilterSuggestionCandidate&&) = default;
FilterSuggestionCandidate& FilterSuggestionCandidate::operator=(
    const FilterSuggestionCandidate&) = default;
FilterSuggestionCandidate& FilterSuggestionCandidate::operator=(
    FilterSuggestionCandidate&&) = default;

FilterSuggestionCandidate::~FilterSuggestionCandidate() = default;

bool operator==(const FilterSuggestionCandidate&,
                const FilterSuggestionCandidate&) = default;

}  // namespace multistep_filter
