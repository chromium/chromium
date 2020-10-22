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
PatternProvider* g_pattern_provider = nullptr;
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
    EnrichPatternsWithEnVersion();
  }
}

const std::vector<MatchingPattern> PatternProvider::GetMatchPatterns(
    const std::string& pattern_name,
    const std::string& page_language) {
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
    return GetAllPatternsBaseOnType(pattern_name);
  } else if (
      base::FeatureList::IsEnabled(
          features::
              kAutofillApplyNegativePatternsForFieldTypeDetectionHeuristics)) {
    return GetAllPatternsBaseOnType(pattern_name);
  } else {
    return {};
  }
}

const std::vector<MatchingPattern> PatternProvider::GetMatchPatterns(
    ServerFieldType type,
    const std::string& page_language) {
  std::string pattern_name = AutofillType(type).ToString();
  return GetMatchPatterns(pattern_name, page_language);
}

// static.
PatternProvider& PatternProvider::GetInstance() {
  if (!g_pattern_provider) {
    static base::NoDestructor<PatternProvider> instance;
    g_pattern_provider = instance.get();
    field_type_parsing::PopulateFromResourceBundle();
  }
  return *g_pattern_provider;
}

// static.
void PatternProvider::SetPatternProviderForTesting(
    PatternProvider* pattern_provider) {
  DCHECK(pattern_provider);
  g_pattern_provider = pattern_provider;
}

// static.
void PatternProvider::ResetPatternProvider() {
  g_pattern_provider = nullptr;
}

void PatternProvider::EnrichPatternsWithEnVersion() {
  for (auto& p : patterns_) {
    std::map<std::string, std::vector<MatchingPattern>>& lg_to_patterns =
        p.second;

    auto it = lg_to_patterns.find("en");
    if (it == lg_to_patterns.end())
      continue;
    std::vector<MatchingPattern> en_patterns = it->second;

    for (MatchingPattern& en_pattern : en_patterns) {
      en_pattern.match_field_attributes = MATCH_NAME;
    }

    for (auto& q : lg_to_patterns) {
      const std::string& page_language = q.first;
      std::vector<MatchingPattern>& patterns = q.second;

      if (page_language != "en") {
        patterns.insert(patterns.end(), en_patterns.begin(), en_patterns.end());
      }
    }
  }
}

const std::vector<MatchingPattern> PatternProvider::GetAllPatternsBaseOnType(
    ServerFieldType type) {
  std::string type_str = AutofillType(type).ToString();
  return GetAllPatternsBaseOnType(type_str);
}

const std::vector<MatchingPattern> PatternProvider::GetAllPatternsBaseOnType(
    const std::string& type) {
  std::vector<MatchingPattern> match_patterns;

  for (const auto& inner_map : patterns_[type]) {
    match_patterns.insert(match_patterns.end(), inner_map.second.begin(),
                          inner_map.second.end());
  }

  return match_patterns;
}

}  //  namespace autofill
