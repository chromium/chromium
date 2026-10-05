// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_search_item.h"

namespace {

// Minimum non-whitespace character threshold required for a search query.
// Queries shorter than this return an empty result set.
constexpr size_t kMinQueryLength = 2;

// Multiplier applied to scores when a match is found in an item's title.
constexpr double kTitleWeight = 1.00;

// Multiplier applied to scores when a match is found in an item's synonyms.
constexpr double kSynonymWeight = 0.85;

// Multiplier applied to scores when a match is found in an item's secondary
// text (e.g. section headers or descriptions).
constexpr double kSecondaryTextWeight = 0.80;

// Base score awarded for each matching character between query and candidate.
constexpr int kMatchScore = 16;

// Extra bonus awarded when matching a character at the start of a candidate
// string.
constexpr int kInitialBoundaryBonus = 16;

// Extra bonus awarded when matching a character at a subsequent word boundary.
constexpr int kBoundaryBonus = 8;

// Bonus added for each character matched contiguously in an unbroken streak.
constexpr int kConsecutiveBonus = 6;

// Penalty subtracted when a character does not match (substitution / typo).
constexpr int kTypoPenalty = -12;

// Substitutions (typos) are only allowed for queries longer than this many
// characters. Shorter queries must match exactly or via transpositions.
constexpr size_t kMaxQueryLengthWithoutSubstitution = 3;

// Penalty subtracted when two adjacent characters are transposed (e.g. "teh"
// -> "the").
constexpr int kSwapPenalty = -6;

// Penalty subtracted when starting a gap between matched characters.
constexpr int kGapStartPenalty = 6;

// Penalty subtracted for each additional character skipped in an existing gap.
constexpr int kGapExtensionPenalty = 2;

// Baseline normalization bias in [0.0, 1.0] ensuring qualifying fuzzy matches
// (where all query characters are aligned via exact matches, adjacent
// transpositions, or substitutions) achieve a distinguishable positive score
// range ([0.25, 1.0]) above non-matches (0.0).
constexpr double kScoreBias = 0.25;

// Minimum score cutoff threshold. Results with score below this threshold are
// dropped to filter out weak or low-confidence matches.
constexpr double kMinScore = 0.60;

// Returns true if index `i` of `text` is the start of the string or immediately
// follows a whitespace delimiter.
bool IsWordBoundary(std::u16string_view text, size_t i) {
  return i == 0 || base::IsUnicodeWhitespace(text[i - 1]);
}

}  // namespace

FuzzyFinder::FuzzyFinder(std::vector<raw_ptr<FuzzySearchItem>> searchable_items)
    : searchable_items_(std::move(searchable_items)) {}

FuzzyFinder::~FuzzyFinder() = default;

bool FuzzyFinder::HasMinQueryLength(std::u16string_view trimmed_query) {
  CHECK(trimmed_query == base::TrimWhitespace(trimmed_query, base::TRIM_ALL));
  return trimmed_query.length() >= kMinQueryLength;
}

std::vector<FuzzySearchResult> FuzzyFinder::Find(std::u16string_view query,
                                                 size_t max_results) {
  if (searchable_items_.empty() || max_results == 0) {
    return {};
  }

  // Trim leading and trailing whitespace from the query.
  const std::u16string_view trimmed_query =
      base::TrimWhitespace(query, base::TRIM_ALL);

  // Reject queries shorter than the minimum threshold to avoid broad/low-signal
  // results.
  if (!HasMinQueryLength(trimmed_query)) {
    return {};
  }

  std::vector<FuzzySearchResult> results;
  results.reserve(max_results);

  // Create the searcher object once per query; ignores case and accents.
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents search{
      std::u16string(trimmed_query)};

  // Iterate through the searchable items and perform the search.
  for (FuzzySearchItem* item : searchable_items_) {
    CHECK(item);
    size_t match_start = 0;
    size_t match_length = 0;
    if (search.Search(item->GetTitle(), &match_start, &match_length)) {
      FuzzySearchResult result;
      result.item = item;
      result.match_ranges.emplace_back(match_start, match_start + match_length);
      results.emplace_back(std::move(result));
      if (results.size() >= max_results) {
        break;
      }
    }
  }
  return results;
}

