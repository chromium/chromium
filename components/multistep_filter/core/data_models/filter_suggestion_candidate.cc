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
    std::u16string label,
    std::u16string value)
    : key(std::move(key)), label(std::move(label)), value(std::move(value)) {}

std::string FilterSuggestionCandidateAttribute::ToString() const {
  return base::StrCat({"FilterSuggestionCandidateAttribute(key=", key,
                       ", label=", base::UTF16ToUTF8(label),
                       ", value=", base::UTF16ToUTF8(value), ")"});
}

FilterSuggestionCandidate::FilterSuggestionCandidate(
    base::Uuid filter_annotation_id,
    GURL navigation_url,
    std::vector<FilterSuggestionCandidateAttribute> attributes,
    std::u16string short_text,
    std::u16string detailed_text)
    : filter_annotation_id(std::move(filter_annotation_id)),
      navigation_url(std::move(navigation_url)),
      attributes(std::move(attributes)),
      short_text(std::move(short_text)),
      detailed_text(std::move(detailed_text)) {}

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
  std::string short_text_suffix =
      !short_text.empty()
          ? base::StrCat({", short_text=", base::UTF16ToUTF8(short_text)})
          : "";
  std::string detailed_text_suffix =
      !detailed_text.empty()
          ? base::StrCat({", detailed_text=", base::UTF16ToUTF8(detailed_text)})
          : "";

  return base::StrCat({"FilterSuggestionCandidate(filter_annotation_id=",
                       filter_annotation_id.AsLowercaseString(),
                       ", navigation_url=", navigation_url.spec(),
                       ", attributes=[",
                       base::JoinString(attribute_strings, ", "), "]",
                       short_text_suffix, detailed_text_suffix, ")"});
}

bool operator==(const FilterSuggestionCandidate&,
                const FilterSuggestionCandidate&) = default;

}  // namespace multistep_filter
