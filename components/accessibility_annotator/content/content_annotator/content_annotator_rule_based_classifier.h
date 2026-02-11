// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_RULE_BASED_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_RULE_BASED_CLASSIFIER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/types/pass_key.h"
#include "third_party/re2/src/re2/re2.h"

namespace accessibility_annotator {

class ContentAnnotatorRuleBasedClassifier {
 public:
  using PassKey = base::PassKey<ContentAnnotatorRuleBasedClassifier>;

  // Creates a rule based classifier from a string of rules.
  // Matching is done using by comparing the text to the regex rules.
  // Returns nullptr if the rules are invalid.
  static std::unique_ptr<ContentAnnotatorRuleBasedClassifier> Create(
      std::string_view rules_json);

  using CategoryToRegexesMap =
      base::flat_map<std::string, std::vector<std::unique_ptr<re2::RE2>>>;

  explicit ContentAnnotatorRuleBasedClassifier(
      PassKey pass_key,
      CategoryToRegexesMap category_to_regexes_map);
  ~ContentAnnotatorRuleBasedClassifier();
  ContentAnnotatorRuleBasedClassifier(
      const ContentAnnotatorRuleBasedClassifier&) = delete;
  ContentAnnotatorRuleBasedClassifier& operator=(
      const ContentAnnotatorRuleBasedClassifier&) = delete;
  ContentAnnotatorRuleBasedClassifier(ContentAnnotatorRuleBasedClassifier&&);
  ContentAnnotatorRuleBasedClassifier& operator=(
      ContentAnnotatorRuleBasedClassifier&&);

  // Returns the category of the text if it matches any of the rules, otherwise
  // returns std::nullopt.
  // If multiple categories match, returns the first one.
  // TODO(crbug.com/482477208): Add support for multiple categories.
  std::optional<std::string_view> Classify(std::string_view text) const;

 private:
  // A map of category to a vector of compiled regex rules.
  CategoryToRegexesMap category_to_regexes_map_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_RULE_BASED_CLASSIFIER_H_
