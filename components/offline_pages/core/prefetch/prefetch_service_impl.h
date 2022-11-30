// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

class PrefetchServiceImpl : public PrefetchService {
 public:
  PrefetchServiceImpl(
      std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector,
      std::unique_ptr<PrefetchDispatcher> dispatcher,
      std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory,
      OfflinePageModel* offline_page_model,
      std::unique_ptr<PrefetchStore> prefetch_store,
      std::unique_ptr<PrefetchDownloader> prefetch_downloader,
      std::unique_ptr<PrefetchImporter> prefetch_importer,
      std::unique_ptr<PrefetchGCMHandler> gcm_handler,
      std::unique_ptr<PrefetchBackgroundTaskHandler> background_task_handler,
      image_fetcher::ImageFetcher* image_fetcher_,
      PrefService* prefs);

  PrefetchServiceImpl(const PrefetchServiceImpl&) = delete;
  PrefetchServiceImpl& operator=(const PrefetchServiceImpl&) = delete;

  ~PrefetchServiceImpl() override;

  // PrefetchService implementation:
  // Externally used functions.
  void SetSuggestionProvider(
      SuggestionsProvider* suggestions_provider) override;
  void NewSuggestionsAvailable() override;
  void RemoveSuggestion(GURL url) override;
  PrefetchGCMHandler* GetPrefetchGCMHandler() override;
  std::string GetCachedGCMToken() const override;
  void SetEnabledByServer(PrefService* pref_service, bool enabled) override;

  // Internal usage only functions.
  void ForceRefreshSuggestions() override;
  OfflineMetricsCollector* GetOfflineMetricsCollector() override;
  PrefetchDispatcher* GetPrefetchDispatcher() override;
  PrefetchNetworkRequestFactory* GetPrefetchNetworkRequestFactory() override;
  OfflinePageModel* GetOfflinePageModel() override;
  PrefetchStore* GetPrefetchStore() override;
  OfflineEventLogger* GetLogger() override;
  PrefetchDownloader* GetPrefetchDownloader() override;
  PrefetchImporter* GetPrefetchImporter() override;
  PrefetchBackgroundTaskHandler* GetPrefetchBackgroundTaskHandler() override;

  void GCMTokenReceived(const std::string& gcm_token,
                        instance_id::InstanceID::Result result);

  image_fetcher::ImageFetcher* GetImageFetcher() override;

  base::WeakPtr<PrefetchServiceImpl> GetWeakPtr();

  // KeyedService implementation:
  void Shutdown() override;

  // Replaces the ImageFetcher. The ReducedModeImageFetcher is used when the
  // PrefetchService is created, and will be replaced with CachedImageFetcher
  // when Chrome is launched from reduced mode to full browser mode.
  void ReplaceImageFetcher(image_fetcher::ImageFetcher* image_fetcher);

 private:

  OfflineEventLogger logger_;

  std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector_;
  std::unique_ptr<PrefetchDispatcher> prefetch_dispatcher_;
  std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory_;
  raw_ptr<OfflinePageModel> offline_page_model_;
  std::unique_ptr<PrefetchStore> prefetch_store_;
  std::unique_ptr<PrefetchDownloader> prefetch_downloader_;
  std::unique_ptr<PrefetchImporter> prefetch_importer_;
  std::unique_ptr<PrefetchGCMHandler> prefetch_gcm_handler_;
  std::unique_ptr<PrefetchBackgroundTaskHandler>
      prefetch_background_task_handler_;
  raw_ptr<PrefService> prefs_;

  // Owned by CachedImageFetcherService.
  raw_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  raw_ptr<SuggestionsProvider> suggestions_provider_ = nullptr;

  base::WeakPtrFactory<PrefetchServiceImpl> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
