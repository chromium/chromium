// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_query_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/i18n/break_iterator.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/one_p_resolver.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"

namespace accessibility_annotator {

namespace {

// Tokenizes `text` using native word boundaries and returns true if any
// token exists in `filter_words_set`.
bool TextContainsAnyFilterWord(
    std::u16string_view text,
    const base::flat_set<std::u16string>& filter_words_set) {
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return false;
  }

  while (iter.Advance()) {
    if (iter.IsWord()) {
      std::u16string word = base::ToLowerASCII(iter.GetString());
      if (filter_words_set.contains(word)) {
        return true;
      }
    }
  }
  return false;
}

// Returns true if at least one word in `filter_words_set` is present in
// `entry.value` or any of its `metadata_list` values.
bool EntryMatchesAnyFilterWord(
    const MemorySearchResult& entry,
    const base::flat_set<std::u16string>& filter_words_set) {
  if (TextContainsAnyFilterWord(entry.value, filter_words_set)) {
    return true;
  }
  return std::ranges::any_of(
      entry.metadata_list, [&](const EntryMetadata& metadata) {
        return TextContainsAnyFilterWord(metadata.value, filter_words_set);
      });
}

// Deduplicates search results in `MemorySearchResults`.
// An entry is considered a duplicate if its `type`, `value` and its
// `metadata_list` are identical to an entry already in the unique set.
// The first occurrence of a duplicate entry is preserved, maintaining its
// relative order and other fields (like confidence_score). The `sources` of
// subsequent duplicates are merged into the preserved entry.
void DeduplicateResults(std::vector<MemorySearchResult>& results) {
  std::vector<MemorySearchResult> unique_results;
  unique_results.reserve(results.size());
  for (MemorySearchResult& result : results) {
    auto it = std::ranges::find_if(
        unique_results, [&result](const MemorySearchResult& existing) {
          return existing.type == result.type &&
                 existing.value == result.value &&
                 existing.metadata_list == result.metadata_list;
        });
    if (it != unique_results.end()) {
      for (MemoryEntrySource& source : result.sources) {
        if (!std::ranges::contains(it->sources, source)) {
          it->sources.push_back(std::move(source));
        }
      }
    } else {
      unique_results.push_back(std::move(result));
    }
  }
  results = std::move(unique_results);
}

}  // namespace

AccessibilityQueryService::AccessibilityQueryService(
    std::unique_ptr<AccessibilityQueryServiceDelegate> delegate,
    std::vector<std::unique_ptr<MemoryDataProvider>> data_providers,
    std::unique_ptr<OnePResolver> one_p_resolver,
    optimization_guide::RemoteModelExecutor* remote_model_executor)
    : delegate_(std::move(delegate)),
      data_providers_(std::move(data_providers)),
      one_p_resolver_(std::move(one_p_resolver)),
      classifier_(CreateQueryClassifier(remote_model_executor)) {}

AccessibilityQueryService::~AccessibilityQueryService() = default;

void AccessibilityQueryService::Shutdown() {
  data_providers_.clear();
  one_p_resolver_.reset();
}

void AccessibilityQueryService::Query(
    std::u16string_view query,
    bool full_search,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback) {
  // Invalidate any in-flight queries.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // We can't query if we don't have any data providers configured.
  if (data_providers_.empty()) {
    update_callback.Run(
        MemorySearchResults(MemorySearchStatus::kInternalFailure));
    return;
  }

  // Run the query classifier to understand the user's intent, extracting
  // intent type and filter words.
  classifier_.Run(
      std::u16string(query), full_search,
      base::BindOnce(&AccessibilityQueryService::OnClassificationComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::u16string(query),
                     full_search, std::move(update_callback)));
}

void AccessibilityQueryService::OnClassificationComplete(
    std::u16string query,
    bool full_search,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    ClassifiedQuery classified_query) {
  // If the classifier couldn't figure out what the user is asking for, we try
  // the 1P resolver as a fallback if full search is enabled.
  if (classified_query.intent == EntryType::kUnknown) {
    if (full_search) {
      QueryOnePResolver(std::move(query), update_callback,
                        /*fallback_entries=*/{},
                        MemorySearchStatus::kUnsupportedQuery);
      return;
    }
    update_callback.Run(
        MemorySearchResults(MemorySearchStatus::kUnsupportedQuery));
    return;
  }

  EntryType intent = classified_query.intent;

  // Use a barrier callback to wait for all data providers to return their
  // results. This ensures we don't process partial results. Once all providers
  // finish, `OnDataRetrieved` will be called with the combined results.
  base::RepeatingCallback<void(std::vector<MemorySearchResult>)>
      barrier_callback = base::BarrierCallback<std::vector<MemorySearchResult>>(
          data_providers_.size(),
          base::BindOnce(&AccessibilityQueryService::OnDataRetrieved,
                         weak_ptr_factory_.GetWeakPtr(), std::move(query),
                         full_search, std::move(classified_query),
                         update_callback));

  // Request all data providers to fetch entries matching the classified intent.
  for (const std::unique_ptr<MemoryDataProvider>& provider : data_providers_) {
    auto log_and_call_barrier_callback = base::BindOnce(
        [](std::string_view provider_histogram_suffix,
           base::RepeatingCallback<void(std::vector<MemorySearchResult>)>
               barrier_callback,
           std::vector<MemorySearchResult> results) {
          base::UmaHistogramCounts1000(
              base::StrCat({"AccessibilityAnnotator.AccessibilityQueryService."
                            "ProviderResultCount.",
                            provider_histogram_suffix}),
              results.size());
          barrier_callback.Run(std::move(results));
        },
        provider->GetHistogramSuffix(), barrier_callback);
    provider->RetrieveAll(intent, std::move(log_and_call_barrier_callback));
  }
}

