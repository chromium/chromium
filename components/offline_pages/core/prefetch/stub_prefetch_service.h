// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STUB_PREFETCH_SERVICE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STUB_PREFETCH_SERVICE_H_

#include <string>

#include "components/offline_pages/core/prefetch/prefetch_service.h"

namespace offline_pages {

// Stub implementation of PrefetchService interface for testing.
class StubPrefetchService : public PrefetchService {
 public:
  void SetContentSuggestionsService(
      ntp_snippets::ContentSuggestionsService* content_suggestions) override;
  void SetSuggestionProvider(
      SuggestionsProvider* suggestions_provider) override;
  void NewSuggestionsAvailable() override;
  void RemoveSuggestion(GURL url) override;
  std::string GetCachedGCMToken() const override;
  void ForceRefreshSuggestions() override;
  PrefetchGCMHandler* GetPrefetchGCMHandler() override;
  OfflineEventLogger* GetLogger() override;
  OfflineMetricsCollector* GetOfflineMetricsCollector() override;
  PrefetchDispatcher* GetPrefetchDispatcher() override;
  PrefetchNetworkRequestFactory* GetPrefetchNetworkRequestFactory() override;
  PrefetchDownloader* GetPrefetchDownloader() override;
  PrefetchStore* GetPrefetchStore() override;
  PrefetchImporter* GetPrefetchImporter() override;
  PrefetchBackgroundTaskHandler* GetPrefetchBackgroundTaskHandler() override;
  ThumbnailFetcher* GetThumbnailFetcher() override;
  OfflinePageModel* GetOfflinePageModel() override;
  image_fetcher::ImageFetcher* GetImageFetcher() override;
  void SetEnabledByServer(PrefService* pref_service, bool enabled) override;

  SuggestedArticlesObserver* GetSuggestedArticlesObserverForTesting() override;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STUB_PREFETCH_SERVICE_H_
