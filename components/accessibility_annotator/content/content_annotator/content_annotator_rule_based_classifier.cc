// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_rule_based_classifier.h"

#include <utility>

#include "components/accessibility_annotator/content/content_annotator/content_annotator_classifier_rules_parser.h"
#include "third_party/re2/src/re2/re2.h"

namespace accessibility_annotator {

// static
std::unique_ptr<ContentAnnotatorRuleBasedClassifier>
ContentAnnotatorRuleBasedClassifier::Create(std::string_view rules_json) {
  const base::flat_map<std::string, std::vector<std::string>> rules =
      ParseRulesFromJson(rules_json);
  if (rules.empty()) {
    return nullptr;
  }
  std::vector<std::pair<std::string, std::vector<std::unique_ptr<re2::RE2>>>>
      category_to_regexes_pairs;
  category_to_regexes_pairs.reserve(rules.size());
  for (const auto& [category, regex_rules] : rules) {
    std::vector<std::unique_ptr<re2::RE2>> compiled_regex_rules;
    compiled_regex_rules.reserve(regex_rules.size());
    for (const auto& rule : regex_rules) {
      re2::RE2::Options options;
      options.set_case_sensitive(false);
      auto regex = std::make_unique<re2::RE2>(rule, options);
      if (!regex->ok()) {
        // TODO(crbug.com/483205791): Add UMA logging for classifier errors.
        return nullptr;
      }
      compiled_regex_rules.push_back(std::move(regex));
    }
    category_to_regexes_pairs.emplace_back(category,
                                           std::move(compiled_regex_rules));
  }
  return std::make_unique<ContentAnnotatorRuleBasedClassifier>(
      PassKey(), std::move(category_to_regexes_pairs));
}

ContentAnnotatorRuleBasedClassifier::ContentAnnotatorRuleBasedClassifier(
    PassKey pass_key,
    CategoryToRegexesMap category_to_regexes_map)
    : category_to_regexes_map_(std::move(category_to_regexes_map)) {}

ContentAnnotatorRuleBasedClassifier::~ContentAnnotatorRuleBasedClassifier() =
    default;
ContentAnnotatorRuleBasedClassifier::ContentAnnotatorRuleBasedClassifier(
    ContentAnnotatorRuleBasedClassifier&&) = default;
ContentAnnotatorRuleBasedClassifier&
ContentAnnotatorRuleBasedClassifier::operator=(
    ContentAnnotatorRuleBasedClassifier&&) = default;

std::optional<std::string_view> ContentAnnotatorRuleBasedClassifier::Classify(
    std::string_view text) const {
  for (const auto& [category, regexes] : category_to_regexes_map_) {
    for (const auto& regex : regexes) {
      if (re2::RE2::PartialMatch(text, *regex)) {
        return category;
      }
    }
  }
  return std::nullopt;
}
}  // namespace accessibility_annotator
