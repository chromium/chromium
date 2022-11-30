// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/fuzzy_finder.h"

#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/ustring.h"

namespace {
// Used only for exact matches.
static const double kMaxScore = 1.0;
// When needle is a prefix of haystack.
static const double kPrefixScore = .99;
// When a heuristic determines that the match should score highly,
// but it is *not* an exact match or prefix.
static const double kVeryHighScore = .95;

// Max haystack size in UTF-16 units for the dynamic programming algorithm.
// Haystacks longer than this are scored by ConsecutiveMatchWithGaps.
static constexpr size_t kMaxHaystack = 1024;
// Max needle size in UTF-16 units for the dynamic programming algorithm.
// Needles longer than this are scored by ConsecutiveMatchWithGaps
static constexpr size_t kMaxNeedle = 16;

struct MatchRecord {
  MatchRecord(int start, int end, int length, bool is_boundary, int gap_before)
      : range(start, end),
        length(length),
        gap_before(gap_before),
        is_boundary(is_boundary) {}
  gfx::Range range;
  // This can't be inferred from `range` since range is in code units for
  // display, but `length` is in code points.
  int length;
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
    score += (match.length - 1) * base_score * kRegularMultiplier;
  }
  DCHECK(score <= 1.0);
  return score;
}

size_t LengthInCodePoints(const std::u16string& str) {
  return u_countChar32(str.data(), str.size());
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
double ConsecutiveMatchWithGaps(const std::u16string& needle,
                                const std::u16string& haystack,
                                std::vector<gfx::Range>* matched_ranges) {
  DCHECK(needle == base::i18n::FoldCase(needle));
  DCHECK(haystack == base::i18n::FoldCase(haystack));
  DCHECK(matched_ranges->empty());
  // Special case for prefix.
  if (base::StartsWith(haystack, needle)) {
    matched_ranges->emplace_back(0, needle.size());
    return kPrefixScore;
  }
  base::i18n::UTF16CharIterator n_iter(needle);
  base::i18n::UTF16CharIterator h_iter(haystack);

  std::vector<MatchRecord> matches;
  int gap_size_before_match = 0;
  int match_began_on_boundary = true;
  int match_start = -1;
  int match_length = 0;

  // Find matching ranges.
  while (!n_iter.end() && !h_iter.end()) {
    if (n_iter.get() == h_iter.get()) {
      // There's a match.
      if (match_length == 0) {
        // Match start.
        match_start = h_iter.array_pos();
        match_began_on_boundary =
            h_iter.start() || u_isUWhiteSpace(h_iter.PreviousCodePoint());
      }
      ++match_length;
      h_iter.Advance();
      n_iter.Advance();
    } else {
      if (match_length > 0) {
        DCHECK(match_start != -1);
        match_length = 0;
        matches.emplace_back(match_start, h_iter.array_pos(), match_length,
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
  if (match_length > 0) {
    DCHECK(match_start != -1);
    matches.emplace_back(match_start, h_iter.array_pos(), match_length,
                         match_began_on_boundary, gap_size_before_match);
  }
  for (const MatchRecord& match : matches) {
    matched_ranges->push_back(match.range);
  }
  double score = ScoreForMatches(matches, LengthInCodePoints(needle),
                                 LengthInCodePoints(haystack));
  score *= kPrefixScore;  // Normalize so that a prefix always wins.
  return score;
}
// Converts a list of indices in `positions` into contiguous ranges and fills
// `matched_ranges` with the result.
// For example: [0, 1, 4, 7, 8, 9] -> [{0, 2}, {4, 1}, {7, 3}].
void ConvertPositionsToRanges(const std::vector<size_t>& positions,
                              std::vector<gfx::Range>* matched_ranges) {
  size_t n = positions.size();
  DCHECK(n > 0);
  size_t start = positions.front();
  size_t length = 1;
  for (size_t i = 0; i < n - 1; ++i) {
    if (positions.at(i) + 1 < positions.at(i + 1)) {
      // Noncontiguous positions -> close out the range.
      matched_ranges->emplace_back(start, start + length);
      start = positions.at(i + 1);
      length = 1;
    } else {
      ++length;
    }
  }
  matched_ranges->emplace_back(start, start + length);
}

// Returns the maximum score for the given matrix, then backtracks to fill in
// `matched_ranges`. See fuzzy_finder.md for extended discussion.
int ScoreForMatrix(const std::vector<int> score_matrix,
                   size_t width,
                   size_t height,
                   const std::vector<size_t> codepoint_to_offset,
                   std::vector<gfx::Range>* matched_ranges) {
  // Find winning score and its index.
  size_t max_index = 0;
  int max_score = 0;
  for (size_t i = 0; i < width; i++) {
    int score = score_matrix[(height - 1) * width + i];
    if (score > max_score) {
      max_score = score;
      max_index = i;
    }
  }

  // Backtrack through the matrix to find matching positions.
  std::vector<size_t> positions = {codepoint_to_offset[max_index]};
  size_t cur_i = max_index;
  size_t cur_j = height - 1;
  while (cur_j > 0) {
    // Move diagonally...
    --cur_i;
    --cur_j;
    // ...then scan left until the score stops increasing.
    int current = score_matrix[cur_j * width + cur_i];
    int left = cur_i == 0 ? 0 : score_matrix[cur_j * width + cur_i - 1];
    while (current < left) {
      cur_i -= 1;
      if (cur_i == 0)
        break;
      current = left;
      left = score_matrix[cur_j * width + cur_i - 1];
    }
    positions.push_back(codepoint_to_offset[cur_i]);
  }

  base::ranges::reverse(positions);
  ConvertPositionsToRanges(positions, matched_ranges);
  return max_score;
}

}  // namespace

namespace commander {

FuzzyFinder::FuzzyFinder(const std::u16string& needle)
    : needle_(base::i18n::FoldCase(needle)) {
  if (needle_.size() <= kMaxNeedle) {
    score_matrix_.reserve(needle_.size() * kMaxHaystack);
    consecutive_matrix_.reserve(needle_.size() * kMaxHaystack);
  }
}

FuzzyFinder::~FuzzyFinder() = default;

double FuzzyFinder::Find(const std::u16string& haystack,
                         std::vector<gfx::Range>* matched_ranges) {
  matched_ranges->clear();
  if (needle_.size() == 0)
    return 0;
  const std::u16string& folded = base::i18n::FoldCase(haystack);
  size_t m = needle_.size();
  size_t n = folded.size();
  // Special case 0: M > N. We don't allow skipping anything in |needle|, so
  // no match possible.
  if (m > n) {
    return 0;
  }
  // Special case 1: M == N. It must be either an exact match,
  // or a non-match.
  if (m == n) {
    if (folded == needle_) {
      matched_ranges->emplace_back(0, needle_.length());
      return kMaxScore;
    } else {
      return 0;
    }
  }
  // Special case 2: needle is a prefix of haystack
  if (base::StartsWith(folded, needle_)) {
    matched_ranges->emplace_back(0, needle_.length());
    return kPrefixScore;
  }
  // Special case 3: M == 1. Scan through all matches, and return:
  //    no match ->
  //      0
  //    prefix match ->
  //      kPrefixScore (but should have been handled above)
  //    word boundary match (e.g. needle: j, haystack "Orange [J]uice") ->
  //      kVeryHighScore
  //    any other match ->
  //      Scored based on how far into haystack needle is found, normalized by
  //      haystack length.
  if (m == 1) {
    size_t substring_position = folded.find(needle_);
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
      substring_position = folded.find(needle_, substring_position + 1);
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
  double score = ConsecutiveMatchWithGaps(needle_, folded, matched_ranges);
  if (score == 0) {
    matched_ranges->clear();
    return 0;
  } else if (n > kMaxHaystack || m > kMaxNeedle) {
    return score;
  }
  matched_ranges->clear();
  return MatrixMatch(needle_, folded, matched_ranges);
}

double FuzzyFinder::MatrixMatch(const std::u16string& needle_string,
                                const std::u16string& haystack_string,
                                std::vector<gfx::Range>* matched_ranges) {
  static constexpr int kMatchScore = 16;
  static constexpr int kBoundaryBonus = 8;
  static constexpr int kConsecutiveBonus = 4;
  static constexpr int kInitialBonus = kBoundaryBonus * 2;
  static constexpr int kGapStart = 3;
  static constexpr int kGapExtension = 1;

  const size_t m = LengthInCodePoints(needle_string);
  const size_t n = LengthInCodePoints(haystack_string);

  DCHECK_LE(m, kMaxNeedle);
  DCHECK_LE(n, kMaxHaystack);
  score_matrix_.assign(m * n, 0);
  consecutive_matrix_.assign(m * n, 0);
  word_boundaries_.assign(n, false);
  codepoint_to_offset_.assign(n, 0);

  base::i18n::UTF16CharIterator needle(needle_string);
  base::i18n::UTF16CharIterator haystack(haystack_string);

  // Fill in first row and word boundaries.
  bool in_gap = false;
  int32_t needle_code_point = needle.get();
  int32_t haystack_code_point;
  word_boundaries_[0] = true;
  while (!haystack.end()) {
    haystack_code_point = haystack.get();
    size_t i = haystack.char_offset();
    codepoint_to_offset_[i] = haystack.array_pos();
    if (i < n - 1)
      word_boundaries_[i + 1] = u_isUWhiteSpace(haystack_code_point);
    int bonus = word_boundaries_[i] ? kInitialBonus : 0;
    if (needle_code_point == haystack_code_point) {
      consecutive_matrix_[i] = 1;
      score_matrix_[i] = kMatchScore + bonus;
      in_gap = false;
    } else {
      int penalty = in_gap ? kGapExtension : kGapStart;
      int left_score = i > 0 ? score_matrix_[i - 1] : 0;
      score_matrix_[i] = std::max(left_score - penalty, 0);
      in_gap = true;
    }
    haystack.Advance();
  }

  while (!haystack.start())
    haystack.Rewind();
  needle.Advance();

  // Fill in rows 1 through n -1:
  while (!needle.end()) {
    in_gap = false;
    while (!haystack.end()) {
      size_t j = needle.char_offset();
      size_t i = haystack.char_offset();
      size_t idx = i + (j * n);
      if (i < j) {
        // Since all of needle must match, by the time we've gotten to the nth
        // character of needle, at least n - 1 characters of haystack have been
        // consumed.
        haystack.Advance();
        continue;
      }
      // If we choose `left_score`, we're either creating or extending a gap.
      int left_score = i > 0 ? score_matrix_[idx - 1] : 0;
      int penalty = in_gap ? kGapExtension : kGapStart;
      left_score -= penalty;
      // If we choose `diagonal_score`, we're extending a match.
      int diagonal_score = 0;
      int consecutive = 0;
      if (needle.get() == haystack.get()) {
        DCHECK_GT(j, 0u);
        DCHECK_GE(i, j);
        // DCHECKs above show that this index is valid.
        size_t diagonal_index = idx - n - 1;
        diagonal_score = score_matrix_[diagonal_index] + kMatchScore;
        if (word_boundaries_[j]) {
          diagonal_score += kBoundaryBonus;
          // If we're giving a boundary bonus, it implies that this position
          // is an "acronym" type match rather than a "consecutive string"
          // type match, so reset consecutive to not double dip.
          consecutive = 1;
        } else {
          consecutive = consecutive_matrix_[idx] + 1;
          if (consecutive > 1) {
            // Find the beginning of this consecutive run.
            size_t run_start = i - consecutive;
            diagonal_score += word_boundaries_[run_start] ? kBoundaryBonus
                                                          : kConsecutiveBonus;
          }
        }
      }
      in_gap = left_score > diagonal_score;
      consecutive_matrix_[idx] = in_gap ? 0 : consecutive;
      score_matrix_[idx] = std::max(0, std::max(left_score, diagonal_score));
      haystack.Advance();
    }
    while (!haystack.start())
      haystack.Rewind();
    needle.Advance();
  }

  const int raw_score =
      ScoreForMatrix(score_matrix_, n, m, codepoint_to_offset_, matched_ranges);
  const int max_possible_score =
      kInitialBonus + kMatchScore + (kBoundaryBonus + kMatchScore) * (m - 1);
  // But that said, in most cases, good matches will score well below this, so
  // let's saturate a little.
  constexpr float kScoreBias = 0.25;
  const double score =
      kScoreBias +
      (static_cast<double>(raw_score) / max_possible_score) * (1 - kScoreBias);
  DCHECK_LE(score, 1.0);
  // Make sure it scores below exact matches and prefixes.
  return score * kVeryHighScore;
}

}  // namespace commander
