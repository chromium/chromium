// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_TEST_TACO_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_TEST_TACO_H_

#include <memory>

#include "base/check.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"

namespace image_fetcher {
class ImageFetcher;
}

namespace offline_pages {
class OfflineMetricsCollector;
class OfflinePageModel;
class PrefetchBackgroundTaskHandler;
class PrefetchDispatcher;
class PrefetchDownloader;
class PrefetchGCMHandler;
class PrefetchImporter;
class PrefetchNetworkRequestFactory;
class PrefetchService;
class PrefetchStore;
class TestDownloadClient;
class TestDownloadService;

// The taco class acts as a wrapper around the prefetch service making
// it easy to create for tests, using test versions of the dependencies.
// This class is like a taco shell, and the filling is the prefetch service.
// The default dependencies may be replaced by the test author to provide
// custom versions that have test-specific hooks.
class PrefetchServiceTestTaco {
 public:
  explicit PrefetchServiceTestTaco();
  ~PrefetchServiceTestTaco();

  // These methods must be called before CreatePrefetchService() is invoked.
  // If called after they will CHECK().
  //
  // Default type: TestOfflineMetricsCollector.
  void SetOfflineMetricsCollector(
      std::unique_ptr<OfflineMetricsCollector> metrics_collector);
  // Default type: TestPrefetchDispatcher.
  void SetPrefetchDispatcher(std::unique_ptr<PrefetchDispatcher> dispatcher);
  // Default type: TestPrefetchGCMHandler.
  void SetPrefetchGCMHandler(std::unique_ptr<PrefetchGCMHandler> gcm_handler);
  // Default type: TestNetworkRequestFactory.
  void SetPrefetchNetworkRequestFactory(
      std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory);
  void SetPrefetchStore(std::unique_ptr<PrefetchStore> prefetch_store_sql);
  void SetPrefetchDownloader(
      std::unique_ptr<PrefetchDownloader> prefetch_downloader);
  void SetPrefetchImporter(std::unique_ptr<PrefetchImporter> prefetch_importer);
  void SetPrefetchBackgroundTaskHandler(
      std::unique_ptr<PrefetchBackgroundTaskHandler>
          prefetch_background_task_handler);
  // Default type: image_fetcher::MockImageFetcher.
  void SetThumbnailImageFetcher(
      std::unique_ptr<image_fetcher::ImageFetcher> thumbnail_image_fetcher);
  void SetOfflinePageModel(
      std::unique_ptr<OfflinePageModel> offline_page_model);

  // Default type: TestingPrefServiceSimple.
  // When updating the PrefService, it's up to the caller to make sure that the
  // PrefetchNetworkRequestFactory and PrefetchDownloader were created with the
  // same PrefService.
  void SetPrefService(std::unique_ptr<PrefService> prefs);

  // Creates and caches an instance of PrefetchService, using default or
  // overridden test dependencies.
  void CreatePrefetchService();

  // Once CreatePrefetchService() is called, this accessor method starts
  // returning the PrefetchService.
  PrefetchService* prefetch_service() const {
    CHECK(prefetch_service_);
    return prefetch_service_.get();
  }

  TestDownloadService* download_service() { return download_service_.get(); }

  // Creates and returns the ownership of the created PrefetchService instance.
  // Leaves the taco empty, not usable.
  std::unique_ptr<PrefetchService> CreateAndReturnPrefetchService();

  PrefService* pref_service() const { return pref_service_.get(); }
  PrefetchNetworkRequestFactory* network_request_factory() const {
    return network_request_factory_.get();
  }

 private:
  std::unique_ptr<OfflineMetricsCollector> metrics_collector_;
  std::unique_ptr<PrefetchDispatcher> dispatcher_;
  std::unique_ptr<PrefetchGCMHandler> gcm_handler_;
  std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory_;
  std::unique_ptr<PrefetchStore> prefetch_store_;
  std::unique_ptr<PrefetchDownloader> prefetch_downloader_;
  std::unique_ptr<PrefetchImporter> prefetch_importer_;
  std::unique_ptr<PrefetchBackgroundTaskHandler>
      prefetch_background_task_handler_;
  std::unique_ptr<PrefetchService> prefetch_service_;
  std::unique_ptr<image_fetcher::ImageFetcher> thumbnail_image_fetcher_;
  std::unique_ptr<OfflinePageModel> offline_page_model_;
  std::unique_ptr<TestDownloadService> download_service_;
  std::unique_ptr<TestDownloadClient> download_client_;
  std::unique_ptr<PrefService> pref_service_;
};

}  // namespace offline_pages
#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_TEST_TACO_H_
