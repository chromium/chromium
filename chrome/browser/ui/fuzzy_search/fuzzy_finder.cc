// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"

#include <algorithm>
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

// Base score awarded for each matching character between query and candidate.
constexpr int kMatchScore = 16;

// Extra bonus awarded when matching a character at the start of a candidate
// string.
constexpr int kInitialBoundaryBonus = 16;

// Extra bonus awarded when matching a character at a subsequent word boundary.
constexpr int kBoundaryBonus = 8;

// Bonus added for each character matched contiguously in an unbroken streak.
constexpr int kConsecutiveBonus = 4;

// Penalty subtracted when a character does not match (substitution / typo).
constexpr int kTypoPenalty = -12;

// Penalty subtracted when two adjacent characters are transposed (e.g. "teh"
// -> "the").
constexpr int kSwapPenalty = -6;

// Penalty subtracted when starting a gap between matched characters.
constexpr int kGapStartPenalty = 3;

// Penalty subtracted for each additional character skipped in an existing gap.
constexpr int kGapExtensionPenalty = 1;

// Baseline normalization bias in [0.0, 1.0] ensuring qualifying fuzzy matches
// (where all query characters are aligned via exact matches, adjacent
// transpositions, or substitutions) achieve a distinguishable positive score
// range ([0.25, 1.0]) above non-matches (0.0).
constexpr double kScoreBias = 0.25;

