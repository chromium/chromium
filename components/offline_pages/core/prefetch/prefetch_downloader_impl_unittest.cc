// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_downloader_impl.h"

#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/download/public/background_service/test/test_download_service.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"
#include "components/offline_pages/core/prefetch/prefetch_server_urls.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"
#include "components/offline_pages/core/prefetch/test_download_client.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const version_info::Channel kTestChannel = version_info::Channel::UNKNOWN;
const char kDownloadId[] = "1234Ab";
const char kDownloadId2[] = "Abcd";
const char kFailedDownloadId[] = "f1f1FF";
const char kDownloadLocation[] = "page/1";
const char kDownloadLocation2[] = "page/zz";
const char kOperationName[] = "operation-123";
const char kServerPathForDownload[] = "/v1/media/page/1";
}  // namespace

namespace offline_pages {

class PrefetchDownloaderImplTest : public PrefetchRequestTestBase {
 public:
  PrefetchDownloaderImplTest() = default;

  void SetUp() override {
    PrefetchRequestTestBase::SetUp();

    prefetch_service_taco_.reset(new PrefetchServiceTestTaco);

    auto downloader = std::make_unique<PrefetchDownloaderImpl>(
        &download_service_, kTestChannel,
        prefetch_service_taco_->pref_service());
    download_service_.SetFailedDownload(kFailedDownloadId, false);
    download_service_.SetIsReady(true);
    download_client_ = std::make_unique<TestDownloadClient>(downloader.get());
    download_service_.set_client(download_client_.get());
    prefetch_service_taco_->SetPrefetchDownloader(std::move(downloader));
    prefetch_service_taco_->CreatePrefetchService();
  }

  void OnDownloadServiceReady() {
    prefetch_downloader()->OnDownloadServiceReady(
        std::set<std::string>(),
        std::map<std::string, std::pair<base::FilePath, int64_t>>());
  }

  void TearDown() override {
    prefetch_service_taco_.reset();
    PrefetchRequestTestBase::TearDown();
    FastForwardUntilNoTasksRemain();
  }

  void StartDownload(const std::string& download_id,
                     const std::string& download_location,
                     const std::string& operation_name) {
    prefetch_downloader()->StartDownload(download_id, download_location,
                                         operation_name);
  }

  base::Optional<download::DownloadParams> GetDownload(
      const std::string& guid) const {
    return download_service_.GetDownload(guid);
  }

  const std::vector<PrefetchDownloadResult>& completed_downloads() const {
    return prefetch_dispatcher()->download_results;
  }

  TestScopedOfflineClock* clock() { return &clock_; }

  PrefetchDownloader* prefetch_downloader() const {
    return prefetch_service_taco_->prefetch_service()->GetPrefetchDownloader();
  }

  TestPrefetchDispatcher* prefetch_dispatcher() const {
    return static_cast<TestPrefetchDispatcher*>(
        prefetch_service_taco_->prefetch_service()->GetPrefetchDispatcher());
  }

 private:
  download::test::TestDownloadService download_service_;
  std::unique_ptr<TestDownloadClient> download_client_;
  std::unique_ptr<PrefetchServiceTestTaco> prefetch_service_taco_;
  TestScopedOfflineClock clock_;
};

TEST_F(PrefetchDownloaderImplTest, DownloadParams) {
  OnDownloadServiceReady();
  base::Time epoch = base::Time();
  clock()->SetNow(epoch);

  StartDownload(kDownloadId, kDownloadLocation, kOperationName);
  base::Optional<download::DownloadParams> params = GetDownload(kDownloadId);
  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(kDownloadId, params->guid);
  EXPECT_EQ(download::DownloadClient::OFFLINE_PAGE_PREFETCH, params->client);
  GURL download_url = params->request_params.url;
  EXPECT_TRUE(download_url.SchemeIs(url::kHttpsScheme));
  EXPECT_FALSE(params->request_params.require_safety_checks);
  EXPECT_EQ(kServerPathForDownload, download_url.path());
  std::string key_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(download_url, "key", &key_value));
  EXPECT_FALSE(key_value.empty());
  std::string alt_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(download_url, "alt", &alt_value));
  EXPECT_EQ("media", alt_value);
  EXPECT_EQ(base::StrCat({kPrefetchOperationHeaderName, ": ", kOperationName,
                          "\r\n\r\n"}),
            params->request_params.request_headers.ToString());

  EXPECT_EQ(base::TimeDelta::FromDays(2),
            params->scheduling_params.cancel_time - epoch);
  RunUntilIdle();
}

TEST_F(PrefetchDownloaderImplTest, ExperimentHeaderInDownloadParams) {
  OnDownloadServiceReady();
  SetUpExperimentOption();

  StartDownload(kDownloadId, kDownloadLocation, kOperationName);
  base::Optional<download::DownloadParams> params = GetDownload(kDownloadId);
  ASSERT_TRUE(params.has_value());
  std::string header_value;
  EXPECT_TRUE(params->request_params.request_headers.GetHeader(
      kPrefetchExperimentHeaderName, &header_value));
  EXPECT_EQ(kExperimentValueSetInFieldTrial, header_value);
  RunUntilIdle();
}

TEST_F(PrefetchDownloaderImplTest, DownloadSucceeded) {
  OnDownloadServiceReady();
  StartDownload(kDownloadId, kDownloadLocation, kOperationName);
  StartDownload(kDownloadId2, kDownloadLocation2, kOperationName);
  RunUntilIdle();
  ASSERT_EQ(2u, completed_downloads().size());
  EXPECT_EQ(kDownloadId, completed_downloads()[0].download_id);
  EXPECT_TRUE(completed_downloads()[0].success);
  EXPECT_EQ(kDownloadId2, completed_downloads()[1].download_id);
  EXPECT_TRUE(completed_downloads()[1].success);
}

TEST_F(PrefetchDownloaderImplTest, DownloadFailed) {
  OnDownloadServiceReady();
  StartDownload(kFailedDownloadId, kDownloadLocation, kOperationName);
  RunUntilIdle();
  ASSERT_EQ(1u, completed_downloads().size());
  EXPECT_EQ(kFailedDownloadId, completed_downloads()[0].download_id);
  EXPECT_FALSE(completed_downloads()[0].success);
}

TEST_F(PrefetchDownloaderImplTest, DoNotCleanupTwiceIfServiceStartsFirst) {
  OnDownloadServiceReady();
  EXPECT_EQ(0, prefetch_dispatcher()->cleanup_downloads_count);

  prefetch_downloader()->CleanupDownloadsWhenReady();
  EXPECT_EQ(1, prefetch_dispatcher()->cleanup_downloads_count);

  // We should not cleanup again.
  prefetch_downloader()->CleanupDownloadsWhenReady();
  EXPECT_EQ(1, prefetch_dispatcher()->cleanup_downloads_count);
}

TEST_F(PrefetchDownloaderImplTest, DoNotCleanupTwiceIfDispatcherStartsFirst) {
  prefetch_downloader()->CleanupDownloadsWhenReady();
  EXPECT_EQ(0, prefetch_dispatcher()->cleanup_downloads_count);
  // One unknown download is sent to the downloader when the service is ready.
  std::set<std::string> download_ids_before = {kDownloadId2};
  prefetch_downloader()->OnDownloadServiceReady(download_ids_before, {});
  EXPECT_EQ(1, prefetch_dispatcher()->cleanup_downloads_count);

  // We should not cleanup again.
  prefetch_downloader()->CleanupDownloadsWhenReady();
  EXPECT_EQ(1, prefetch_dispatcher()->cleanup_downloads_count);
}

}  // namespace offline_pages
