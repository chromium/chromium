// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/image_service_consent_helper.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/page_image_service/metrics_util.h"
#include "components/page_image_service/mojom/page_image_service.mojom-shared.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_image_service {
namespace {

using ::testing::ElementsAre;

class ImageServiceConsentHelperTest : public testing::Test {
 public:
  ImageServiceConsentHelperTest() = default;

  void SetUp() override {
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    consent_helper_ = std::make_unique<ImageServiceConsentHelper>(
        test_sync_service_.get(), syncer::DataType::BOOKMARKS);
  }

  void SetDownloadStatusAndFireNotification(
      syncer::SyncService::DataTypeDownloadStatus download_status) {
    test_sync_service_->SetDownloadStatusFor({syncer::DataType::BOOKMARKS},
                                             download_status);
    test_sync_service_->FireStateChanged();
  }

  PageImageServiceConsentStatus GetResultSynchronously() {
    PageImageServiceConsentStatus out_result;
    consent_helper_->EnqueueRequest(
        base::BindLambdaForTesting(
            [&](PageImageServiceConsentStatus result) { out_result = result; }),
        mojom::ClientId::Bookmarks);
    return out_result;
  }

  void FastForwardBy(base::TimeDelta duration) {
    task_environment_.FastForwardBy(duration);
  }

  ImageServiceConsentHelper* consent_helper() { return consent_helper_.get(); }

