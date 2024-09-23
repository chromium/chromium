// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_CACHE_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_CACHE_SERVICE_H_

#include <memory>
#include <string>

#include "base/containers/lru_cache.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/common/zero_suggest_cache_service_interface.h"
#include "components/prefs/pref_service.h"

class AutocompleteSchemeClassifier;

class ZeroSuggestCacheService : public ZeroSuggestCacheServiceInterface,
                                public KeyedService {
 public:
  ZeroSuggestCacheService(
      std::unique_ptr<AutocompleteSchemeClassifier> scheme_classifier,
      PrefService* prefs,
      size_t cache_size);

  ZeroSuggestCacheService(const ZeroSuggestCacheService&) = delete;
  ZeroSuggestCacheService& operator=(const ZeroSuggestCacheService&) = delete;

  ~ZeroSuggestCacheService() override;

  // Read/write zero suggest cache entries.
  CacheEntry ReadZeroSuggestResponse(const std::string& page_url) const;
  void StoreZeroSuggestResponse(const std::string& page_url,
                                const std::string& response_json);

  // Remove all zero suggest cache entries.
  void ClearCache();

  // Returns whether or not the in-memory zero suggest cache is empty.
  bool IsInMemoryCacheEmptyForTesting() const;

  // ZeroSuggestCacheServiceInterface:
  std::vector<ZeroSuggestCacheServiceInterface::CacheEntrySuggestResult>
  GetSuggestResults(const ZeroSuggestCacheServiceInterface::CacheEntry&
                        cache_entry) const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  std::unique_ptr<AutocompleteSchemeClassifier> scheme_classifier_;
  // Pref service used for in-memory cache data persistence. Not owned.
  const raw_ptr<PrefService> prefs_;
  // Cache mapping each page URL to the corresponding zero suggest response
  // (serialized JSON). |mutable| is used here because reading from the cache,
  // while logically const, will actually modify the internal recency list of
  // the HashingLRUCache object.
  mutable base::HashingLRUCache<std::string, CacheEntry> cache_;
  // Dedicated cache entry for "ZPS on NTP" data in order to minimize any
  // negative impact due to cache eviction policy.
  CacheEntry ntp_entry_;
  base::ObserverList<Observer> observers_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_CACHE_SERVICE_H_
