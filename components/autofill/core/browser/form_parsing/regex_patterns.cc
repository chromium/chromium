// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/regex_patterns.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns_inl.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

// Returns the span of MatchPatternRefs for the given pattern name, language
// code, and pattern file.
//
// Hits a CHECK if the given pattern file contains no patterns for the given
// name.
//
// Falls back to the union of all patterns of a the given name in the given
// pattern file if there are no patterns for the given language.
base::span<const MatchPatternRef> GetMatchPatterns(
    std::string_view name,
    std::string_view language_code,
    PatternFile pattern_file) {
  auto it = kPatternMap.find(std::make_pair(name, language_code));
  if (!language_code.empty() && it == kPatternMap.end())
    it = kPatternMap.find(std::make_pair(name, ""));
  CHECK(it != kPatternMap.end());
  switch (pattern_file) {
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    case PatternFile::kLegacy:
      return it->second[0];
#else
    case PatternFile::kDefault:
      return it->second[0];
    case PatternFile::kPredictionImprovements:
      return it->second[1];
#endif
  }
  NOTREACHED();
}

}  // namespace

std::optional<PatternFile> GetActivePatternFile() {
  return HeuristicSourceToPatternFile(GetActiveHeuristicSource());
}

base::span<const MatchPatternRef> GetMatchPatterns(
    std::string_view name,
    std::optional<LanguageCode> language_code,
    PatternFile pattern_file) {
  return language_code ? GetMatchPatterns(name, **language_code, pattern_file)
                       : GetMatchPatterns(name, "", pattern_file);
}

base::span<const MatchPatternRef> GetMatchPatterns(
    FieldType type,
    std::optional<LanguageCode> language_code,
    PatternFile pattern_file) {
  return GetMatchPatterns(FieldTypeToStringView(type), language_code,
                          pattern_file);
}

bool IsSupportedLanguageCode(LanguageCode language_code) {
  return kLanguages.contains(*language_code);
}

// The dereferencing operator implements the distinction between ordinary and
// supplementary patterns (see the header). This is why it must return a copy,
// rather than a const reference.
MatchingPattern MatchPatternRef::operator*() const {
  const MatchingPattern& p = kPatterns[index()];
  return {
      .positive_pattern = p.positive_pattern,
      .negative_pattern = p.negative_pattern,
      .match_field_attributes = is_supplementary()
                                    ? DenseSet({MatchAttribute::kName})
                                    : p.match_field_attributes,
      .form_control_types = p.form_control_types,
      .feature = p.feature,
  };
}

}  // namespace autofill
