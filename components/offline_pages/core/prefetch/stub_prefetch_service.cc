// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/stub_prefetch_service.h"

#include "url/gurl.h"

namespace offline_pages {

void StubPrefetchService::SetContentSuggestionsService(
    ntp_snippets::ContentSuggestionsService* content_suggestions) {}

void StubPrefetchService::SetSuggestionProvider(
    SuggestionsProvider* suggestions_provider) {}

void StubPrefetchService::NewSuggestionsAvailable() {}

void StubPrefetchService::RemoveSuggestion(GURL url) {}

void StubPrefetchService::ForceRefreshSuggestions() {}

std::string StubPrefetchService::GetCachedGCMToken() const {
  return "";
}

PrefetchGCMHandler* StubPrefetchService::GetPrefetchGCMHandler() {
  return nullptr;
}

OfflineEventLogger* StubPrefetchService::GetLogger() {
  return nullptr;
}

OfflineMetricsCollector* StubPrefetchService::GetOfflineMetricsCollector() {
  return nullptr;
}

PrefetchDispatcher* StubPrefetchService::GetPrefetchDispatcher() {
  return nullptr;
}

PrefetchNetworkRequestFactory*
StubPrefetchService::GetPrefetchNetworkRequestFactory() {
  return nullptr;
}

PrefetchDownloader* StubPrefetchService::GetPrefetchDownloader() {
  return nullptr;
}

PrefetchStore* StubPrefetchService::GetPrefetchStore() {
  return nullptr;
}

PrefetchImporter* StubPrefetchService::GetPrefetchImporter() {
  return nullptr;
}

PrefetchBackgroundTaskHandler*
StubPrefetchService::GetPrefetchBackgroundTaskHandler() {
  return nullptr;
}

ThumbnailFetcher* StubPrefetchService::GetThumbnailFetcher() {
  return nullptr;
}

OfflinePageModel* StubPrefetchService::GetOfflinePageModel() {
  return nullptr;
}

image_fetcher::ImageFetcher* StubPrefetchService::GetImageFetcher() {
  return nullptr;
}

SuggestedArticlesObserver*
StubPrefetchService::GetSuggestedArticlesObserverForTesting() {
  return nullptr;
}

void StubPrefetchService::SetEnabledByServer(PrefService* pref_service,
                                             bool enabled) {}

}  // namespace offline_pages
