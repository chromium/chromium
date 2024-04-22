// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_ZERO_SUGGEST_CACHE_SERVICE_INTERFACE_H_
#define COMPONENTS_OMNIBOX_COMMON_ZERO_SUGGEST_CACHE_SERVICE_INTERFACE_H_

#include <string>
#include <vector>

#include "base/observer_list_types.h"

// Minimal interface for `ZeroSuggestCacheService` so users don't have to
// depend on the full components/omnibox/browser. This was created to avoid the
// dependency cycle:
// Omnibox -> HistoryEmbeddingsService -> PageContentAnnotationsService ->
// ZeroSuggestCacheService (omnibox).
class ZeroSuggestCacheServiceInterface {
 public:
  // The raw JSON response from the remote Suggest service. For memory
  // efficiency reasons, `CacheEntry` does not store the deserialized
  // `SearchSuggestionParser::SuggestResult`s as a data member.
  struct CacheEntry {
    CacheEntry();
    explicit CacheEntry(const std::string response_json);
    CacheEntry(const CacheEntry& entry);
    CacheEntry& operator=(const CacheEntry& entry) = default;
    ~CacheEntry();

    // JSON response received from the remote Suggest service.
    std::string response_json;

    // Estimates dynamic memory usage.
    // See base/trace_event/memory_usage_estimator.h for more info.
    size_t EstimateMemoryUsage() const;
  };

  // A partial view of `SearchSuggestionParser::Result`, deserialized from
  // `CachedEntry`.
  struct CacheEntrySuggestResult {
    CacheEntrySuggestResult(std::vector<int> subtypes,
                            std::u16string suggestion);
    CacheEntrySuggestResult(const CacheEntrySuggestResult& entry);
    ~CacheEntrySuggestResult();
    std::vector<int> subtypes;
    std::u16string suggestion;
  };

  // Parses the stored JSON response in order to extract the list of
  // suggestions received from the remote Suggest service.
  virtual std::vector<CacheEntrySuggestResult> GetSuggestResults(
      const CacheEntry& cache_entry) const = 0;

  class Observer : public base::CheckedObserver {
   public:
    // Notifies listeners when a particular cache entry has been updated.
    virtual void OnZeroSuggestResponseUpdated(const std::string& page_url,
                                              const CacheEntry& response) {}
  };

  // Add/remove observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

#endif  // COMPONENTS_OMNIBOX_COMMON_ZERO_SUGGEST_CACHE_SERVICE_INTERFACE_H_
