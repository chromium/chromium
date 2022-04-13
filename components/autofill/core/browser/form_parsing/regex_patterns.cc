// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/regex_patterns.h"

#include "base/check.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns_inl.h"

namespace autofill {

base::span<const MatchPatternRef> GetMatchPatterns(
    base::StringPiece name,
    base::StringPiece language_code) {
  auto* it = kPatternMap.find(std::make_pair(name, language_code));
  if (!language_code.empty() && it == kPatternMap.end())
    it = kPatternMap.find(std::make_pair(name, ""));
  CHECK(it != kPatternMap.end());
  return it->second;
}

base::span<const MatchPatternRef> GetMatchPatterns(
    base::StringPiece name,
    absl::optional<LanguageCode> language_code) {
  return language_code ? GetMatchPatterns(name, **language_code)
                       : GetMatchPatterns(name, "");
}

base::span<const MatchPatternRef> GetMatchPatterns(
    ServerFieldType type,
    absl::optional<LanguageCode> language_code) {
  return GetMatchPatterns(FieldTypeToStringPiece(type), language_code);
}

// The dereferencing operator implements the distinction between ordinary and
// supplementary patterns (see the header). This is why it must return a copy,
// rather than a const reference.
MatchingPattern MatchPatternRef::operator*() const {
  const MatchingPattern& p = kPatterns[index()];
  return {
      .positive_pattern = p.positive_pattern,
      .negative_pattern = p.negative_pattern,
      .positive_score = p.positive_score,
      .match_field_attributes =
          is_supplementary() ? DenseSet<MatchAttribute>{MatchAttribute::kName}
                             : p.match_field_attributes,
      .match_field_input_types = p.match_field_input_types,
  };
}

}  // namespace autofill
