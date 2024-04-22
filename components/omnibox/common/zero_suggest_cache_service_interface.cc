// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/zero_suggest_cache_service_interface.h"

#include <string>
#include <utility>
#include <vector>

#include "base/trace_event/memory_usage_estimator.h"

using CacheEntry = ZeroSuggestCacheServiceInterface::CacheEntry;

CacheEntry::CacheEntry() = default;

CacheEntry::CacheEntry(std::string response_json)
    : response_json(std::move(response_json)) {}

CacheEntry::CacheEntry(const CacheEntry& entry) = default;

CacheEntry::~CacheEntry() = default;

size_t CacheEntry::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(response_json);
}

ZeroSuggestCacheServiceInterface::CacheEntrySuggestResult::
    CacheEntrySuggestResult(std::vector<int> subtypes,
                            std::u16string suggestion)
    : subtypes(subtypes), suggestion(suggestion) {}

ZeroSuggestCacheServiceInterface::CacheEntrySuggestResult::
    CacheEntrySuggestResult(const CacheEntrySuggestResult& entry) = default;

ZeroSuggestCacheServiceInterface::CacheEntrySuggestResult::
    ~CacheEntrySuggestResult() = default;
