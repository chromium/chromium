// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_query_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/one_p_resolver.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"

namespace accessibility_annotator {

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
  // We can't query if we don't have any data providers configured.
  if (data_providers_.empty()) {
    update_callback.Run(
        MemorySearchResults(MemorySearchStatus::kInternalFailure));
    return;
  }

  // First, run the query classifier to understand the user's intent,
  // extracting intent type and filter words.
  std::u16string query_str(query);
  classifier_.Run(
      query_str,
      base::BindOnce(&AccessibilityQueryService::OnClassificationComplete,
                     weak_ptr_factory_.GetWeakPtr(), query_str,
                     std::move(update_callback)));
}

void AccessibilityQueryService::OnClassificationComplete(
    std::u16string query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    ClassifiedQuery classified_query) {
  // If the classifier couldn't figure out what the user is asking for, we try
  // the 1P resolver as a fallback.
  if (classified_query.intent == EntryType::kUnknown) {
    QueryOnePResolver(std::move(query), update_callback,
                      /*fallback_entries=*/{},
                      MemorySearchStatus::kUnsupportedQuery);
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
                         std::move(classified_query), update_callback));

  // Request all data providers to fetch entries matching the classified intent.
  for (const std::unique_ptr<MemoryDataProvider>& provider : data_providers_) {
    provider->RetrieveAll(intent, barrier_callback);
  }
}

void AccessibilityQueryService::OnDataRetrieved(
    std::u16string query,
    ClassifiedQuery classified_query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    std::vector<std::vector<MemorySearchResult>> entries_list) {
  // Flatten the list of lists into a single vector of results.
  std::vector<MemorySearchResult> entries;
  for (std::vector<MemorySearchResult>& list : entries_list) {
    base::Extend(entries, std::move(list));
  }

  // If we couldn't find any local results, try the 1P resolver.
  if (entries.empty()) {
    QueryOnePResolver(std::move(query), update_callback,
                      /*fallback_entries=*/{},
                      MemorySearchStatus::kFinalResponseSuccess);
    return;
  }

  // If there are no filter words, we don't need to filter anything out.
  // We can just return all the entries we got from the local providers.
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

  // If the strict filtering removes all items, it falls back to querying
  // the 1P resolver if one is available. The 1P resolver might
  // be able to find relevant results that the strict local filtering missed.
  // TODO: crbug.com/495871024 - Only trigger the 1P resolver if *none* of the
  // filter words were found in the local entries, rather than falling back even
  // on partial matches.
  if (filtered_entries.empty()) {
    QueryOnePResolver(std::move(query), update_callback, std::move(entries),
                      MemorySearchStatus::kFinalResponseSuccess);
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
      query,
      base::BindOnce(
          [](base::RepeatingCallback<void(MemorySearchResults)> update_callback,
             std::vector<MemorySearchResult> fallback_entries,
             MemorySearchStatus fallback_status,
             std::vector<MemorySearchResult> one_p_entries) {
            // If the 1P resolver didn't find any results, we fall back to
            // the provided local entries and status.
            if (one_p_entries.empty()) {
              update_callback.Run(MemorySearchResults(
                  fallback_status, std::move(fallback_entries)));
            } else {
              // The 1P resolver successfully found relevant results, so we
              // return those instead of the fallback.
              update_callback.Run(
                  MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                                      std::move(one_p_entries)));
            }
          },
          update_callback, std::move(fallback_entries), fallback_status));
}

}  // namespace accessibility_annotator
