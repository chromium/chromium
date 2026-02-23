// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_classifier_rules_parser.h"

#include <optional>
#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"

namespace accessibility_annotator {

namespace {
std::optional<ContentClassifierRelevance> IntToContentClassifierRelevance(
    int relevance_int) {
  return ((relevance_int < 0) ||
          (relevance_int >
           static_cast<int>(ContentClassifierRelevance::kMaxValue)))
             ? std::nullopt
             : std::make_optional(
                   static_cast<ContentClassifierRelevance>(relevance_int));
}
}  // namespace

base::flat_map<std::string, std::vector<std::string>> ParseRulesFromJson(
    std::string_view rules_json) {
  std::optional<base::Value> value =
      base::JSONReader::Read(rules_json, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value || !value->is_dict()) {
    return {};
  }

  const base::DictValue& rules_dict = value->GetDict();
  if (rules_dict.size() > kMaxRuleCategories) {
    return {};
  }

  std::vector<std::pair<std::string, std::vector<std::string>>> rule_pairs;
  rule_pairs.reserve(rules_dict.size());
  for (const auto [category, rule_values] : rules_dict) {
    if (!rule_values.is_list()) {
      // TODO(crbug.com/483205791): Add UMA logging for classifier errors.
      return {};
    }
    const base::ListValue& rule_list = rule_values.GetList();
    if (rule_list.size() > kMaxRulesPerCategory || rule_list.empty()) {
      return {};
    }

    std::vector<std::string> current_rules;
    current_rules.reserve(rule_list.size());
    for (const auto& rule_value : rule_list) {
      if (!rule_value.is_string()) {
        return {};
      }
      const std::string& rule = rule_value.GetString();
      if (rule.empty()) {
        return {};
      }
      current_rules.push_back(rule);
    }
    rule_pairs.emplace_back(category, std::move(current_rules));
  }

  return std::move(rule_pairs);
}

base::flat_map<std::string, ContentClassifierRelevance>
ParseRelevanceValuesFromJson(std::string_view relevance_values_json) {
  std::optional<base::Value> value = base::JSONReader::Read(
      relevance_values_json, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value || !value->is_dict()) {
    return {};
  }

  const base::DictValue& dict = value->GetDict();
  if (dict.size() > kMaxRuleCategories) {
    return {};
  }

  std::vector<std::pair<std::string, ContentClassifierRelevance>>
      relevance_pairs;
  for (const auto item : dict) {
    if (!item.second.is_int()) {
      return {};
    }
    int relevance_int = item.second.GetInt();
    std::optional<ContentClassifierRelevance> relevance =
        IntToContentClassifierRelevance(relevance_int);
    if (relevance.value_or(ContentClassifierRelevance::kUnknown) ==
        ContentClassifierRelevance::kUnknown) {
      return {};
    }
    relevance_pairs.emplace_back(item.first, relevance.value());
  }
  return std::move(relevance_pairs);
}

}  // namespace accessibility_annotator
