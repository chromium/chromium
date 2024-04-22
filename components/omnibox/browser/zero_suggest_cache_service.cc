// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_cache_service.h"

#include <iterator>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"

using CacheEntry = ZeroSuggestCacheServiceInterface::CacheEntry;
using CacheEntrySuggestResult =
    ZeroSuggestCacheServiceInterface::CacheEntrySuggestResult;

ZeroSuggestCacheService::ZeroSuggestCacheService(
    std::unique_ptr<AutocompleteSchemeClassifier> scheme_classifier,
    PrefService* prefs,
    size_t cache_size)
    : scheme_classifier_(std::move(scheme_classifier)),
      prefs_(prefs),
      cache_(cache_size) {
  DCHECK(prefs);
  DCHECK_GT(cache_size, 0u);

  if (!base::FeatureList::IsEnabled(omnibox::kZeroSuggestInMemoryCaching)) {
    return;
  }

  // Load cached ZPS response for NTP from prior session via prefs, if any.
  StoreZeroSuggestResponse(
      /*page_url=*/"", prefs->GetString(omnibox::kZeroSuggestCachedResults));

  // Load cached ZPS responses for SRP/Web from prior session via prefs, if any.
  const auto& prefs_dict =
      prefs->GetDict(omnibox::kZeroSuggestCachedResultsWithURL);
  for (auto it = prefs_dict.begin(); it != prefs_dict.end(); ++it) {
    const auto& page_url = it->first;
    const auto* response_json_ptr = (it->second).GetIfString();
    if (response_json_ptr) {
      StoreZeroSuggestResponse(page_url, *response_json_ptr);
    }
  }
}

ZeroSuggestCacheService::~ZeroSuggestCacheService() {
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

CacheEntry ZeroSuggestCacheService::ReadZeroSuggestResponse(
    const std::string& page_url) const {
  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestInMemoryCaching)) {
    // Read cached ZPS response for NTP.
    if (page_url.empty()) {
      return ntp_entry_;
    }

    // Read cached ZPS response for SRP/Web.
    const auto it = cache_.Get(page_url);
    return it != cache_.end() ? it->second : CacheEntry();
  }

  return CacheEntry(
      omnibox::GetUserPreferenceForZeroSuggestCachedResponse(prefs_, page_url));
}

void ZeroSuggestCacheService::StoreZeroSuggestResponse(
    const std::string& page_url,
    const std::string& response_json) {
  auto entry = CacheEntry(response_json);
  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestInMemoryCaching)) {
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
  } else {
    omnibox::SetUserPreferenceForZeroSuggestCachedResponse(prefs_, page_url,
                                                           response_json);
  }

  for (auto& observer : observers_) {
    observer.OnZeroSuggestResponseUpdated(page_url, entry);
  }
}

void ZeroSuggestCacheService::ClearCache() {
  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestInMemoryCaching)) {
    // Clear current contents of in-memory cache.
    ntp_entry_.response_json.clear();
    cache_.Clear();
  }

  // Clear user prefs used for cross-session persistence.
  prefs_->SetString(omnibox::kZeroSuggestCachedResults, "");
  prefs_->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                  base::Value::Dict());
}

bool ZeroSuggestCacheService::IsInMemoryCacheEmptyForTesting() const {
  return ntp_entry_.response_json.empty() && cache_.empty();
}

void ZeroSuggestCacheService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ZeroSuggestCacheService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<CacheEntrySuggestResult> ZeroSuggestCacheService::GetSuggestResults(
    const CacheEntry& cache_entry) const {
  auto response_data =
      SearchSuggestionParser::DeserializeJsonData(cache_entry.response_json);
  if (!response_data) {
    return {};
  }

  AutocompleteInput input(u"", metrics::OmniboxEventProto::INVALID_SPEC,
                          *scheme_classifier_);
  SearchSuggestionParser::Results results;
  if (!SearchSuggestionParser::ParseSuggestResults(
          *response_data, input, *scheme_classifier_,
          /*default_result_relevance=*/100, /*is_keyword_result=*/false,
          &results)) {
    return {};
  }

  std::vector<CacheEntrySuggestResult> suggest_results;
  suggest_results.reserve(results.suggest_results.size());
  base::ranges::transform(
      results.suggest_results, std::back_inserter(suggest_results),
      [](const auto& suggest_result) {
        return CacheEntrySuggestResult{suggest_result.subtypes(),
                                       suggest_result.suggestion()};
      });

  return suggest_results;
}
