// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_INFO_CACHE_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_INFO_CACHE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/time/time.h"

class GURL;

namespace base {
class Time;
}

namespace commerce {

struct PriceInsightsInfo;
struct ProductInfo;

class CommerceInfoCache {
 public:
  struct CacheEntry {
   public:
    CacheEntry();
    CacheEntry(const CacheEntry&) = delete;
    CacheEntry& operator=(const CacheEntry&) = delete;
    ~CacheEntry();

    // Whether the fallback local extraction needs to run for page.
    bool needs_local_extraction_run{false};

    // The time that the local extraction execution started. This is primarily
    // used for metrics.
    base::Time local_extraction_execution_start_time;

    std::unique_ptr<base::CancelableOnceClosure> run_local_extraction_task;

    // The product info associated with the URL or nullptr if not available.
    std::unique_ptr<ProductInfo> product_info;

    // A flag indicating whether we should check for product info on-demand.
    // This will be used to prevent repeated attempts.
    bool run_product_info_on_demand{true};

    // The price insights info associated with the URL or nullptr if not
    // available.
    std::unique_ptr<PriceInsightsInfo> price_insights_info;
  };

  CommerceInfoCache();
  CommerceInfoCache(const CommerceInfoCache&);
  CommerceInfoCache& operator=(const CommerceInfoCache&);
  ~CommerceInfoCache();

  // Notify the cache that a URL is being referenced by some system we care
  // about. This should be called once per instance. For example, if two tabs
  // have the same URL open, this method should be called once for each.
  void AddRef(const GURL& url);

  // Notify the cache that a URL is no longer being used by a particular source.
  // This method should be called 1:1 with RemoveRef. The cache will not remove
  // an entry until all instances of a URL are cleared.
  void RemoveRef(const GURL& url);

  // Returns whether the cache is maintaining a URL. Returning |true| here does
  // not necessarily mean there is information cached, only that it can be.
  bool IsUrlReferenced(const GURL& url);

  // Gets a pointer to the cache entry or |nullptr| if it doesn't exist. The
  // pointer returned here should not be held by the client.
  CommerceInfoCache::CacheEntry* GetEntryForUrl(const GURL& url);

  // Gets the number of times a URL is being referenced in the cache.
  size_t GetUrlRefCount(const GURL& url);

 private:
  // A map of URLs to the number of times that URL is being used or is open.
  std::unordered_map<std::string, size_t> referenced_urls_;
  std::unordered_map<std::string,
                     std::unique_ptr<CommerceInfoCache::CacheEntry>>
      cache_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_INFO_CACHE_H_
