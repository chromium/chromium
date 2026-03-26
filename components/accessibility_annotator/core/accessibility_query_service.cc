// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_query_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/strings/string_util.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"

namespace accessibility_annotator {

AccessibilityQueryService::AccessibilityQueryService(
    std::vector<std::unique_ptr<MemoryDataProvider>> data_providers)
    : data_providers_(std::move(data_providers)),
      classifier_(CreateQueryClassifier()) {}

AccessibilityQueryService::~AccessibilityQueryService() = default;

void AccessibilityQueryService::Shutdown() {
  data_providers_.clear();
}

void AccessibilityQueryService::Query(
    std::u16string_view query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback) {
  if (data_providers_.empty()) {
    update_callback.Run(
        MemorySearchResults(MemorySearchStatus::kInternalFailure));
    return;
  }

  ClassifiedQuery classified_query = classifier_.Run(std::u16string(query));
  if (classified_query.intent == QueryIntentType::kUnknown) {
    update_callback.Run(
        MemorySearchResults(MemorySearchStatus::kUnsupportedQuery));
    return;
  }

  std::vector<MemorySearchResult> entries;
  for (const std::unique_ptr<MemoryDataProvider>& provider : data_providers_) {
    base::Extend(entries, provider->RetrieveAll(classified_query.intent));
  }

  if (classified_query.filter_words.empty()) {
    update_callback.Run(MemorySearchResults(
        MemorySearchStatus::kFinalResponseSuccess, std::move(entries)));
    return;
  }

  // Returns true if all words in `filter_words` are present in `entry.value`.
  // Note: `classified_query.filter_words` are guaranteed to be lowercase as
  // they are processed by `QueryClassifier`.
  auto all_filter_words_present = [&](const MemorySearchResult& entry) {
    std::u16string haystack = base::ToLowerASCII(entry.value);
    return std::ranges::all_of(
        classified_query.filter_words, [&](const std::u16string& word) {
          return internal::ContainsStandalonePhrase(haystack, word);
        });
  };

  // Filters results by ensuring every filter word in response is
  // present in the result's value (case-insensitive).
  std::vector<MemorySearchResult> filtered_entries;
  std::ranges::copy_if(entries, std::back_inserter(filtered_entries),
                       all_filter_words_present);

  // If the strict filtering removes all items, it falls back to returning
  // the original, unfiltered results.
  update_callback.Run(MemorySearchResults(
      MemorySearchStatus::kFinalResponseSuccess,
      filtered_entries.empty() ? std::move(entries)
                               : std::move(filtered_entries)));
}

}  // namespace accessibility_annotator
