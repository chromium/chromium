// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_CLASSIFIER_RULES_PARSER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_CLASSIFIER_RULES_PARSER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"

namespace accessibility_annotator {

inline constexpr size_t kMaxRuleCategories = 100;
inline constexpr size_t kMaxRulesPerCategory = 200;

// LINT.IfChange(ContentClassifierRelevance)
enum class ContentClassifierRelevance {
  kUnknown = 0,
  kLowContentRelevance = 1,
  kMediumContentRelevance = 2,
  kHighContentRelevance = 3,
  kMaxValue = kHighContentRelevance,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:ContentClassifierRelevanceValue)

// Parses a JSON string of rules into a map of categories to rules.
// The string should be in the format:
// '{"category1":["rule1","rule2"],"category2":["ruleA","ruleB"]}'
base::flat_map<std::string, std::vector<std::string>> ParseRulesFromJson(
    std::string_view rules_json);

// Parses a JSON string into a map of category to relevance value.
// The string should be in the format:
// '{"category1":1,"category2":2}'
base::flat_map<std::string, ContentClassifierRelevance>
ParseRelevanceValuesFromJson(std::string_view relevance_values_json);

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_CLASSIFIER_RULES_PARSER_H_