// Returns true if the query meets the standard minimum search length.
bool HasMinQueryLength(std::u16string_view query) {
  return query.length() >= kMinQueryLength;
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
//   't' (j=0) | 32 (match)  | 29 (gap -3) | 28 (gap -1) | 27 (gap -1) |
//             +-------------+-------------+-------------+-------------+
//   'a' (j=1) |  0 (no diag)| 52 (match)  | 49 (gap -3) | 48 (gap -1) |
//             +-------------+-------------+-------------+-------------+
//   'b' (j=2) |  0 (no diag)|  0 (no diag)| 72 (match)  | 69 (gap -3) |
//             +-------------+-------------+-------------+-------------+
// clang-format on
//
// Cell Breakdown:
// 1. Cell (0,0): 't' matches 't' at word start:
//    16 (kMatchScore) + 16 (kInitialBoundaryBonus) = 32.
// 2. Cell (1,1): 'a' matches 'a' with streak = 2:
//    diag 32 + 16 (kMatchScore) + 4 (kConsecutiveBonus) = 52.
// 3. Cell (2,2): 'b' matches 'b' with streak = 3:
//    diag 52 + 16 (kMatchScore) + 4 (kConsecutiveBonus) = 72.
// 4. Max score in row 2 (M - 1) is 72 (at i=2).
//
// Max Possible Score Breakdown:
// max_possible = 32 (1st char: kMatchScore 16 + kInitialBoundaryBonus 16)
//              + 24 * (M - 1) (remaining chars:
//                              kMatchScore 16 + kBoundaryBonus 8)
//              = 32 + 24 * (3 - 1) = 80.
//
// Normalized Score:
// norm = 0.25 (kScoreBias) + (72 / 80) * (1.0 - 0.25)
//      = 0.25 + 0.90 * 0.75 = 0.9250.
double ComputeDpMatrixMatch(std::u16string_view query,
                            std::u16string_view candidate) {
  const size_t m = query.length();
  const size_t n = candidate.length();

  // Guard against empty strings or queries that exceed the candidate length.
  // Because the inner DP loop for row j starts at candidate index i = j,
  // when m > n the final row (m - 1) is never evaluated (i = j >= n is
  // false), meaning m > n can never produce a match. Early exiting here
  // avoids unnecessary heap matrix allocations.
  //
  // TODO(crbug.com/549169077): Support queries longer than candidate strings.
  if (m == 0 || n == 0 || m > n) {
    return 0.0;
  }

  // Precompute word boundaries across the candidate string ahead of time.
  // A character at index `i` is a boundary if it is the start of the string or
  // immediately follows a whitespace delimiter. Precomputing this in O(N)
  // enables O(1) lookups in the inner alignment loop instead of repeatedly
  // scanning preceding characters.
  std::vector<bool> word_boundaries(n, false);
  word_boundaries[0] = true;
  for (size_t i = 1; i < n; ++i) {
    word_boundaries[i] = base::IsUnicodeWhitespace(candidate[i - 1]);
  }

  // Flattened 2D matrices of size M * N:
  // - `score_matrix[j * n + i]`: Optimal score aligning query prefix 0..j with
  //   candidate prefix 0..i.
  // - `consecutive_matrix[j * n + i]`: Length of the contiguous matching run
  //   ending at (j, i).
  // TODO(crbug.com/549169077): Consider passing reusable scratch buffers
  // through ScoreItem to avoid per-candidate heap vector reallocations during
  // large search queries.
  std::vector<int> score_matrix(m * n, 0);
  std::vector<int> consecutive_matrix(m * n, 0);

  // --- Row 0: Align the first query character (j = 0) ---
  // The first character represents the base case where a new match begins.
  // Matching at a word boundary receives an initial boundary bonus. If the
  // character does not match, we propagate score from the left with gap
  // penalties (opening penalty for the first skip, extension penalty for
  // subsequent skips).
  bool in_gap = false;
  for (size_t i = 0; i < n; ++i) {
    if (query[0] == candidate[i]) {
      score_matrix[i] =
          kMatchScore + (word_boundaries[i] ? kInitialBoundaryBonus : 0);
      consecutive_matrix[i] = 1;
      in_gap = false;
    } else {
      const int penalty = in_gap ? kGapExtensionPenalty : kGapStartPenalty;
      const int left_score = (i > 0) ? score_matrix[i - 1] : 0;
      score_matrix[i] = std::max(left_score - penalty, 0);
      in_gap = true;
    }
  }

  // --- Rows 1 to M - 1: Align remaining query characters ---
  for (size_t j = 1; j < m; ++j) {
    in_gap = false;
    for (size_t i = j; i < n; ++i) {
      const size_t idx = i + (j * n);
      const size_t diag_idx = (i - 1) + ((j - 1) * n);

      // 1. Horizontal transition (skip candidate character / gap propagation).
      int left_score = (i > 0) ? score_matrix[idx - 1] : 0;
      left_score -= in_gap ? kGapExtensionPenalty : kGapStartPenalty;

      int diagonal_score = 0;
      int consecutive = 0;

      const bool is_exact_match = (query[j] == candidate[i]);
      // Check for adjacent character transposition (e.g. user typed "teh" for
      // "the").
      const bool is_swap_match =
          (j > 0 && i > 0 && query[j] == candidate[i - 1] &&
           query[j - 1] == candidate[i]);

      // 2. Diagonal transitions:
      // Only allow diagonal transitions if the previous query prefix had a
      // valid alignment (score > 0) to ensure full query coverage.
      if (is_exact_match) {
        if (score_matrix[diag_idx] > 0) {
          diagonal_score = score_matrix[diag_idx] + kMatchScore;
          if (word_boundaries[i]) {
            diagonal_score += kBoundaryBonus;
            consecutive = 1;
          } else {
            consecutive = consecutive_matrix[diag_idx] + 1;
            if (consecutive > 1) {
              diagonal_score += kConsecutiveBonus;
            }
          }
        }
      } else if (is_swap_match) {
        if (j == 1) {
          diagonal_score = (kMatchScore * 2) + kSwapPenalty;
          consecutive = 2;
        } else if (j > 1 && i > 1) {
          const size_t trans_diag_idx = (i - 2) + ((j - 2) * n);
          if (score_matrix[trans_diag_idx] > 0) {
            diagonal_score =
                score_matrix[trans_diag_idx] + (kMatchScore * 2) + kSwapPenalty;
            consecutive = 2;
          }
        }
      } else {
        // Mismatch / substitution typo:
        if (score_matrix[diag_idx] > 0) {
          diagonal_score = score_matrix[diag_idx] + kTypoPenalty;
          consecutive = 0;
        }
      }

      in_gap = (left_score > diagonal_score);
      consecutive_matrix[idx] = in_gap ? 0 : consecutive;
      score_matrix[idx] = std::max(0, std::max(left_score, diagonal_score));
    }
  }

  // --- Final Score Extraction ---
  // The query matching process must account for all M characters of the query.
  // Row (M - 1) holds the scores where the full query has been matched. The
  // match can finish at any character in the candidate string, so we find the
  // maximum score across all columns in the final row.
  int max_score = 0;
  for (size_t i = 0; i < n; ++i) {
    max_score = std::max(max_score, score_matrix[i + ((m - 1) * n)]);
  }

  if (max_score <= 0) {
    return 0.0;
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

// Scores an item across its title and synonyms.
double ScoreItem(const FuzzySearchItem* item, std::u16string_view norm_query) {
  CHECK(item);

  // TODO(crbug.com/549169077): Support full diacritic/accent folding or
  // transliteration (e.g. via ICU) so accented words at initial positions
  // align identically to non-accented queries.

  // 1. Title match
  const std::u16string norm_title = base::i18n::ToLower(item->GetTitle());
  double best_score =
      ComputeDpMatrixMatch(norm_query, norm_title) * kTitleWeight;

  // 2. Synonyms match
  for (const std::u16string& synonym : item->GetSynonyms()) {
    const std::u16string norm_syn = base::i18n::ToLower(synonym);
    const double syn_score =
        ComputeDpMatrixMatch(norm_query, norm_syn) * kSynonymWeight;
    best_score = std::max(best_score, syn_score);
  }

  return best_score;
}

}  // namespace

FuzzyFinder::FuzzyFinder(std::vector<FuzzySearchItem*> searchable_items)
    : searchable_items_(std::move(searchable_items)) {}

FuzzyFinder::~FuzzyFinder() = default;

// TODO(crbug.com/549169077): Implement full fuzzy matching algorithm and
// support matching against title, secondary text, and synonyms.
// Currently implements case- and accent-insensitive substring matching
// against item titles.
std::vector<FuzzySearchResult> FuzzyFinder::Find(const std::u16string& query,
                                                 size_t max_results) {
  if (searchable_items_.empty() || max_results == 0) {
    return {};
  }

  // Trim leading and trailing whitespace from the query.
  std::u16string_view trimmed_query =
      base::TrimWhitespace(query, base::TRIM_ALL);

  // Reject queries shorter than the minimum threshold (3 characters) to avoid
  // broad/low-signal results.
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
    if (search.Search(item->GetTitle(), nullptr, nullptr)) {
      FuzzySearchResult result;
      result.item = item;
      results.emplace_back(result);
      if (results.size() >= max_results) {
        break;
      }
    }
  }
  return results;
}

std::vector<FuzzySearchResult> FuzzyFinder::FuzzyFind(
    const std::u16string& query,
    size_t max_results) {
  if (searchable_items_.empty() || max_results == 0) {
    return {};
  }

  // Trim leading and trailing whitespace from the query.
  const std::u16string_view trimmed_query =
      base::TrimWhitespace(query, base::TRIM_ALL);

  // Reject queries shorter than the minimum threshold (2 characters).
  if (!HasMinQueryLength(trimmed_query)) {
    return {};
  }

  const std::u16string normalized_query = base::i18n::ToLower(trimmed_query);

  std::vector<FuzzySearchResult> results;
  results.reserve(searchable_items_.size());

  for (FuzzySearchItem* item : searchable_items_) {
    CHECK(item);
    const double score = ScoreItem(item, normalized_query);
    if (score > 0.0) {
      results.push_back(FuzzySearchResult{item, score});
    }
  }

  // Stable sort in descending order by score (preserving insertion order on
  // ties).
  std::stable_sort(results.begin(), results.end(),
                   [](const FuzzySearchResult& a, const FuzzySearchResult& b) {
                     return a.score > b.score;
                   });

  if (results.size() > max_results) {
    results.resize(max_results);
  }

  return results;
}
