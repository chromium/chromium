// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_cache_service.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"

using CacheEntry = ZeroSuggestCacheService::CacheEntry;

ZeroSuggestCacheService::ZeroSuggestCacheService(PrefService* prefs,
                                                 size_t cache_size)
    : prefs_(prefs), cache_(cache_size) {
  DCHECK_GT(cache_size, 0u);

  if (prefs_) {
    if (!base::FeatureList::IsEnabled(omnibox::kZeroSuggestInMemoryCaching)) {
      return;
    }

    // Load cached ZPS response for NTP from prior session via prefs.
    ntp_entry_.response_json =
        omnibox::GetUserPreferenceForZeroSuggestCachedResponse(prefs_,
                                                               /*page_url=*/"");

    // Load cached ZPS responses for SRP/Web from prior session via prefs.
    const auto& prefs_dict =
        prefs->GetDict(omnibox::kZeroSuggestCachedResultsWithURL);
    for (auto it = prefs_dict.begin(); it != prefs_dict.end(); ++it) {
      const auto& page_url = it->first;
      const auto& response_json = (it->second).GetString();
      StoreZeroSuggestResponse(page_url, response_json);
    }
  }
}

ZeroSuggestCacheService::~ZeroSuggestCacheService() {
  if (prefs_) {
    if (!base::FeatureList::IsEnabled(omnibox::kZeroSuggestInMemoryCaching)) {
      return;
    }

    // Dump cached ZPS response for NTP to prefs.
    omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
        prefs_, /*page_url=*/"", /*response=*/ntp_entry_.response_json);

    // Dump cached ZPS responses for SRP/Web to prefs.
    base::Value::Dict prefs_dict;
    for (const auto& item : cache_) {
      const auto& page_url = item.first;
      const auto& response_json = item.second.response_json;
      prefs_dict.Set(page_url, response_json);
    }
    prefs_->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                    std::move(prefs_dict));
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
    const std::string& response_json) {
  auto entry = CacheEntry(response_json);

  if (page_url.empty()) {
    // Write ZPS response for NTP to cache.
    ntp_entry_ = entry;
  } else {
    // Write ZPS response for SRP/Web to cache.
    cache_.Put(page_url, entry);
  }

  base::UmaHistogramCounts1M(
      "Omnibox.ZeroSuggestProvider.CacheMemoryUsage",
      base::trace_event::EstimateMemoryUsage(cache_) +
          base::trace_event::EstimateMemoryUsage(ntp_entry_));

  for (auto& observer : observers_) {
    observer.OnZeroSuggestResponseUpdated(page_url, entry);
  }
}

void ZeroSuggestCacheService::ClearCache() {
  // Clear current contents of in-memory cache.
  ntp_entry_.response_json.clear();
  cache_.Clear();

  // Clear user prefs used for cross-session persistence.
  if (prefs_) {
    if (!base::FeatureList::IsEnabled(omnibox::kZeroSuggestInMemoryCaching)) {
      return;
    }

    prefs_->SetString(omnibox::kZeroSuggestCachedResults, "");
    prefs_->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                    base::Value::Dict());
  }
}

bool ZeroSuggestCacheService::IsCacheEmpty() const {
  return ntp_entry_.response_json.empty() && cache_.empty();
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
  auto response_data =
      SearchSuggestionParser::DeserializeJsonData(response_json);
  if (!response_data) {
    return SearchSuggestionParser::SuggestResults();
  }

  SearchSuggestionParser::Results results;
  if (!SearchSuggestionParser::ParseSuggestResults(
          *response_data, input, client.GetSchemeClassifier(),
          /*default_result_relevance=*/100, /*is_keyword_result=*/false,
          &results)) {
    return SearchSuggestionParser::SuggestResults();
  }

  return results.suggest_results;
}

size_t CacheEntry::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(response_json);
}
