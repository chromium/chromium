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
  }
}

const std::vector<MatchingPattern> PatternProvider::GetMatchPatterns(
    const std::string& pattern_name,
    const std::string& page_language) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1134496): Remove feature check once launched.
  if (base::FeatureList::IsEnabled(
          features::kAutofillUsePageLanguageToSelectFieldParsingPatterns)) {
    return patterns_[pattern_name][page_language];
  } else {
    return GetAllPatternsBaseOnType(pattern_name);
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
