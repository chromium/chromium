// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_cache_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/prefs/pref_service.h"

using CacheEntry = ZeroSuggestCacheService::CacheEntry;

ZeroSuggestCacheService::ZeroSuggestCacheService(PrefService* prefs,
                                                 size_t cache_size)
    : prefs_(prefs), cache_(cache_size) {
  DCHECK_GT(cache_size, 0u);

  if (prefs_) {
    // Load cached ZPS response for NTP from prior session via prefs.
    ntp_entry_.response_json =
        omnibox::GetUserPreferenceForZeroSuggestCachedResponse(prefs_,
                                                               /*page_url=*/"");
  }
}

ZeroSuggestCacheService::~ZeroSuggestCacheService() {
  if (prefs_) {
    // Dump cached ZPS response for NTP to prefs.
    omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
        prefs_, /*page_url=*/"", /*response=*/ntp_entry_.response_json);
  }
}

CacheEntry ZeroSuggestCacheService::ReadZeroSuggestResponse(
    const std::string& page_url) const {
  // Read cached ZPS response for NTP.
  if (page_url.empty()) {
    return ntp_entry_;
  }

  // Read cached ZPS response for SRP/Web.
  const auto it = cache_.Get(page_url);
  return it != cache_.end() ? it->second : CacheEntry();
}

void ZeroSuggestCacheService::StoreZeroSuggestResponse(
    const std::string& page_url,
    const CacheEntry& response) {
  if (page_url.empty()) {
    // Write ZPS response for NTP to cache.
    ntp_entry_ = response;
  } else {
    // Write ZPS response for SRP/Web to cache.
    cache_.Put(page_url, response);
  }

  base::UmaHistogramCounts1M(
      "Omnibox.ZeroSuggestProvider.CacheMemoryUsage",
      base::trace_event::EstimateMemoryUsage(cache_) +
          base::trace_event::EstimateMemoryUsage(ntp_entry_));

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

CacheEntry::CacheEntry() = default;

CacheEntry::CacheEntry(const std::string& response_json)
    : response_json(response_json) {}

CacheEntry::CacheEntry(const CacheEntry& entry) = default;

CacheEntry::~CacheEntry() = default;

SearchSuggestionParser::SuggestResults CacheEntry::GetSuggestResults(
    const AutocompleteInput& input,
    const AutocompleteProviderClient& client) const {
  SearchSuggestionParser::Results results;

  auto response_data =
      SearchSuggestionParser::DeserializeJsonData(response_json);
  if (!response_data) {
    return results.suggest_results;
  }

  if (!SearchSuggestionParser::ParseSuggestResults(
          *response_data, input, client.GetSchemeClassifier(),
          /*default_result_relevance=*/100, /*is_keyword_result=*/false,
          &results)) {
    return results.suggest_results;
  }

  return results.suggest_results;
}

size_t CacheEntry::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(response_json);
}
