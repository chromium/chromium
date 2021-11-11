// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_STRING_MATCHING_TOKENIZED_STRING_MATCH_H_
#define CHROMEOS_COMPONENTS_STRING_MATCHING_TOKENIZED_STRING_MATCH_H_

#include <string>
#include <vector>

#include "ui/gfx/range/range.h"

namespace chromeos {
namespace string_matching {

class TokenizedString;

// TokenizedStringMatch takes two tokenized strings: one as the text and
// the other one as the query. It matches the query against the text,
// calculates a relevance score between [0, 1] and marks the matched portions
// of text. A relevance of zero means the two are completely different to each
// other. The higher the relevance score, the better the two strings are
// matched. Matched portions of text are stored as index ranges.
class TokenizedStringMatch {
 public:
  typedef std::vector<gfx::Range> Hits;

  TokenizedStringMatch();

  TokenizedStringMatch(const TokenizedStringMatch&) = delete;
  TokenizedStringMatch& operator=(const TokenizedStringMatch&) = delete;

  ~TokenizedStringMatch();

  // Calculates the relevance and hits. Returns true if the two strings are
  // somewhat matched, i.e. relevance score is not zero.
  bool Calculate(const TokenizedString& query, const TokenizedString& text);

  // Convenience wrapper to calculate match from raw string input.
  bool Calculate(const std::u16string& query, const std::u16string& text);

  double relevance() const { return relevance_; }
  const Hits& hits() const { return hits_; }

 private:
  // Score in range of [0,1] representing how well the query matches the text.
  double relevance_;

  // Char index ranges in |text| of where matches are found.
  Hits hits_;
};

}  // namespace string_matching
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_STRING_MATCHING_TOKENIZED_STRING_MATCH_H_
