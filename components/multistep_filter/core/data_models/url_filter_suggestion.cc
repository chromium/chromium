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
    : key(annotation_attribute.key),
      attribute_label(std::move(candidate_attribute.label)),
      attribute_value(base::UTF8ToUTF16(annotation_attribute.value)) {
  CHECK_EQ(candidate_attribute.key, annotation_attribute.key);
}

std::string FilterAttributeUiLabel::ToString() const {
  return base::StrCat({"FilterAttributeUiLabel(key=", key,
                       ", label=", base::UTF16ToUTF8(attribute_label),
                       ", value=", base::UTF16ToUTF8(attribute_value), ")"});
}

UrlFilterSuggestion::UrlFilterSuggestion(Params params)
    : navigation_url(std::move(params.navigation_url)),
      source_domain(std::move(params.source_domain)),
      extraction_timestamp(params.extraction_timestamp),
      attribute_ui_labels(std::move(params.attribute_ui_labels)),
      triggering_navigation_id(params.triggering_navigation_id),
      triggering_domain(std::move(params.triggering_domain)),
      task_type(std::move(params.task_type)),
      suggestion_message(std::move(params.suggestion_message)) {}

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

  std::string message_suffix =
      !suggestion_message.empty()
          ? base::StrCat({", suggestion_message=",
                          base::UTF16ToUTF8(suggestion_message)})
          : "";

  return base::StrCat(
      {"UrlFilterSuggestion(navigation_url=", navigation_url.spec(),
       ", source_domain=", base::UTF16ToUTF8(source_domain),
       ", extraction_timestamp=",
       base::NumberToString(
           extraction_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds()),
       ", attribute_ui_labels=[", base::JoinString(attribute_strings, ", "),
       "], triggering_navigation_id=",
       base::NumberToString(triggering_navigation_id), ", triggering_domain=",
       triggering_domain, ", task_type=", task_type, message_suffix, ")"});
}

}  // namespace multistep_filter
