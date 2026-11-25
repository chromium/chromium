// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/string_search.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_search_item.h"

namespace {

// Minimum non-whitespace character threshold required for a search query.
// Queries shorter than this return an empty result set.
constexpr size_t kMinQueryLength = 2;

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

// Discounts word-by-word multi-word scores so an exact in-order phrase match
// ranks above a split or cross-field match.
constexpr double kMultiTokenScale = 0.88;

// Returns true if index `i` of `text` is the start of the string or immediately
// follows a whitespace delimiter.
bool IsWordBoundary(std::u16string_view text, size_t i) {
  return i == 0 || base::IsUnicodeWhitespace(text[i - 1]);
}

using ExactSearch = base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents;

gfx::Range FindExactMatchRange(ExactSearch& search, std::u16string_view text) {
  size_t match_start = 0;
  size_t match_length = 0;
  if (!search.Search(text, &match_start, &match_length) || match_length == 0) {
    return gfx::Range::InvalidRange();
  }
  return gfx::Range(match_start, match_start + match_length);
}

std::vector<gfx::Range> MergeRanges(std::vector<gfx::Range> ranges) {
  std::ranges::sort(ranges);
  std::vector<gfx::Range> merged;
  for (const gfx::Range& range : ranges) {
    if (!merged.empty() && range.start() <= merged.back().end()) {
      merged.back().set_end(std::max(merged.back().end(), range.end()));
    } else {
      merged.push_back(range);
    }
  }
  return merged;
}

// An exact searcher for one word of a multi-word query. Single-character
// words only match at word starts, so a trailing character typed mid-word
// doesn't highlight an unrelated letter.
struct TokenSearch {
  std::unique_ptr<ExactSearch> search;
  bool word_start_only = false;
};

// Finds the first match of `search` in `text` that begins at a word boundary,
// or InvalidRange() if none.
gfx::Range FindWordStartMatchRange(ExactSearch& search,
                                   std::u16string_view text) {
  for (size_t offset = 0; offset < text.size(); ++offset) {
    if (!IsWordBoundary(text, offset)) {
      continue;
    }
    const gfx::Range match = FindExactMatchRange(search, text.substr(offset));
    if (!match.IsValid()) {
      return gfx::Range::InvalidRange();
    }
    const size_t match_start = offset + match.start();
    if (IsWordBoundary(text, match_start)) {
      return gfx::Range(match_start, match_start + match.length());
    }
    offset = match_start;
  }
  return gfx::Range::InvalidRange();
}

// Computes the ranges of `title` to highlight: the first contiguous case- and
// accent-insensitive match of the whole query or, failing that, the first such
// match of each of `token_searches` (the words of a multi-word query)
// regardless of order.
//
// E.g. for query "history open" and title "Open history page", the whole query
// has no match while "history" matches [5, 12) and "open" matches [0, 4),
// highlighting "Open" and "history".
std::vector<gfx::Range> ComputeHighlightRanges(
    std::u16string_view title,
    ExactSearch& full_search,
    const std::vector<TokenSearch>& token_searches) {
  if (const gfx::Range full_range = FindExactMatchRange(full_search, title);
      full_range.IsValid()) {
    return {full_range};
  }
  std::vector<gfx::Range> ranges;
  for (const TokenSearch& token_search : token_searches) {
    const gfx::Range range =
        token_search.word_start_only
            ? FindWordStartMatchRange(*token_search.search, title)
            : FindExactMatchRange(*token_search.search, title);
    if (range.IsValid()) {
      ranges.push_back(range);
    }
  }
  return MergeRanges(std::move(ranges));
}

}  // namespace

FuzzyFinder::FuzzyFinder(std::vector<raw_ptr<FuzzySearchItem>> searchable_items)
    : searchable_items_(std::move(searchable_items)) {}