  syncer::TestSyncService* sync_service() { return test_sync_service_.get(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<ImageServiceConsentHelper> consent_helper_;
};

TEST_F(ImageServiceConsentHelperTest, EnabledAndDisabledRunSynchronously) {
  SetDownloadStatusAndFireNotification(
      syncer::SyncService::DataTypeDownloadStatus::kError);
  EXPECT_EQ(GetResultSynchronously(), PageImageServiceConsentStatus::kFailure);

  SetDownloadStatusAndFireNotification(
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  EXPECT_EQ(GetResultSynchronously(), PageImageServiceConsentStatus::kSuccess);

  // Set explicit passphrase for Bookmarks to simulate UploadToGoogleState not
  // active.
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kBookmarks});
  sync_service()->SetIsUsingExplicitPassphrase(true);
  EXPECT_EQ(GetResultSynchronously(), PageImageServiceConsentStatus::kFailure);
}

TEST_F(ImageServiceConsentHelperTest, ExpireOldRequests) {
  SetDownloadStatusAndFireNotification(
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);

  std::vector<PageImageServiceConsentStatus> results;
  consent_helper()->EnqueueRequest(
      base::BindLambdaForTesting([&](PageImageServiceConsentStatus result) {
        results.push_back(result);
      }),
      mojom::ClientId::Bookmarks);

  EXPECT_TRUE(results.empty()) << "Callback should not be run immediately.";
  FastForwardBy(base::Seconds(3));
  EXPECT_TRUE(results.empty()) << "Callback should not be run after 3 seconds.";

  // Add another request while the first request is still pending.
  consent_helper()->EnqueueRequest(
      base::BindLambdaForTesting([&](PageImageServiceConsentStatus result) {
        results.push_back(result);
      }),
      mojom::ClientId::Bookmarks);

  FastForwardBy(base::Seconds(10));
  ASSERT_EQ(results.size(), 2U) << "Both callbacks should expire as false.";
  EXPECT_THAT(results, ElementsAre(PageImageServiceConsentStatus::kTimedOut,
                                   PageImageServiceConsentStatus::kTimedOut));

  // Enqueuing another one should restart the timer, which should expire after
  // a second delay of 10 seconds.
  consent_helper()->EnqueueRequest(
      base::BindLambdaForTesting([&](PageImageServiceConsentStatus result) {
        results.push_back(result);
      }),
      mojom::ClientId::Bookmarks);
  EXPECT_EQ(results.size(), 2U) << "Callback should not be run immediately.";
  FastForwardBy(base::Seconds(3));
  EXPECT_EQ(results.size(), 2U);
  FastForwardBy(base::Seconds(10));
  ASSERT_EQ(results.size(), 3U);
  EXPECT_EQ(results[2], PageImageServiceConsentStatus::kTimedOut);

  // Enqueuing another one should restart the timer and shut down sync.
  consent_helper()->EnqueueRequest(
      base::BindLambdaForTesting([&](PageImageServiceConsentStatus result) {
        results.push_back(result);
      }),
      mojom::ClientId::Bookmarks);
  EXPECT_EQ(results.size(), 3U) << "Callback should not be run immediately.";
  consent_helper()->OnSyncShutdown(sync_service());
  FastForwardBy(base::Seconds(11));
  ASSERT_EQ(results.size(), 4U);
  // Sync service shutdown. We do not know consent status.
  EXPECT_EQ(results[3], PageImageServiceConsentStatus::kFailure);
}

TEST_F(ImageServiceConsentHelperTest, InitializationFulfillsAllQueuedRequests) {
  SetDownloadStatusAndFireNotification(
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);

  // Enqueue two requests, 2 seconds apart.
  std::vector<PageImageServiceConsentStatus> results;
  consent_helper()->EnqueueRequest(
      base::BindLambdaForTesting([&](PageImageServiceConsentStatus result) {
        results.push_back(result);
      }),
      mojom::ClientId::Bookmarks);
  ASSERT_TRUE(results.empty());
  FastForwardBy(base::Seconds(2));
  consent_helper()->EnqueueRequest(
      base::BindLambdaForTesting([&](PageImageServiceConsentStatus result) {
        results.push_back(result);
      }),
      mojom::ClientId::Bookmarks);
  ASSERT_TRUE(results.empty()) << "Still nothing should be run yet.";

  SetDownloadStatusAndFireNotification(
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  ASSERT_EQ(results.size(), 2U)
      << "Requests should have been immediately fulfilled as true.";
  EXPECT_THAT(results, ElementsAre(PageImageServiceConsentStatus::kSuccess,
                                   PageImageServiceConsentStatus::kSuccess));
}

TEST_F(ImageServiceConsentHelperTest, InitializationDisabledCase) {
  SetDownloadStatusAndFireNotification(
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);

  std::vector<PageImageServiceConsentStatus> results;
  consent_helper()->EnqueueRequest(
      base::BindLambdaForTesting([&](PageImageServiceConsentStatus result) {
        results.push_back(result);
      }),
      mojom::ClientId::Bookmarks);
  ASSERT_TRUE(results.empty());

  SetDownloadStatusAndFireNotification(
      syncer::SyncService::DataTypeDownloadStatus::kError);
  EXPECT_THAT(results, ElementsAre(PageImageServiceConsentStatus::kFailure));
}

// In production, sometimes the callback to a request enqueues a new request.
// This tests this case and fixes the crash in https://crbug.com/1472360.
TEST_F(ImageServiceConsentHelperTest, CallbacksMakingNewRequests) {
  SetDownloadStatusAndFireNotification(
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);

  std::vector<PageImageServiceConsentStatus> results;

  // These two blocks are identical. The crash is reliably triggered when
  // adding two of these. Probably having two pushes the vector to reallocate
  // while iterating.
  consent_helper()->EnqueueRequest(
      base::BindLambdaForTesting([&](PageImageServiceConsentStatus result) {
        results.push_back(result);
        consent_helper()->EnqueueRequest(
            base::BindLambdaForTesting(
                [&](PageImageServiceConsentStatus result2) {
                  results.push_back(result2);
                }),
            mojom::ClientId::Bookmarks);
      }),
      mojom::ClientId::Bookmarks);
  consent_helper()->EnqueueRequest(
      base::BindLambdaForTesting([&](PageImageServiceConsentStatus result) {
        results.push_back(result);
        consent_helper()->EnqueueRequest(
            base::BindLambdaForTesting(
                [&](PageImageServiceConsentStatus result2) {
                  results.push_back(result2);
                }),
            mojom::ClientId::Bookmarks);
      }),
      mojom::ClientId::Bookmarks);

  // New requests added during iteration live as long as the NEXT timeout.
  FastForwardBy(base::Seconds(12));
  EXPECT_EQ(results.size(), 2U);
  FastForwardBy(base::Seconds(12));
  EXPECT_EQ(results.size(), 4U);
}

}  // namespace
}  // namespace page_image_service
