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
#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {
const char* kSourceCodeLanguage = "en";
}

PatternProvider* PatternProvider::g_pattern_provider = nullptr;

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
    EnrichPatternsWithEnVersion();
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

// static
PatternProvider& PatternProvider::GetInstance() {
  if (!g_pattern_provider) {
    static base::NoDestructor<PatternProvider> instance;
    g_pattern_provider = instance.get();
    field_type_parsing::PopulateFromResourceBundle();
  }
  return *g_pattern_provider;
}

// static
void PatternProvider::ResetPatternProvider() {
  g_pattern_provider = nullptr;
}

void PatternProvider::EnrichPatternsWithEnVersion() {
  for (auto& p : patterns_) {
    std::map<std::string, std::vector<MatchingPattern>>& lg_to_patterns =
        p.second;

    auto it = lg_to_patterns.find(kSourceCodeLanguage);
    if (it == lg_to_patterns.end())
      continue;
    std::vector<MatchingPattern> en_patterns = it->second;
    for (MatchingPattern& en_pattern : en_patterns) {
      en_pattern.match_field_attributes = MATCH_NAME;
    }

    for (auto& q : lg_to_patterns) {
      const std::string& page_language = q.first;
      std::vector<MatchingPattern>& patterns = q.second;

      if (page_language != kSourceCodeLanguage) {
        patterns.insert(patterns.end(), en_patterns.begin(), en_patterns.end());
      }
    }
  }
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

  size_t en_size = [&type_patterns]() -> size_t {
    auto jt = type_patterns.find(kSourceCodeLanguage);
    return jt != type_patterns.end() ? jt->second.size() : 0;
  }();

  std::vector<MatchingPattern> all_language_patterns;
  for (const auto& p : type_patterns) {
    const std::string& page_language = p.first;
    const std::vector<MatchingPattern>& language_patterns = p.second;
    DCHECK(language_patterns.size() >= en_size);
    auto end = language_patterns.end() -
               (page_language == kSourceCodeLanguage ? 0 : en_size);
    all_language_patterns.insert(all_language_patterns.end(),
                                 language_patterns.begin(), end);
  }
  return all_language_patterns;
}

}  //  namespace autofill