FuzzyFinder::FuzzyFinder(std::vector<raw_ptr<FuzzySearchItem>> searchable_items,
                         ScoringParams params)
    : searchable_items_(std::move(searchable_items)),
      params_{
          .title = std::clamp(params.title, 0.0, 1.0),
          .secondary_text = std::clamp(params.secondary_text, 0.0, 1.0),
          .synonym = std::clamp(params.synonym, 0.0, 1.0),
          .min_score = std::clamp(params.min_score, 0.0, 1.0),
      } {}

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

  // Reject queries shorter than the minimum threshold to avoid
  // broad/low-signal results.
  if (!HasMinQueryLength(trimmed_query)) {
    return {};
  }

  ExactSearch search{std::u16string(trimmed_query)};

  std::vector<FuzzySearchResult> results;
  results.reserve(max_results);

  // Iterate through the searchable items and perform the search.
  for (FuzzySearchItem* item : searchable_items_) {
    CHECK(item);
    const gfx::Range match_range =
        FindExactMatchRange(search, item->GetTitle());
    if (match_range.IsValid()) {
      FuzzySearchResult result;
      result.item = item;
      result.match_ranges.push_back(match_range);
      result.highlight_ranges.push_back(match_range);
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
  const std::vector<std::u16string_view> tokens =
      base::SplitStringPiece(normalized_query, base::kWhitespaceUTF16,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  const bool is_multi_word = tokens.size() >= 2;
  if (is_multi_word &&
      std::ranges::none_of(tokens, [](std::u16string_view token) {
        return token.size() >= kMinQueryLength;
      })) {
    return {};
  }

  std::vector<FuzzySearchResult> results;
  results.reserve(searchable_items_.size());

  for (FuzzySearchItem* item : searchable_items_) {
    CHECK(item);
    std::vector<gfx::Range> match_ranges;
    double score = ScoreItem(item, normalized_query, match_ranges);

    if (is_multi_word) {
      double token_sum = 0.0;
      std::vector<gfx::Range> token_ranges;
      bool all_matched = true;
      for (std::u16string_view token : tokens) {
        std::vector<gfx::Range> ranges;
        const double token_score = ScoreItem(item, token, ranges);
        if (token_score <= 0.0 || token_score < params_.min_score) {
          all_matched = false;
          break;
        }
        token_sum += token_score;
        token_ranges.insert(token_ranges.end(), ranges.begin(), ranges.end());
      }
      if (!all_matched) {
        continue;
      }
      const double split_score = (token_sum / tokens.size()) * kMultiTokenScale;
      if (split_score > score) {
        score = split_score;
        match_ranges = MergeRanges(std::move(token_ranges));
      }
    }

    // Non-matches (score 0) are always dropped, even if `min_score` is 0.
    if (score > 0.0 && score >= params_.min_score) {
      results.push_back(
          FuzzySearchResult{item, score, std::move(match_ranges)});
    }
  }

  // Stable sort in descending order by score plus the item's bonus weight
  // (preserving insertion order on ties). Only items that passed the cutoff
  // above reach the sort, so a bonus reorders matches and never turns a
  // non-match into a match.
  std::ranges::stable_sort(results, std::ranges::greater(),
                           [](const FuzzySearchResult& result) {
                             const double bonus = result.item->GetBonusWeight();
                             CHECK(bonus >= 0.0 && bonus <= 1.0);
                             return result.score + bonus;
                           });

  if (results.size() > max_results) {
    results.resize(max_results);
  }

  if (!results.empty()) {
    ExactSearch full_search{std::u16string(trimmed_query)};
    std::vector<TokenSearch> token_searches;
    if (is_multi_word) {
      // Split the query before lowercasing so a word's character count is
      // not skewed by case mappings that change its length (e.g. "İ").
      for (std::u16string_view token : base::SplitStringPiece(
               trimmed_query, base::kWhitespaceUTF16, base::TRIM_WHITESPACE,
               base::SPLIT_WANT_NONEMPTY)) {
        base::i18n::UTF16CharIterator iter(token);
        const bool is_single_char = iter.Advance() && iter.end();
        token_searches.push_back(
            {std::make_unique<ExactSearch>(std::u16string(token)),
             /*word_start_only=*/is_single_char});
      }
    }
    for (FuzzySearchResult& result : results) {
      result.highlight_ranges = ComputeHighlightRanges(
          result.item->GetTitle(), full_search, token_searches);
    }
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
      params_.title;

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
  score_field(item->GetSecondaryText(), params_.secondary_text);
  for (const std::u16string& synonym : item->GetSynonyms()) {
    score_field(synonym, params_.synonym);
  }

  return best_score;
}

// Matrix Representation Example:
// Query "tab" (M=3) vs. Candidate "tabs" (N=4). Rows are query characters (j)
// and columns are candidate characters (i). Cells with i < j are never
// evaluated ("-").
//
// clang-format off
//   Candidate   't' (i=0)     'a' (i=1)     'b' (i=2)     's' (i=3)
//             +-------------+-------------+-------------+-------------+
//   't' (j=0) | 32 (match)  | 26 (gap -6) | 24 (gap -2) | 22 (gap -2) |
//             +-------------+-------------+-------------+-------------+
//   'a' (j=1) |      -      | 54 (match)  | 48 (gap -6) | 46 (gap -2) |
//             +-------------+-------------+-------------+-------------+
//   'b' (j=2) |      -      |      -      | 76 (match)  | 70 (gap -6) |
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

  // Row j starts at candidate index i = j, so a query longer than the
  // candidate can never be fully aligned.
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
      // For j > 0, only allow diagonal steps if the previous query prefix had
      // a valid alignment (score > 0) to ensure full query coverage.
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
  // Row M - 1 holds alignments of the full query. The match may end at any
  // candidate position, so take the best score in that row, breaking ties in
  // favor of the longer consecutive run.
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
          // `curr_j`; not an exact character match, so omit from
          // match_ranges.
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
  // `max_possible` is the score of a query whose every character matches at a
  // word boundary. Normalizing by it keeps scores comparable across query
  // lengths, and kScoreBias lifts any full alignment above non-matches.
  const double max_possible =
      kInitialBoundaryBonus + kMatchScore +
      (kBoundaryBonus + kMatchScore) * static_cast<double>(m - 1);

  constexpr double kMaxCoverageBonus = 0.02;
  const double alignment_norm =
      kScoreBias +
      (max_score / max_possible) * (1.0 - kScoreBias - kMaxCoverageBonus);
  const double coverage_bonus =
      (static_cast<double>(m) / n) * kMaxCoverageBonus;
  return std::clamp(alignment_norm + coverage_bonus, 0.0, 1.0);
}
