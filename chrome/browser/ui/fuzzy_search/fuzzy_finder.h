// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_FINDER_H_
#define CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_FINDER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/range/range.h"

class FuzzySearchItem;

// Represents a search match returned by FuzzyFinder, containing the matching
// item and associated metadata for UI rendering.
struct FuzzySearchResult {
  raw_ptr<FuzzySearchItem> item = nullptr;
  double score = 0.0;
  std::vector<gfx::Range> match_ranges;
};

// Performs fuzzy search over a collection of FuzzySearchItems (matching against
// fields such as title, secondary text, and synonyms) to find relevant items
// for a user query.
class FuzzyFinder {
 public:
  explicit FuzzyFinder(std::vector<raw_ptr<FuzzySearchItem>> searchable_items);

  FuzzyFinder(const FuzzyFinder&) = delete;
  FuzzyFinder& operator=(const FuzzyFinder&) = delete;
  ~FuzzyFinder();

  // Returns true if the trimmed_query meets the minimum length threshold
  // required for a search. trimmed_query must already have leading and trailing
  // whitespace removed.
  static bool HasMinQueryLength(std::u16string_view trimmed_query);

  // Searches searchable_items_ for items matching query via case- and
  // accent-insensitive substring matching against item titles.
  //
  // Parameters:
  //   query: The user-provided string to search for.
  //   max_results: The maximum number of results to return. Callers must
  //                explicitly set this to match their UI capacity, which
  //                prevents unbounded searches and saves CPU cycles.
  //
  // Returns up to max_results matching items. Returns an empty vector if:
  // - The query does not meet the minimum non-whitespace character threshold.
  // - No items match.
  // - searchable_items_ is empty or max_results is 0.
  std::vector<FuzzySearchResult> Find(std::u16string_view query,
                                      size_t max_results);

  // Performs a fuzzy search / string approximation over `searchable_items_`
  // which takes into account typos, letter transpositions, and word boundary
  // tolerances. Each field in a `FuzzySearchItem` is weighted differently (i.e.
  // titles have a higher influence on an item's score than synonyms or
  // secondary text). Any match with score below a minimum confidence threshold
  // is dropped.
  //
  // Returns up to max_results matching items ordered by descending score.
  std::vector<FuzzySearchResult> FuzzyFind(std::u16string_view query,
                                           size_t max_results);

 private:
  // Represents the character alignment decision made at each position when
  // matching a query against a candidate string. Used when backtracking from
  // the best match end position to extract exact matching character spans for
  // UI bolding.
  enum class MatchStep {
    kNone,
    // Candidate character was skipped (gap between query characters).
    kSkipCandidate,
    // Exact character match (e.g. query 'a' == candidate 'a'). Highlighted in
    // UI.
    kExactMatch,
    // Adjacent characters were transposed (e.g. "teh" vs "the"). Highlighted
    // in UI.
    kTransposition,
    // Character substitution / typo (e.g. 'g' -> 'f'). Valid alignment for
    // fuzzy scoring, but NOT an exact match so it is omitted from UI
    // highlighting.
    kSubstitution,
  };

  // One cell of the alignment matrix. The cell at row j, column i describes
  // the best alignment of query prefix 0..j with candidate prefix 0..i.
  struct AlignmentCell {
    // Optimal alignment score.
    int score = 0;
    // Length of the contiguous matching run ending at (j, i).
    int consecutive = 0;
    // Alignment step taken to reach (j, i), used for backtracking match
    // ranges.
    MatchStep step = MatchStep::kNone;
  };

  // Scores an item across its title, secondary text, and synonyms using the
  // fuzzy sequence alignment algorithm. If the item's title is the best
  // match, populates `match_ranges` with the character spans of the match.
  // Leaves `match_ranges` empty if secondary text or synonyms produced the
  // best match.
  double ScoreItem(const FuzzySearchItem* item,
                   std::u16string_view norm_query,
                   std::vector<gfx::Range>& match_ranges);

  // Evaluates a candidate string against a query with typo, transposition, and
  // boundary tolerance using reusable scratch buffers. Returns a normalized
  // confidence score in [0.0, 1.0]. If `match_ranges` is provided, populates
  // it with exact match character spans via backtracking.
  double MatchCandidate(std::u16string_view query,
                        std::u16string_view candidate,
                        std::vector<gfx::Range>* match_ranges = nullptr);

  std::vector<raw_ptr<FuzzySearchItem>> searchable_items_;

  // Scratch buffer instantiated once upon FuzzyFinder construction and reused
  // across candidate alignments to eliminate dynamic heap allocations during
  // searches.
  std::vector<AlignmentCell> alignment_matrix_;
};

#endif  // CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_FINDER_H_
