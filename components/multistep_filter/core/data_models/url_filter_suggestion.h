// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_URL_FILTER_SUGGESTION_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_URL_FILTER_SUGGESTION_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "url/gurl.h"

namespace multistep_filter {

// Represents the UI label for a filter attribute.
struct FilterAttributeUiLabel {
  FilterAttributeUiLabel(FilterSuggestionCandidateAttribute candidate_attribute,
                         FilterAttribute annotation_attribute);

  FilterAttributeUiLabel(const FilterAttributeUiLabel&) = default;
  FilterAttributeUiLabel(FilterAttributeUiLabel&&) = default;
  FilterAttributeUiLabel& operator=(const FilterAttributeUiLabel&) = default;
  FilterAttributeUiLabel& operator=(FilterAttributeUiLabel&&) = default;

  ~FilterAttributeUiLabel() = default;

  std::string ToString() const;

  friend bool operator==(const FilterAttributeUiLabel&,
                         const FilterAttributeUiLabel&) = default;

  // The UI label of the filter attribute.
  std::u16string attribute_label;
  // The value of the filter attribute.
  std::u16string attribute_value;
};

// A struct to hold the data for a URL based filter suggestion.
struct UrlFilterSuggestion {
  UrlFilterSuggestion(GURL navigation_url,
                      std::u16string source_domain,
                      base::Time extraction_timestamp,
                      std::vector<FilterAttributeUiLabel> attribute_ui_labels,
                      int64_t triggering_navigation_id,
                      std::string triggering_domain);

  UrlFilterSuggestion(const UrlFilterSuggestion&);
  UrlFilterSuggestion(UrlFilterSuggestion&&);
  UrlFilterSuggestion& operator=(const UrlFilterSuggestion&);
  UrlFilterSuggestion& operator=(UrlFilterSuggestion&&);

  ~UrlFilterSuggestion();

  std::string ToString() const;

  friend bool operator==(const UrlFilterSuggestion&,
                         const UrlFilterSuggestion&) = default;

  // The URL to navigate to when the suggestion is applied.
  GURL navigation_url;
  // The eTLD+1 domain of the URL from which this suggestion was generated.
  std::u16string source_domain;
  // The timestamp when the annotation was created which was used to generate
  // the suggestion.
  base::Time extraction_timestamp;
  // List of filter attribute UI labels and values for the suggestion. The
  // order follows that of the filter suggestion candidate's attributes.
  std::vector<FilterAttributeUiLabel> attribute_ui_labels;
  // The ID of the navigation that triggered this suggestion.
  int64_t triggering_navigation_id;
  // The eTLD+1 domain of the original triggering navigation. Used only for
  // logging.
  std::string triggering_domain;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_URL_FILTER_SUGGESTION_H_
