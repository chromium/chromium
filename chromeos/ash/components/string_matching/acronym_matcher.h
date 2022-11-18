// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_ACRONYM_MATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_ACRONYM_MATCHER_H_

#include <string>

#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "ui/gfx/range/range.h"

namespace ash::string_matching {

// These are described in more detail in the .cc file.
// The namespace is to distinguish from the constants in prefix matcher.
namespace acronym_matcher_constants {

constexpr double kIsPrefixCharScore = 1.0;
constexpr double kIsFrontOfTokenCharScore = 0.8;

constexpr double kNoMatchScore = 0.0;

}  // namespace acronym_matcher_constants

// AcronymMatcher matches the chars of a given query as acronym of tokens in
// a given text. i.e. To capture the information that:
// With the text "axx bxx cxx", queries "a", "ab", "abc", "b", "bc" and "c" are
// all considered as acronym matching to the text, while queries "abcd", "ab c",
// and "abdc" are not.
class AcronymMatcher {
 public:
  typedef std::vector<gfx::Range> Hits;

  AcronymMatcher(const TokenizedString& query, const TokenizedString& text);
  ~AcronymMatcher();

  AcronymMatcher(const AcronymMatcher&) = delete;
  AcronymMatcher& operator=(const AcronymMatcher&) = delete;

  // Perform acronym match. Stops on the first full match
  // and returns true. Otherwise, returns false to indicate no match.
  double CalculateRelevance();

  // Return the hits match of the AcronymMatcher. E.g.,
  // For query: "coc" and text: "Crash Of Crowns".
  // The hit would be {{0,1}, {6,7}, {9,10}} as the query is matched at position
  // 0, 6 and 9, i.e., "[C]rash [O]f [C]rowns".
  //
  // N.B. This function is not expected to be called before the function
  // `CalculateRelevance()`.
  const Hits& hits() const { return hits_; }

 private:
  std::u16string query_;
  std::u16string text_acronym_;
  Hits text_mapping_;

  Hits hits_;
};

}  // namespace ash::string_matching

#endif  // CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_ACRONYM_MATCHER_H_
