// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/image_service_consent_helper.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/page_image_service/features.h"
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
  ImageServiceConsentHelperTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kImageServiceObserveSyncDownloadStatus);
  }

  void SetUp() override {
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    consent_helper_ = std::make_unique<ImageServiceConsentHelper>(
        test_sync_service_.get(), syncer::ModelType::HISTORY_DELETE_DIRECTIVES);
  }

  void SetDownloadStatusAndFireNotification(
      syncer::SyncService::ModelTypeDownloadStatus download_status) {
    test_sync_service_->SetDownloadStatusFor(
        {syncer::ModelType::HISTORY_DELETE_DIRECTIVES}, download_status);
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

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<ImageServiceConsentHelper> consent_helper_;
};

TEST_F(ImageServiceConsentHelperTest, EnabledAndDisabledRunSynchronously) {
  SetDownloadStatusAndFireNotification(
      syncer::SyncService::ModelTypeDownloadStatus::kError);
  EXPECT_EQ(GetResultSynchronously(), PageImageServiceConsentStatus::kFailure);

  SetDownloadStatusAndFireNotification(
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  EXPECT_EQ(GetResultSynchronously(), PageImageServiceConsentStatus::kSuccess);
}

TEST_F(ImageServiceConsentHelperTest, ExpireOldRequests) {
  SetDownloadStatusAndFireNotification(
      syncer::SyncService::ModelTypeDownloadStatus::kWaitingForUpdates);

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
}

TEST_F(ImageServiceConsentHelperTest, InitializationFulfillsAllQueuedRequests) {
  SetDownloadStatusAndFireNotification(
      syncer::SyncService::ModelTypeDownloadStatus::kWaitingForUpdates);

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
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  ASSERT_EQ(results.size(), 2U)
      << "Requests should have been immediately fulfilled as true.";
  EXPECT_THAT(results, ElementsAre(PageImageServiceConsentStatus::kSuccess,
                                   PageImageServiceConsentStatus::kSuccess));
}

TEST_F(ImageServiceConsentHelperTest, InitializationDisabledCase) {
  SetDownloadStatusAndFireNotification(
      syncer::SyncService::ModelTypeDownloadStatus::kWaitingForUpdates);

  std::vector<PageImageServiceConsentStatus> results;
  consent_helper()->EnqueueRequest(
      base::BindLambdaForTesting([&](PageImageServiceConsentStatus result) {
        results.push_back(result);
      }),
      mojom::ClientId::Bookmarks);
  ASSERT_TRUE(results.empty());

  SetDownloadStatusAndFireNotification(
      syncer::SyncService::ModelTypeDownloadStatus::kError);
  EXPECT_THAT(results, ElementsAre(PageImageServiceConsentStatus::kFailure));
}

class ImageServiceConsentHelperDownloadStatusKillSwitchTest
    : public ImageServiceConsentHelperTest {
 public:
  ImageServiceConsentHelperDownloadStatusKillSwitchTest() {
    scoped_feature_list_.InitAndDisableFeature(
        kImageServiceObserveSyncDownloadStatus);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ImageServiceConsentHelperDownloadStatusKillSwitchTest,
       SyncStatusNotObserved) {
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kHistory});
  sync_service()->FireStateChanged();
  EXPECT_EQ(GetResultSynchronously(), PageImageServiceConsentStatus::kSuccess);

  // Error notification would normally be false but we shouldn't be listening to
  // the download status.
  SetDownloadStatusAndFireNotification(
      syncer::SyncService::ModelTypeDownloadStatus::kError);
  EXPECT_EQ(GetResultSynchronously(), PageImageServiceConsentStatus::kSuccess);
}

}  // namespace
}  // namespace page_image_service
