// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_CACHE_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_CACHE_SERVICE_H_

#include <string>

#include "base/containers/lru_cache.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"

class ZeroSuggestCacheService : public KeyedService {
 public:
  struct CacheEntry {
    CacheEntry();
    explicit CacheEntry(const std::string& response_json);
    CacheEntry(const CacheEntry& entry);

    CacheEntry& operator=(const CacheEntry& entry) = default;

    ~CacheEntry();

    // JSON response received from the remote Suggest service.
    std::string response_json;

    // Estimates dynamic memory usage.
    // See base/trace_event/memory_usage_estimator.h for more info.
    size_t EstimateMemoryUsage() const;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Notifies listeners when a particular cache entry has been updated.
    virtual void OnZeroSuggestResponseUpdated(const std::string& page_url,
                                              const CacheEntry& response) {}
  };

  explicit ZeroSuggestCacheService(size_t cache_size);

  ZeroSuggestCacheService(const ZeroSuggestCacheService&) = delete;
  ZeroSuggestCacheService& operator=(const ZeroSuggestCacheService&) = delete;

  ~ZeroSuggestCacheService() override;

  // Read/write zero suggest cache entries.
  CacheEntry ReadZeroSuggestResponse(const std::string& page_url) const;
  void StoreZeroSuggestResponse(const std::string& page_url,
                                const CacheEntry& response);

  // Remove all zero suggest cache entries.
  void ClearCache();

  // Returns whether or not the zero suggest cache is empty.
  bool IsCacheEmpty() const;

  // Add/remove observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Cache mapping each page URL to the corresponding zero suggest response
  // (serialized JSON). |mutable| is used here because reading from the cache,
  // while logically const, will actually modify the internal recency list of
  // the HashingLRUCache object.
  mutable base::HashingLRUCache<std::string, CacheEntry> cache_;
  base::ObserverList<Observer> observers_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_CACHE_SERVICE_H_
