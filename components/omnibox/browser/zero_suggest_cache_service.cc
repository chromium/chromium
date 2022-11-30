// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_cache_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/memory_usage_estimator.h"

ZeroSuggestCacheService::ZeroSuggestCacheService(size_t cache_size)
    : cache_(cache_size) {}

ZeroSuggestCacheService::~ZeroSuggestCacheService() = default;

std::string ZeroSuggestCacheService::ReadZeroSuggestResponse(
    const std::string& page_url) const {
  const auto it = cache_.Get(page_url);
  return it != cache_.end() ? it->second : std::string();
}

void ZeroSuggestCacheService::StoreZeroSuggestResponse(
    const std::string& page_url,
    const std::string& response) {
  cache_.Put(page_url, response);
  base::UmaHistogramCounts1M("Omnibox.ZeroSuggestProvider.CacheMemoryUsage",
                             base::trace_event::EstimateMemoryUsage(cache_));

  for (auto& observer : observers_) {
    observer.OnZeroSuggestResponseUpdated(page_url, response);
  }
}

void ZeroSuggestCacheService::ClearCache() {
  cache_.Clear();
}

bool ZeroSuggestCacheService::IsCacheEmpty() const {
  return cache_.empty();
}

void ZeroSuggestCacheService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ZeroSuggestCacheService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
