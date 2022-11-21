// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/acronym_matcher.h"

#include <cstddef>
#include <string>

#include "base/i18n/case_conversion.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace ash::string_matching {

namespace {

using acronym_matcher_constants::kIsFrontOfTokenCharScore;
using acronym_matcher_constants::kIsPrefixCharScore;
using acronym_matcher_constants::kNoMatchScore;

}  // namespace

// AcronymMatcher:
//
// The factors below are applied when the current char of query matches
// the current char of the text to be matched. Different factors are chosen
// based on where the match happens:
//
// 1) `kIsPrefixCharScore` is used when we match the first char of the query to
// the first char of the text
//
// 2) `kIsFrontOfTokenCharScore` will be used if the first char of the token
// matches the current char of the query in all other cases.
//
// Examples:
//
//   For text: 'Google Chrome'.
//
//   Query 'gc' would use kIsPrefixCharScore for 'g' and
//       kIsFrontOfTokenCharScore for 'c'.

// kNoMatchScore is a relevance score that represents no match.

AcronymMatcher::AcronymMatcher(const TokenizedString& query,
                               const TokenizedString& text) {
  query_ = base::i18n::ToLower(query.text());
  for (auto token : text.tokens()) {
    text_acronym_.push_back(token[0]);
  }
  text_mapping_ = text.mappings();
}
AcronymMatcher::~AcronymMatcher() = default;

double AcronymMatcher::CalculateRelevance() {
  auto found_index = text_acronym_.find(query_);
  if (found_index == std::u16string::npos) {
    return kNoMatchScore;
  }

  double relevance = kIsFrontOfTokenCharScore * query_.size();
  if (found_index == 0) {
    relevance += kIsPrefixCharScore - kIsFrontOfTokenCharScore;
  }

  // update the |hits_| if there is a match.
  for (size_t i = 0; i < query_.size(); i++) {
    size_t start_pos = text_mapping_[found_index + i].start();
    hits_.emplace_back(start_pos, start_pos + 1);
  }

  return relevance;
}

}  // namespace ash::string_matching
