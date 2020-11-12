// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {
const char* kSourceCodeLanguage = "en";

// Adds the English patterns, restricted to MatchFieldType MATCH_NAME, to
// every other language.
void EnrichPatternsWithEnVersion(
    PatternProvider::Map* type_and_lang_to_patterns) {
  DCHECK(type_and_lang_to_patterns);
  for (auto& p : *type_and_lang_to_patterns) {
    std::map<std::string, std::vector<MatchingPattern>>& lang_to_patterns =
        p.second;

    auto it = lang_to_patterns.find(kSourceCodeLanguage);
    if (it == lang_to_patterns.end())
      continue;
    std::vector<MatchingPattern> en_patterns = it->second;
    for (MatchingPattern& en_pattern : en_patterns) {
      en_pattern.match_field_attributes = MATCH_NAME;
    }

    for (auto& q : lang_to_patterns) {
      const std::string& page_language = q.first;
      std::vector<MatchingPattern>& patterns = q.second;

      if (page_language != kSourceCodeLanguage) {
        patterns.insert(patterns.end(), en_patterns.begin(), en_patterns.end());
      }
    }
  }
}

// Sorts patterns in descending order by their score.
void SortPatternsByScore(PatternProvider::Map* type_and_lang_to_patterns) {
  for (auto& p : *type_and_lang_to_patterns) {
    std::map<std::string, std::vector<MatchingPattern>>& lang_to_patterns =
        p.second;
    for (auto& q : lang_to_patterns) {
      std::vector<MatchingPattern>& patterns = q.second;
      std::sort(patterns.begin(), patterns.end(),
                [](const MatchingPattern& mp1, const MatchingPattern& mp2) {
                  return mp1.positive_score > mp2.positive_score;
                });
    }
  }
}
}

PatternProvider* PatternProvider::g_pattern_provider = nullptr;

// static
PatternProvider& PatternProvider::GetInstance() {
  if (!g_pattern_provider) {
    static base::NoDestructor<PatternProvider> instance;
    g_pattern_provider = instance.get();
    // TODO(crbug/1147608) This is an ugly hack to avoid loading the JSON. The
    // motivation is that some Android unit tests fail because a dependency is
    // missing. Instead of fixing this dependency, we'll go for an alternative
    // solution that avoids the whole async/sync problem.
    if (base::FeatureList::IsEnabled(
            features::kAutofillUsePageLanguageToSelectFieldParsingPatterns) ||
        base::FeatureList::IsEnabled(
            features::
                kAutofillApplyNegativePatternsForFieldTypeDetectionHeuristics)) {
      field_type_parsing::PopulateFromResourceBundle();
    }
  }
  return *g_pattern_provider;
}

// static
void PatternProvider::ResetPatternProvider() {
  g_pattern_provider = nullptr;
}

PatternProvider::PatternProvider() = default;
PatternProvider::~PatternProvider() = default;

void PatternProvider::SetPatterns(PatternProvider::Map patterns,
                                  const base::Version version,
                                  const bool overwrite_equal_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pattern_version_.IsValid() || pattern_version_ < version ||
      (overwrite_equal_version && pattern_version_ == version)) {
    patterns_ = patterns;
    pattern_version_ = version;
    EnrichPatternsWithEnVersion(&patterns_);
    SortPatternsByScore(&patterns_);
  }
}

const std::vector<MatchingPattern> PatternProvider::GetMatchPatterns(
    const std::string& pattern_name,
    const std::string& page_language) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1134496): Remove feature check once launched.
  if (base::FeatureList::IsEnabled(
          features::kAutofillUsePageLanguageToSelectFieldParsingPatterns)) {
    auto outer_it = patterns_.find(pattern_name);
    if (outer_it != patterns_.end()) {
      const std::map<std::string, std::vector<MatchingPattern>>&
          lang_to_pattern = outer_it->second;
      auto inner_it = lang_to_pattern.find(page_language);
      if (inner_it != lang_to_pattern.end()) {
        const std::vector<MatchingPattern>& patterns = inner_it->second;
        if (!patterns.empty()) {
          return patterns;
        }
      }
    }
    return GetAllPatternsByType(pattern_name);
  } else if (
      base::FeatureList::IsEnabled(
          features::
              kAutofillApplyNegativePatternsForFieldTypeDetectionHeuristics)) {
    return GetAllPatternsByType(pattern_name);
  } else {
    return {};
  }
}

const std::vector<MatchingPattern> PatternProvider::GetMatchPatterns(
    ServerFieldType type,
    const std::string& page_language) const {
  std::string pattern_name = AutofillType(type).ToString();
  return GetMatchPatterns(pattern_name, page_language);
}

const std::vector<MatchingPattern> PatternProvider::GetAllPatternsByType(
    ServerFieldType type) const {
  std::string type_str = AutofillType(type).ToString();
  return GetAllPatternsByType(type_str);
}

const std::vector<MatchingPattern> PatternProvider::GetAllPatternsByType(
    const std::string& type) const {
  auto it = patterns_.find(type);
  if (it == patterns_.end())
    return {};
  const std::map<std::string, std::vector<MatchingPattern>>& type_patterns =
      it->second;

  std::vector<MatchingPattern> all_language_patterns;
  for (const auto& p : type_patterns) {
    const std::string& page_language = p.first;
    const std::vector<MatchingPattern>& language_patterns = p.second;
    for (const MatchingPattern& mp : language_patterns) {
      if (page_language == kSourceCodeLanguage ||
          mp.language != kSourceCodeLanguage) {
        all_language_patterns.push_back(mp);
      }
    }
  }
  return all_language_patterns;
}

}  //  namespace autofill
