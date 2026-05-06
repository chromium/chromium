// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "url/gurl.h"

namespace multistep_filter {

FilterSuggestionCandidateAttribute::FilterSuggestionCandidateAttribute(
    std::string key,
    std::u16string label)
    : key(std::move(key)), label(std::move(label)) {}

std::string FilterSuggestionCandidateAttribute::ToString() const {
  return base::StrCat({"FilterSuggestionCandidateAttribute(key=", key,
                       ", label=", base::UTF16ToUTF8(label), ")"});
}

FilterSuggestionCandidate::FilterSuggestionCandidate(
    base::Uuid filter_annotation_id,
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

std::string FilterSuggestionCandidate::ToString() const {
  std::vector<std::string> attribute_strings;
  for (const FilterSuggestionCandidateAttribute& attr : attributes) {
    attribute_strings.push_back(attr.ToString());
  }
  return base::StrCat({"FilterSuggestionCandidate(filter_annotation_id=",
                       filter_annotation_id.AsLowercaseString(),
                       ", navigation_url=", navigation_url.spec(),
                       ", attributes=[",
                       base::JoinString(attribute_strings, ", "), "])"});
}

bool operator==(const FilterSuggestionCandidate&,
                const FilterSuggestionCandidate&) = default;

}  // namespace multistep_filter
