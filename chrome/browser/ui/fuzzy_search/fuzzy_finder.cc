// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/i18n/string_search.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_search_item.h"

namespace {

constexpr size_t kMinQueryLength = 3;

// Returns true if the query meets the standard minimum search length.
bool HasMinQueryLength(std::u16string_view query) {
  return query.length() >= kMinQueryLength;
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
