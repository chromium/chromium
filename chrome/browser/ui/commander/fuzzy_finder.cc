// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/fuzzy_finder.h"

#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/strings/string_util.h"

namespace {
// Used only for exact matches.
static const double kMaxScore = 1.0;
// When needle is a prefix of haystack.
static const double kPrefixScore = .99;
// When a heuristic determines that the match should score highly,
// but it is *not* an exact match or prefix.
static const double kVeryHighScore = .95;

struct MatchRecord {
  MatchRecord(int start, int end, bool is_boundary, int gap_before)
      : range(start, end), gap_before(gap_before), is_boundary(is_boundary) {}
  gfx::Range range;
  int gap_before;
  bool is_boundary;
};

// Scores matches identified by ConsecutiveMatchWithGaps(). See that comment
// for details.
double ScoreForMatches(const std::vector<MatchRecord>& matches,
                       size_t needle_size,
                       size_t haystack_size) {
  // |base_score| is the maximum per match, so total should not exceed 1.0.
  const double base_score = 1.0 / needle_size;
  const double gap_penalty = 1.0 / haystack_size;
  static const double kRegularMultiplier = .5;
  static const double kWordBoundaryMultiplier = .8;
  static const double kInitialMultiplier = 1.0;
  double score = 0;

  for (size_t i = 0; i < matches.size(); i++) {
    MatchRecord match = matches[i];
    // The first character of the match is special; it gets a relative bonus
    // if it is on a boundary. Otherwise, it is penalized by the distance
    // between it and the previous match.
    if (match.is_boundary) {
      score +=
          base_score * (i == 0 ? kInitialMultiplier : kWordBoundaryMultiplier);
    } else {
      double penalty_multiplier = 1 - (gap_penalty * match.gap_before);
      DCHECK_GT(penalty_multiplier, 0);
      score += base_score * kRegularMultiplier * penalty_multiplier;
    }
    // ...then the rest of a contiguous match.
    score += (match.range.length() - 1) * base_score * kRegularMultiplier;
  }
  DCHECK(score <= 1.0);
  return score;
}

// Returns a positive score if every code point in |needle| is present in
// |haystack| in the same order. The match *need not* be contiguous. Matches in
// special positions are given extra weight, and noncontiguous matches are
// penalized based on the size of the gaps between.
// This is not guaranteed to return the best possible match; for example, given
// needle = "orange" and haystack = "William of Orange", this function will
// match as "William [o]f O[range]" rather than "William of [Orange]". It's main
// use is to filter nonmatches before a more comprehensive algorithm, and as a
// fallback for when the inputs are too high for a more comprehensive algorithm
// to be performant.
double ConsecutiveMatchWithGaps(const base::string16& needle,
                                const base::string16& haystack,
                                std::vector<gfx::Range>* matched_ranges) {
  DCHECK(needle == base::i18n::FoldCase(needle));
  DCHECK(haystack == base::i18n::FoldCase(haystack));
  DCHECK(matched_ranges->empty());
  // Special case for prefix.
  if (base::StartsWith(haystack, needle)) {
    matched_ranges->emplace_back(0, needle.size());
    return kPrefixScore;
  }
  base::i18n::UTF16CharIterator n_iter(&needle);
  base::i18n::UTF16CharIterator h_iter(&haystack);

  std::vector<MatchRecord> matches;
  int gap_size_before_match = 0;
  int match_began_on_boundary = true;
  bool in_match = false;
  int match_start = -1;

  // Find matching ranges.
  while (!n_iter.end() && !h_iter.end()) {
    if (n_iter.get() == h_iter.get()) {
      // There's a match.
      if (!in_match) {
        // Match start.
        in_match = true;
        match_start = h_iter.array_pos();
        match_began_on_boundary =
            h_iter.start() ||
            base::IsUnicodeWhitespace(h_iter.PreviousCodePoint());
      }
      h_iter.Advance();
      n_iter.Advance();
    } else {
      if (in_match) {
        DCHECK(match_start != -1);
        in_match = false;
        matches.emplace_back(match_start, h_iter.array_pos(),
                             match_began_on_boundary, gap_size_before_match);
        gap_size_before_match = 1;
        match_start = -1;
      } else {
        gap_size_before_match++;
      }
      h_iter.Advance();
    }
  }
  if (!n_iter.end()) {
    // Didn't match all of |needle|.
    matched_ranges->clear();
    return 0;
  }
  if (in_match) {
    DCHECK(match_start != -1);
    matches.emplace_back(match_start, h_iter.array_pos(),
                         match_began_on_boundary, gap_size_before_match);
  }
  for (const MatchRecord& match : matches) {
    matched_ranges->push_back(match.range);
  }
  double score = ScoreForMatches(matches, needle.size(), haystack.size());
  score *= kPrefixScore;  // Normalize so that a prefix always wins.
  return score;
}

}  // namespace

namespace commander {

double FuzzyFind(const base::string16& needle,
                 const base::string16& haystack,
                 std::vector<gfx::Range>* matched_ranges) {
  DCHECK(needle == base::i18n::FoldCase(needle));
  matched_ranges->clear();
  const base::string16& folded = base::i18n::FoldCase(haystack);
  size_t m = needle.size();
  size_t n = folded.size();
  // Special case 0: M > N. We don't allow skipping anything in |needle|, so
  // no match possible.
  if (m > n) {
    return 0;
  }
  // Special case 1: M == N. It must be either an exact match,
  // or a non-match.
  if (m == n) {
    if (folded == needle) {
      matched_ranges->emplace_back(0, needle.length());
      return kMaxScore;
    } else {
      return 0;
    }
  }
  // Special case 2: M == 1. Scan through all matches, and return:
  //    no match ->
  //      0
  //    prefix match ->
  //      kPrefixScore
  //    word boundary match (e.g. needle: j, haystack "Orange [J]uice") ->
  //      kVeryHighScore
  //    any other match ->
  //      Scored based on how far into haystack needle is found, normalized by
  //      haystack length.
  if (m == 1) {
    size_t substring_position = folded.find(needle);
    while (substring_position != std::string::npos) {
      if (substring_position == 0) {
        // Prefix match.
        matched_ranges->emplace_back(0, 1);
        return kPrefixScore;
      } else {
        wchar_t previous = folded.at(substring_position - 1);
        if (base::IsUnicodeWhitespace(previous)) {
          // Word boundary. Since we've eliminated prefix by now, this is as
          // good as we're going to get, so we can return.
          matched_ranges->clear();
          matched_ranges->emplace_back(substring_position,
                                       substring_position + 1);
          return kVeryHighScore;
          // Internal match. If |matched_ranges| is already populated, we've
          // seen another internal match previously, so ignore this one.
        } else if (matched_ranges->empty()) {
          matched_ranges->emplace_back(substring_position,
                                       substring_position + 1);
        }
      }
      substring_position = folded.find(needle, substring_position + 1);
    }
    if (matched_ranges->empty()) {
      return 0;
    } else {
      // First internal match.
      DCHECK_EQ(matched_ranges->size(), 1u);
      double position = static_cast<double>(matched_ranges->back().start());
      return std::min(1 - position / folded.length(), 0.01);
    }
  }
  // This has two purposes:
  // 1. If there's no match here, we should bail instead of wasting time on the
  //    full O(mn) matching algorithm.
  // 2. If m * n is too big, we will use this result instead of doing the full
  //    full O(mn) matching algorithm.
  // ***TEMPORARY***: The full algorithm isn't implemented yet, so we will use
  // this unconditionally for now.
  return ConsecutiveMatchWithGaps(needle, folded, matched_ranges);
}

}  // namespace commander