void AccessibilityQueryService::OnDataRetrieved(
    std::u16string query,
    bool full_search,
    ClassifiedQuery classified_query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    std::vector<std::vector<MemorySearchResult>> entries_list) {
  // Flatten the list of lists into a single vector of results.
  std::vector<MemorySearchResult> entries;
  for (std::vector<MemorySearchResult>& list : entries_list) {
    base::Extend(entries, std::move(list));
  }

  DeduplicateResults(entries);

  // If we couldn't find any local results, try the 1P resolver if full search
  // is enabled.
  if (entries.empty()) {
    if (full_search) {
      QueryOnePResolver(std::move(query), update_callback,
                        /*fallback_entries=*/{},
                        MemorySearchStatus::kFinalResponseSuccess);
      return;
    }
    update_callback.Run(MemorySearchResults(
        MemorySearchStatus::kFinalResponseSuccess, std::move(entries)));
    return;
  }

  // If there are no filter words, we don't need to filter anything out.
  // We can just return all the entries we got from the local providers.
  if (classified_query.filter_words.empty()) {
    update_callback.Run(MemorySearchResults(
        MemorySearchStatus::kFinalResponseSuccess, std::move(entries)));
    return;
  }

  base::flat_set<std::u16string> filter_words_set =
      classified_query.filter_words;

  auto any_filter_words_present = [&](const MemorySearchResult& entry) {
    return EntryMatchesAnyFilterWord(entry, filter_words_set);
  };

  // Filters results by ensuring at least one filter word in response is
  // present in the result's value (case-insensitive).
  std::vector<MemorySearchResult> filtered_entries;
  std::ranges::copy_if(entries, std::back_inserter(filtered_entries),
                       any_filter_words_present);

  // If the strict filtering removes all items, it falls back to querying
  // the 1P resolver if one is available and full search is enabled.
  // The 1P resolver might be able to find relevant results that the strict
  // local filtering missed.
  if (filtered_entries.empty()) {
    if (full_search) {
      QueryOnePResolver(std::move(query), update_callback, std::move(entries),
                        MemorySearchStatus::kFinalResponseSuccess);
      return;
    }
    update_callback.Run(MemorySearchResults(
        MemorySearchStatus::kFinalResponseSuccess, std::move(entries)));
    return;
  }

  // We successfully filtered the local entries. Return the filtered results.
  update_callback.Run(MemorySearchResults(
      MemorySearchStatus::kFinalResponseSuccess, std::move(filtered_entries)));
}

void AccessibilityQueryService::QueryOnePResolver(
    std::u16string query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    std::vector<MemorySearchResult> fallback_entries,
    MemorySearchStatus fallback_status) {
  // If the 1P resolver is not available or disabled, immediately return the
  // provided fallback results and status.
  if (!one_p_resolver_) {
    update_callback.Run(
        MemorySearchResults(fallback_status, std::move(fallback_entries)));
    return;
  }

  // Query the 1P resolver as a fallback data source.
  one_p_resolver_->Query(
      query, base::BindOnce(&AccessibilityQueryService::OnOnePResolverComplete,
                            weak_ptr_factory_.GetWeakPtr(), update_callback,
                            std::move(fallback_entries), fallback_status));
}

void AccessibilityQueryService::OnOnePResolverComplete(
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    std::vector<MemorySearchResult> fallback_entries,
    MemorySearchStatus fallback_status,
    std::vector<MemorySearchResult> one_p_entries) {
  // If the 1P resolver didn't find any results, we fall back to
  // the provided local entries and status.
  if (one_p_entries.empty()) {
    update_callback.Run(
        MemorySearchResults(fallback_status, std::move(fallback_entries)));
  } else {
    // The 1P resolver successfully found relevant results, so we
    // return those instead of the fallback.
    DeduplicateResults(one_p_entries);
    update_callback.Run(MemorySearchResults(
        MemorySearchStatus::kFinalResponseSuccess, std::move(one_p_entries)));
  }
}

}  // namespace accessibility_annotator
