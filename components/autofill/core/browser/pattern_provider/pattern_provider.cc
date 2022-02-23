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
#include "components/autofill/core/browser/pattern_provider/default_regex_patterns.h"
#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {
const char* kSourceCodeLanguage = "en";

// Adds the English patterns, restricted to MatchAttribute::kName, to
// every other language.
void EnrichPatternsWithEnVersion(
    PatternProvider::Map* type_and_lang_to_patterns) {
  DCHECK(type_and_lang_to_patterns);
  for (auto& p : *type_and_lang_to_patterns) {
    std::map<LanguageCode, std::vector<MatchingPattern>>& lang_to_patterns =
        p.second;

    auto it = lang_to_patterns.find(LanguageCode(kSourceCodeLanguage));
    if (it == lang_to_patterns.end())
      continue;
    std::vector<MatchingPattern> en_patterns = it->second;
    for (MatchingPattern& en_pattern : en_patterns) {
      en_pattern.match_field_attributes = {MatchAttribute::kName};
    }

    for (auto& q : lang_to_patterns) {
      const LanguageCode& page_language = q.first;
      std::vector<MatchingPattern>& patterns = q.second;

      if (page_language != LanguageCode(kSourceCodeLanguage)) {
        patterns.insert(patterns.end(), en_patterns.begin(), en_patterns.end());
      }
    }
  }
}

// Sorts patterns in descending order by their score.
void SortPatternsByScore(PatternProvider::Map* type_and_lang_to_patterns) {
  for (auto& p : *type_and_lang_to_patterns) {
    std::map<LanguageCode, std::vector<MatchingPattern>>& lang_to_patterns =
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

// static
PatternProvider& PatternProvider::GetInstance() {
  static base::NoDestructor<PatternProvider> instance;
  static bool initialized = false;
  if (!initialized) {
    instance->SetPatterns(CreateDefaultRegexPatterns(), base::Version());
    initialized = true;
  }
  return *instance;
}

PatternProvider::PatternProvider() = default;
PatternProvider::~PatternProvider() = default;

void PatternProvider::SetPatterns(PatternProvider::Map patterns,
                                  const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pattern_version_.IsValid() ||
      (version.IsValid() && pattern_version_ <= version)) {
    patterns_ = std::move(patterns);
    pattern_version_ = version;
    EnrichPatternsWithEnVersion(&patterns_);
    SortPatternsByScore(&patterns_);
  }
}

const std::vector<MatchingPattern> PatternProvider::GetMatchPatterns(
    const std::string& pattern_name,
    const LanguageCode& page_language) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1134496): Remove feature check once launched.
  if (features::kAutofillParsingWithLanguageSpecificPatternsParam.Get()) {
    auto outer_it = patterns_.find(pattern_name);
    if (outer_it != patterns_.end()) {
      const std::map<LanguageCode, std::vector<MatchingPattern>>&
          lang_to_pattern = outer_it->second;
      auto inner_it = lang_to_pattern.find(page_language);
      if (inner_it != lang_to_pattern.end()) {
        const std::vector<MatchingPattern>& patterns = inner_it->second;
        if (!patterns.empty()) {
          return patterns;
        }
      }
    }
  }
  return GetAllPatternsByType(pattern_name);
}

const std::vector<MatchingPattern> PatternProvider::GetMatchPatterns(
    ServerFieldType type,
    const LanguageCode& page_language) const {
  return GetMatchPatterns(AutofillType::ServerFieldTypeToString(type),
                          page_language);
}

const std::vector<MatchingPattern> PatternProvider::GetAllPatternsByType(
    ServerFieldType type) const {
  return GetAllPatternsByType(AutofillType::ServerFieldTypeToString(type));
}

const std::vector<MatchingPattern> PatternProvider::GetAllPatternsByType(
    const std::string& type) const {
  auto it = patterns_.find(type);
  if (it == patterns_.end())
    return {};
  const std::map<LanguageCode, std::vector<MatchingPattern>>& type_patterns =
      it->second;

  std::vector<MatchingPattern> all_language_patterns;
  for (const auto& p : type_patterns) {
    const LanguageCode& page_language = p.first;
    const std::vector<MatchingPattern>& language_patterns = p.second;
    for (const MatchingPattern& mp : language_patterns) {
      if (page_language == LanguageCode(kSourceCodeLanguage) ||
          mp.language != LanguageCode(kSourceCodeLanguage)) {
        all_language_patterns.push_back(mp);
      }
    }
  }
  return all_language_patterns;
}

}  //  namespace autofill
