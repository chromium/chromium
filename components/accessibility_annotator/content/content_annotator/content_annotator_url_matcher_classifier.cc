// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_url_matcher_classifier.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_classifier_rules_parser.h"
#include "url/gurl.h"

namespace accessibility_annotator {

// static
std::unique_ptr<ContentAnnotatorUrlMatcherClassifier>
ContentAnnotatorUrlMatcherClassifier::Create(std::string_view rules_json) {
  const base::flat_map<std::string, std::vector<std::string>> rules =
      ParseRulesFromJson(rules_json);
  if (rules.empty()) {
    return nullptr;
  }

  auto url_matcher = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::URLMatcherConditionSet::Vector condition_sets;
  std::vector<std::pair<base::MatcherStringPattern::ID, std::string>>
      id_to_category_pairs;
  base::MatcherStringPattern::ID id_counter = 0;
  for (const auto& [category, url_rules] : rules) {
    for (const auto& rule : url_rules) {
      url_matcher::URLMatcherConditionSet::Conditions conditions;
      conditions.insert(
          url_matcher->condition_factory()->CreateURLMatchesCondition(rule));
      scoped_refptr<url_matcher::URLMatcherConditionSet> condition_set =
          base::MakeRefCounted<url_matcher::URLMatcherConditionSet>(id_counter,
                                                                    conditions);
      condition_sets.push_back(std::move(condition_set));
      id_to_category_pairs.emplace_back(id_counter, category);
      id_counter++;
    }
  }
  url_matcher->AddConditionSets(condition_sets);

  return std::make_unique<ContentAnnotatorUrlMatcherClassifier>(
      PassKey(), std::move(url_matcher), std::move(id_to_category_pairs));
}

ContentAnnotatorUrlMatcherClassifier::ContentAnnotatorUrlMatcherClassifier(
    PassKey pass_key,
    std::unique_ptr<url_matcher::URLMatcher> url_matcher,
    base::flat_map<base::MatcherStringPattern::ID, std::string>
        id_to_category_map)
    : url_matcher_(std::move(url_matcher)),
      id_to_category_map_(std::move(id_to_category_map)) {}

ContentAnnotatorUrlMatcherClassifier::~ContentAnnotatorUrlMatcherClassifier() =
    default;
ContentAnnotatorUrlMatcherClassifier::ContentAnnotatorUrlMatcherClassifier(
    ContentAnnotatorUrlMatcherClassifier&&) = default;
ContentAnnotatorUrlMatcherClassifier&
ContentAnnotatorUrlMatcherClassifier::operator=(
    ContentAnnotatorUrlMatcherClassifier&&) = default;

std::optional<std::string_view> ContentAnnotatorUrlMatcherClassifier::Classify(
    const GURL& url) const {
  const std::set<base::MatcherStringPattern::ID> matching_ids =
      url_matcher_->MatchURL(url);
  if (matching_ids.empty()) {
    return std::nullopt;
  }

  // If there are multiple matches, we just return the category of the first
  // one.
  return id_to_category_map_.at(*matching_ids.begin());
}

}  // namespace accessibility_annotator