std::vector<FuzzySearchResult> FuzzyFinder::FuzzyFind(std::u16string_view query,
                                                      size_t max_results) {
  if (searchable_items_.empty() || max_results == 0) {
    return {};
  }

  // Trim leading and trailing whitespace from the query.
  const std::u16string_view trimmed_query =
      base::TrimWhitespace(query, base::TRIM_ALL);

  // Reject queries shorter than the minimum threshold.
  if (!HasMinQueryLength(trimmed_query)) {
    return {};
  }

  const std::u16string normalized_query = base::i18n::ToLower(trimmed_query);

  std::vector<FuzzySearchResult> results;
  results.reserve(searchable_items_.size());

  for (FuzzySearchItem* item : searchable_items_) {
    CHECK(item);
    std::vector<gfx::Range> match_ranges;
    const double score = ScoreItem(item, normalized_query, match_ranges);
    if (score >= kMinScore) {
      results.push_back(
          FuzzySearchResult{item, score, std::move(match_ranges)});
    }
  }

  // Stable sort in descending order by score (preserving insertion order on
  // ties).
  std::ranges::stable_sort(results, std::ranges::greater(),
                           &FuzzySearchResult::score);

  if (results.size() > max_results) {
    results.resize(max_results);
  }

  return results;
}

double FuzzyFinder::ScoreItem(const FuzzySearchItem* item,
                              std::u16string_view norm_query,
                              std::vector<gfx::Range>& match_ranges) {
  CHECK(item);

  // TODO(crbug.com/549169077): Support full diacritic/accent folding or
  // transliteration (e.g. via ICU) so accented words at initial positions
  // align identically to non-accented queries.

  // 1. Title match. MatchCandidate() leaves `match_ranges` empty if the title
  // does not match.
  double best_score =
      MatchCandidate(norm_query, base::i18n::ToLower(item->GetTitle()),
                     &match_ranges) *
      kTitleWeight;

  // 2. Secondary text and synonym matches. Match ranges are only reported for
  // the title, so they are cleared if any other field scores higher.
  auto score_field = [&](const std::u16string& text, double weight) {
    const double score =
        MatchCandidate(norm_query, base::i18n::ToLower(text)) * weight;
    if (score > best_score) {
      best_score = score;
      match_ranges.clear();
    }
  };
  score_field(item->GetSecondaryText(), kSecondaryTextWeight);
  for (const std::u16string& synonym : item->GetSynonyms()) {
    score_field(synonym, kSynonymWeight);
  }

  return best_score;
}

