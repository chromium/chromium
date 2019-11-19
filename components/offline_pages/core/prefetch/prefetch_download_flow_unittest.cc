// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_downloader_impl.h"

#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/download/public/background_service/test/test_download_service.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_download_client.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
const version_info::Channel kTestChannel = version_info::Channel::UNKNOWN;
const base::FilePath kTestFilePath(FILE_PATH_LITERAL("foo"));
const int64_t kTestFileSize = 88888;

// Tests the interaction between prefetch service and download service to
// validate the whole prefetch download flow regardless which service is up
// first.
class PrefetchDownloadFlowTest : public PrefetchTaskTestBase {
 public:
  PrefetchDownloadFlowTest() {
    feature_list_.InitAndEnableFeature(kPrefetchingOfflinePagesFeature);
  }

  void SetUp() override {
    PrefetchTaskTestBase::SetUp();

    prefetch_service_taco_.reset(new PrefetchServiceTestTaco);
    prefetch_service_taco_->SetPrefService(std::move(prefs_));
    prefetch_prefs::SetEnabledByServer(prefetch_service_taco_->pref_service(),
                                       true);
    prefetch_prefs::SetCachedPrefetchGCMToken(
        prefetch_service_taco_->pref_service(), "dummy_gcm_token");

    auto downloader = std::make_unique<PrefetchDownloaderImpl>(
        &download_service_, kTestChannel,
        prefetch_service_taco_->pref_service());
    download_client_ = std::make_unique<TestDownloadClient>(downloader.get());
    download_service_.set_client(download_client_.get());
    prefetch_service_taco_->SetPrefetchDispatcher(
        std::make_unique<PrefetchDispatcherImpl>(
            prefetch_service_taco_->pref_service()));
    prefetch_service_taco_->SetPrefetchStore(store_util()->ReleaseStore());
    prefetch_service_taco_->SetPrefetchDownloader(std::move(downloader));
    prefetch_service_taco_->CreatePrefetchService();
    item_generator()->set_client_namespace(kSuggestedArticlesNamespace);
  }

  void TearDown() override {
    prefetch_service_taco_.reset();
    PrefetchTaskTestBase::TearDown();
  }

  void SetDownloadServiceReady() {
    SetDownloadServiceReadyWithParams(
        std::set<std::string>(),
        std::map<std::string, std::pair<base::FilePath, int64_t>>());
  }

  void SetDownloadServiceReadyWithParams(
      const std::set<std::string>& outstanding_download_ids,
      const std::map<std::string, std::pair<base::FilePath, int64_t>>&
          success_downloads) {
    download_service_.SetIsReady(true);
    prefetch_downloader()->OnDownloadServiceReady(
        std::set<std::string>(),
        std::map<std::string, std::pair<base::FilePath, int64_t>>());
    RunUntilIdle();
  }

  void BeginBackgroundTask() {
    prefetch_dispatcher()->BeginBackgroundTask(
        std::make_unique<PrefetchBackgroundTask>(
            prefetch_service_taco_->prefetch_service()));
    RunUntilIdle();
  }

  PrefetchDispatcher* prefetch_dispatcher() const {
    return prefetch_service_taco_->prefetch_service()->GetPrefetchDispatcher();
  }

  PrefetchDownloader* prefetch_downloader() const {
    return prefetch_service_taco_->prefetch_service()->GetPrefetchDownloader();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  download::test::TestDownloadService download_service_;
  std::unique_ptr<TestDownloadClient> download_client_;
  std::unique_ptr<PrefetchServiceTestTaco> prefetch_service_taco_;
};

TEST_F(PrefetchDownloadFlowTest, DownloadServiceReadyAfterPrefetchSystemReady) {
  // Create an item ready for download.
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::RECEIVED_BUNDLE);
  item.archive_body_length = 100;
  store_util()->InsertPrefetchItem(item);
  RunUntilIdle();

  // Start the prefetch processing pipeline.
  BeginBackgroundTask();

  // The item can still been scheduled for download though the download service
  // is not ready.
  std::unique_ptr<PrefetchItem> found_item =
      store_util()->GetPrefetchItem(item.offline_id);
  EXPECT_EQ(PrefetchItemState::DOWNLOADING, found_item->state);

  // Now make the download service ready.
  SetDownloadServiceReady();
  RunUntilIdle();

  // The item should finally transit to IMPORTING state.
  found_item = store_util()->GetPrefetchItem(item.offline_id);
  EXPECT_EQ(PrefetchItemState::IMPORTING, found_item->state);
}

TEST_F(PrefetchDownloadFlowTest,
       DownloadServiceReadyBeforePrefetchSystemReady) {
  // Download service is ready initially.
  SetDownloadServiceReady();
  RunUntilIdle();

  // Create an item ready for download.
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::RECEIVED_BUNDLE);
  item.archive_body_length = 100;
  store_util()->InsertPrefetchItem(item);
  RunUntilIdle();

  // Start the prefetch processing pipeline.
  BeginBackgroundTask();

  // The item should finally transit to IMPORTING state.
  std::unique_ptr<PrefetchItem> found_item =
      store_util()->GetPrefetchItem(item.offline_id);
  EXPECT_EQ(PrefetchItemState::IMPORTING, found_item->state);
}

TEST_F(PrefetchDownloadFlowTest, DownloadServiceUnavailable) {
  // Download service is unavailable.
  EXPECT_FALSE(prefetch_downloader()->IsDownloadServiceUnavailable());
  prefetch_downloader()->OnDownloadServiceUnavailable();
  EXPECT_TRUE(prefetch_downloader()->IsDownloadServiceUnavailable());

  // Create an item ready for download.
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::RECEIVED_BUNDLE);
  item.archive_body_length = 100;
  store_util()->InsertPrefetchItem(item);
  RunUntilIdle();

  // Start the prefetch processing pipeline.
  BeginBackgroundTask();

  // The item should not be changed since download service can't be used.
  std::unique_ptr<PrefetchItem> found_item =
      store_util()->GetPrefetchItem(item.offline_id);
  EXPECT_EQ(item, *found_item);
}

TEST_F(PrefetchDownloadFlowTest, DelayRunningDownloadCleanupTask) {
  // Create an item in DOWNLOADING state.
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::DOWNLOADING);
  item.archive_body_length = 100;
  store_util()->InsertPrefetchItem(item);
  RunUntilIdle();

  // Download service is ready.
  std::set<std::string> outgoing_downloads;
  std::map<std::string, std::pair<base::FilePath, int64_t>> success_downloads;
  success_downloads.emplace(item.guid,
                            std::make_pair(kTestFilePath, kTestFileSize));
  SetDownloadServiceReadyWithParams(outgoing_downloads, success_downloads);

  // The item should not be changed because the prefetch processing pipeline has
  // not started yet and the download cleanup task will not be created.
  std::unique_ptr<PrefetchItem> found_item =
      store_util()->GetPrefetchItem(item.offline_id);
  EXPECT_EQ(item, *found_item);

  // Start the prefetch processing pipeline.
  BeginBackgroundTask();

  // The download cleanup task should be created and run. The item should
  // finally transit to IMPORTING state.
  found_item = store_util()->GetPrefetchItem(item.offline_id);
  EXPECT_EQ(PrefetchItemState::IMPORTING, found_item->state);
}

}  // namespace
}  // namespace offline_pages
