// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_info_cache.h"

#include "components/commerce/core/commerce_types.h"
#include "url/gurl.h"

namespace commerce {

CommerceInfoCache::CacheEntry::CacheEntry() = default;
CommerceInfoCache::CacheEntry::~CacheEntry() = default;

CommerceInfoCache::CommerceInfoCache() = default;
CommerceInfoCache::~CommerceInfoCache() = default;

void CommerceInfoCache::AddRef(const GURL& url) {
  const std::string url_string = url.spec();
  auto it = referenced_urls_.find(url_string);
  if (it == referenced_urls_.end()) {
    referenced_urls_[url_string] = 0;

    cache_.emplace(url.spec(),
                   std::make_unique<CommerceInfoCache::CacheEntry>());
  }
  referenced_urls_[url_string] = referenced_urls_[url_string] + 1;
}

void CommerceInfoCache::RemoveRef(const GURL& url) {
  const std::string url_string = url.spec();
  auto it = referenced_urls_.find(url_string);
  if (it == referenced_urls_.end()) {
    return;
  }

  referenced_urls_[url_string] = referenced_urls_[url_string] - 1;

  // If no other systems are maintaining the URL, clear the cache entry if it
  // exists.
  if (referenced_urls_[url_string] == 0) {
    referenced_urls_.erase(it);

    auto cache_it = cache_.find(url_string);
    if (cache_it != cache_.end()) {
      if (cache_it->second->run_local_extraction_task.get()) {
        cache_it->second->run_local_extraction_task->Cancel();
        cache_it->second->run_local_extraction_task.reset();
      }
      cache_.erase(cache_it);
    }
  }
}

bool CommerceInfoCache::IsUrlReferenced(const GURL& url) {
  return referenced_urls_.find(url.spec()) != referenced_urls_.end();
}

CommerceInfoCache::CacheEntry* CommerceInfoCache::GetEntryForUrl(
    const GURL& url) {
  auto it = cache_.find(url.spec());
  if (it == cache_.end()) {
    return nullptr;
  }
  return it->second.get();
}

size_t CommerceInfoCache::GetUrlRefCount(const GURL& url) {
  auto it = referenced_urls_.find(url.spec());
  if (it == referenced_urls_.end()) {
    return 0;
  }
  return it->second;
}

}  // namespace commerce
