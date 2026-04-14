// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"

namespace multistep_filter {

FilterAttributeUiLabel::FilterAttributeUiLabel(
    FilterSuggestionCandidateAttribute candidate_attribute,
    FilterAttribute annotation_attribute)
    : attribute_label(std::move(candidate_attribute.label)),
      attribute_value(base::UTF8ToUTF16(annotation_attribute.value)) {
  CHECK_EQ(candidate_attribute.key, annotation_attribute.key);
}

std::string FilterAttributeUiLabel::ToString() const {
  return base::StrCat(
      {"FilterAttributeUiLabel(label=", base::UTF16ToUTF8(attribute_label),
       ", value=", base::UTF16ToUTF8(attribute_value), ")"});
}

UrlFilterSuggestion::UrlFilterSuggestion(
    GURL navigation_url,
    std::u16string source_domain,
    base::Time extraction_timestamp,
    std::vector<FilterAttributeUiLabel> attribute_ui_labels)
    : navigation_url(std::move(navigation_url)),
      source_domain(std::move(source_domain)),
      extraction_timestamp(extraction_timestamp),
      attribute_ui_labels(std::move(attribute_ui_labels)) {}

UrlFilterSuggestion::UrlFilterSuggestion(const UrlFilterSuggestion&) = default;
UrlFilterSuggestion::UrlFilterSuggestion(UrlFilterSuggestion&&) = default;
UrlFilterSuggestion& UrlFilterSuggestion::operator=(
    const UrlFilterSuggestion&) = default;
UrlFilterSuggestion& UrlFilterSuggestion::operator=(UrlFilterSuggestion&&) =
    default;

UrlFilterSuggestion::~UrlFilterSuggestion() = default;

std::string UrlFilterSuggestion::ToString() const {
  std::vector<std::string> attribute_strings;
  for (const auto& label : attribute_ui_labels) {
    attribute_strings.push_back(label.ToString());
  }

  return base::StrCat(
      {"UrlFilterSuggestion(navigation_url=", navigation_url.spec(),
       ", source_domain=", base::UTF16ToUTF8(source_domain),
       ", extraction_timestamp=",
       base::NumberToString(
           extraction_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds()),
       ", attribute_ui_labels=[", base::JoinString(attribute_strings, ", "),
       "])"});
}

}  // namespace multistep_filter
