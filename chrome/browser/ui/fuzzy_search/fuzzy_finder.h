// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_FINDER_H_
#define CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_FINDER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"

class FuzzySearchItem;

// Represents a search match returned by FuzzyFinder, containing the matching
// item and associated metadata for UI rendering.
struct FuzzySearchResult {
  raw_ptr<FuzzySearchItem> item = nullptr;
  double score = 0.0;
};

// Performs fuzzy search over a collection of FuzzySearchItems (matching against
// fields such as title, secondary text, and synonyms) to find relevant items
// for a user query.
class FuzzyFinder {
 public:
  explicit FuzzyFinder(std::vector<FuzzySearchItem*> searchable_items);

  FuzzyFinder(const FuzzyFinder&) = delete;
  FuzzyFinder& operator=(const FuzzyFinder&) = delete;
  ~FuzzyFinder();

  // Searches searchable_items_ for items matching query.
  //
  // Parameters:
  //   query: The user-provided string to search for.
  //   max_results: The maximum number of results to return. Callers must
  //                explicitly set this to match their UI capacity, which
  //                prevents unbounded searches and saves CPU cycles.
  //
  // Returns up to max_results matching items. Returns an empty vector if:
  // - The query has fewer than 3 non-whitespace characters.
  // - No items match.
  // - searchable_items_ is empty or max_results is 0.
  std::vector<FuzzySearchResult> Find(const std::u16string& query,
                                      size_t max_results);

  // Performs a fuzzy search / string approximation over `searchable_items_`
  // which takes into account typos, letter transpositions, and word boundary
  // tolerances. Each field in a `FuzzySearchItem` is weighted differently (i.e.
  // titles may have a higher influence on an items score than its synonyms).
  //
  // Returns up to max_results matching items ordered by descending score.
  std::vector<FuzzySearchResult> FuzzyFind(const std::u16string& query,
                                           size_t max_results);

 private:
  std::vector<FuzzySearchItem*> searchable_items_;
};

#endif  // CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_FINDER_H_
