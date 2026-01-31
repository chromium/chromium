// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_cache_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
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
    PrefService* prefs)
    : scheme_classifier_(std::move(scheme_classifier)), prefs_(prefs) {
  DCHECK(prefs);
}

ZeroSuggestCacheService::~ZeroSuggestCacheService() = default;

CacheEntry ZeroSuggestCacheService::ReadZeroSuggestResponse(
    const std::string& page_url) const {
  return CacheEntry(
      omnibox::GetUserPreferenceForZeroSuggestCachedResponse(prefs_, page_url));
}

void ZeroSuggestCacheService::StoreZeroSuggestResponse(
    const std::string& page_url,
    const std::string& response_json) {
  auto entry = CacheEntry(std::string(response_json));
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(prefs_, page_url,
                                                         response_json);

  for (auto& observer : observers_) {
    observer.OnZeroSuggestResponseUpdated(page_url, entry);
  }
}

void ZeroSuggestCacheService::ClearCache() {
  // Clear user prefs used for cross-session persistence.
  prefs_->SetString(omnibox::kZeroSuggestCachedResults, "");
  prefs_->SetDict(omnibox::kZeroSuggestCachedResultsWithURL, base::DictValue());
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
  std::ranges::transform(
      results.suggest_results, std::back_inserter(suggest_results),
      [](const auto& suggest_result) {
        return CacheEntrySuggestResult{suggest_result.subtypes(),
                                       suggest_result.suggestion()};
      });

  return suggest_results;
}