// Evaluates a candidate string against a query with typo, transposition, and
// boundary tolerance. Returns a normalized confidence score in [0.0, 1.0].
//
// clang-format off
// Matrix Representation Example:
// Query "tab" (M=3) vs. Candidate "tabs" (N=4):
//
//   Query       't' (i=0)     'a' (i=1)     'b' (i=2)     's' (i=3)
//             +-------------+-------------+-------------+-------------+
//   't' (j=0) | 32 (match)  | 26 (gap -6) | 24 (gap -2) | 22 (gap -2) |
//             +-------------+-------------+-------------+-------------+
//   'a' (j=1) |  0 (no diag)| 54 (match)  | 48 (gap -6) | 46 (gap -2) |
//             +-------------+-------------+-------------+-------------+
//   'b' (j=2) |  0 (no diag)|  0 (no diag)| 76 (match)  | 70 (gap -6) |
//             +-------------+-------------+-------------+-------------+
// clang-format on
//
// Cell Breakdown:
// 1. Cell (0,0): 't' matches 't' at word start:
//    16 (kMatchScore) + 16 (kInitialBoundaryBonus) = 32.
// 2. Cell (1,1): 'a' matches 'a' with streak = 2:
//    diag 32 + 16 (kMatchScore) + 6 (kConsecutiveBonus) = 54.
// 3. Cell (2,2): 'b' matches 'b' with streak = 3:
//    diag 54 + 16 (kMatchScore) + 6 (kConsecutiveBonus) = 76.
// 4. Max score in row 2 (M - 1) is 76 (at i=2).
//
// Max Possible Score Breakdown:
// max_possible = 32 (1st char: kMatchScore 16 + kInitialBoundaryBonus 16)
//              + 24 * (M - 1) (remaining chars:
//                              kMatchScore 16 + kBoundaryBonus 8)
//              = 32 + 24 * (3 - 1) = 80.
//
// Normalized Score:
// norm = 0.25 (kScoreBias) + (76 / 80) * (1.0 - 0.25)
//      = 0.25 + 0.95 * 0.75 = 0.9625.
double FuzzyFinder::MatchCandidate(std::u16string_view query,
                                   std::u16string_view candidate,
                                   std::vector<gfx::Range>* match_ranges) {
  if (match_ranges) {
    match_ranges->clear();
  }

  const size_t m = query.length();
  const size_t n = candidate.length();

  // Guard against empty strings or queries that exceed the candidate length.
  // Because the inner alignment loop for row j starts at candidate index i = j,
  // when m > n the final row (m - 1) is never evaluated (i = j >= n is
  // false), meaning m > n can never produce a match. Early exiting here
  // avoids unnecessary heap matrix allocations.
  //
  // TODO(crbug.com/549169077): Support queries longer than candidate strings.
  if (m == 0 || m > n) {
    return 0.0;
  }

  // Flattened 2D matrix of size M * N, where `alignment_matrix_[j * n + i]`
  // holds the alignment of query prefix 0..j with candidate prefix 0..i.
  alignment_matrix_.assign(m * n, AlignmentCell());

  for (size_t j = 0; j < m; ++j) {
    bool in_gap = false;
    for (size_t i = j; i < n; ++i) {
      const size_t idx = i + (j * n);

      // 1. Horizontal step (skip candidate character / gap propagation).
      int left_score = (i > 0) ? alignment_matrix_[idx - 1].score : 0;
      left_score -= in_gap ? kGapExtensionPenalty : kGapStartPenalty;

      int diagonal_score = 0;
      int consecutive = 0;
      MatchStep diag_step = MatchStep::kNone;

      const bool is_exact_match = (query[j] == candidate[i]);
      // Check for adjacent character transposition (e.g. user typed "teh" for
      // "the").
      const bool is_swap_match = (j > 0 && query[j] == candidate[i - 1] &&
                                  query[j - 1] == candidate[i]);

      // 2. Diagonal match steps:
      // Only allow diagonal steps if the previous query prefix had a
      // valid alignment (score > 0) to ensure full query coverage.
      if (is_exact_match && j == 0) {
        // The first query character is the base case where a new match
        // begins. Matching at a word boundary receives an initial boundary
        // bonus.
        diagonal_score =
            kMatchScore +
            (IsWordBoundary(candidate, i) ? kInitialBoundaryBonus : 0);
        consecutive = 1;
        diag_step = MatchStep::kExactMatch;
      } else if (is_exact_match) {
        const AlignmentCell& diag = alignment_matrix_[idx - n - 1];
        if (diag.score > 0) {
          diagonal_score = diag.score + kMatchScore;
          if (IsWordBoundary(candidate, i)) {
            diagonal_score += kBoundaryBonus;
            consecutive = 1;
          } else {
            consecutive = diag.consecutive + 1;
            if (consecutive > 1) {
              diagonal_score += kConsecutiveBonus;
            }
          }
          diag_step = MatchStep::kExactMatch;
        }
      } else if (is_swap_match) {
        if (j == 1) {
          diagonal_score = (kMatchScore * 2) + kSwapPenalty;
          consecutive = 2;
          diag_step = MatchStep::kTransposition;
        } else {
          const size_t trans_diag_idx = (i - 2) + ((j - 2) * n);
          if (alignment_matrix_[trans_diag_idx].score > 0) {
            diagonal_score = alignment_matrix_[trans_diag_idx].score +
                             (kMatchScore * 2) + kSwapPenalty;
            consecutive = 2;
            diag_step = MatchStep::kTransposition;
          }
        }
      } else if (j > 0 && m > kMaxQueryLengthWithoutSubstitution) {
        // Mismatch / substitution typo: disallowed entirely for short queries
        // and disallowed on the first character (j == 0).
        const AlignmentCell& diag = alignment_matrix_[idx - n - 1];
        if (diag.score > 0) {
          diagonal_score = diag.score + kTypoPenalty;
          consecutive = 0;
          diag_step = MatchStep::kSubstitution;
        }
      }

      in_gap = (left_score > diagonal_score);
      if (in_gap && left_score > 0) {
        alignment_matrix_[idx] = {left_score, 0, MatchStep::kSkipCandidate};
      } else if (!in_gap && diagonal_score > 0) {
        alignment_matrix_[idx] = {diagonal_score, consecutive, diag_step};
      } else {
        alignment_matrix_[idx] = AlignmentCell();
        in_gap = false;
      }
    }
  }

  // --- Final Score Extraction ---
  // The query matching process must account for all M characters of the query.
  // Row (M - 1) holds the scores where the full query has been matched. The
  // match can finish at any character in the candidate string, so we find the
  // maximum score across all columns in the final row.
  const size_t last_row = (m - 1) * n;
  int max_score = 0;
  size_t best_i = 0;
  for (size_t i = 0; i < n; ++i) {
    const int s = alignment_matrix_[last_row + i].score;
    if (s > max_score ||
        (s == max_score && s > 0 &&
         alignment_matrix_[last_row + i].consecutive >
             alignment_matrix_[last_row + best_i].consecutive)) {
      max_score = s;
      best_i = i;
    }
  }

  if (max_score <= 0) {
    return 0.0;
  }

  // --- Backtracking for Match Spans ---
  // Matched candidate indices are visited in strictly decreasing order, so
  // adjacent indices are merged into ranges as they are found and the ranges
  // are reversed into ascending order at the end.
  if (match_ranges) {
    auto add_matched_index = [match_ranges](size_t index) {
      if (!match_ranges->empty() && match_ranges->back().start() == index + 1) {
        match_ranges->back().set_start(index);
      } else {
        match_ranges->emplace_back(index, index + 1);
      }
    };

    int curr_j = static_cast<int>(m) - 1;
    int curr_i = static_cast<int>(best_i);

    bool done = false;
    while (!done && curr_j >= 0 && curr_i >= 0) {
      const size_t idx =
          static_cast<size_t>(curr_i) + static_cast<size_t>(curr_j) * n;
      switch (alignment_matrix_[idx].step) {
        case MatchStep::kExactMatch:
          add_matched_index(static_cast<size_t>(curr_i));
          --curr_j;
          --curr_i;
          break;
        case MatchStep::kTransposition:
          add_matched_index(static_cast<size_t>(curr_i));
          add_matched_index(static_cast<size_t>(curr_i - 1));
          curr_j -= 2;
          curr_i -= 2;
          break;
        case MatchStep::kSubstitution:
          // Character at candidate index `curr_i` substituted query character
          // `curr_j`; not an exact character match, so omit from match_ranges.
          --curr_j;
          --curr_i;
          break;
        case MatchStep::kSkipCandidate:
          --curr_i;
          break;
        case MatchStep::kNone:
          // Not expected when backtracking from a positive score.
          done = true;
          break;
      }
    }

    std::ranges::reverse(*match_ranges);
  }

  // --- Score Normalization ---
  // `max_possible` represents the theoretical maximum score for a query of
  // length M matching ideal word boundaries across multi-word items.
  // Normalizing to [0.0, 1.0] makes confidence scores scale-invariant across
  // different query lengths.
  // The baseline bias (kScoreBias) ensures that any candidate that
  // successfully aligns the full query (via exact matches, adjacent
  // transpositions, or substitutions) maps to [0.25, 1.0], keeping it
  // distinctly above non-matches (0.0).
  const double max_possible =
      kInitialBoundaryBonus + kMatchScore +
      (kBoundaryBonus + kMatchScore) * static_cast<double>(m - 1);

  const double raw_norm =
      kScoreBias + (max_score / max_possible) * (1.0 - kScoreBias);
  return std::clamp(raw_norm, 0.0, 1.0);
}
