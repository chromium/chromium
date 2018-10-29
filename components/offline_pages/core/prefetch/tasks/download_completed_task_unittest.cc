// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/download_completed_task.h"

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
const int64_t kTestOfflineID = 1111;
const int64_t kTestOfflineID2 = 223344;
const char kTestGUID[] = "1a150628-1b56-44da-a85a-c575120af180";
const char kTestGUID2[] = "736edb12-98f6-41c2-8e50-a667694511a5";
const int64_t kTestFileSize = 88888;
const char kClientID1[] = "client-id-1";
const char kClientID2[] = "client-id-2";

base::FilePath TestFilePath() {
  return base::FilePath(FILE_PATH_LITERAL("foo"));
}

class DownloadCompletedTaskTest : public PrefetchTaskTestBase {
 public:
  ~DownloadCompletedTaskTest() override = default;

  void SetUp() override;

 protected:
  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  std::unique_ptr<DownloadCompletedTask> CreateTask(
      PrefetchDownloadResult download_result) {
    return std::make_unique<DownloadCompletedTask>(dispatcher(), store(),
                                                   download_result);
  }
  // Expect that dispatcher()->ItemDownloaded was called for each entry in
  // item_downloaded_results.
  void ExpectItemDownloaded(
      std::vector<std::pair<int64_t, ClientId>> item_downloaded_results) {
    EXPECT_EQ(item_downloaded_results, dispatcher_.item_downloaded_results);
  }

 private:
  TestPrefetchDispatcher dispatcher_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

void DownloadCompletedTaskTest::SetUp() {
  PrefetchTaskTestBase::SetUp();
  PrefetchItem item;
  item.offline_id = kTestOfflineID;
  item.guid = kTestGUID;
  item.state = PrefetchItemState::DOWNLOADING;
  item.creation_time = base::Time::Now();
  item.freshness_time = item.creation_time;
  item.client_id.name_space = kSuggestedArticlesNamespace;
  item.client_id.id = kClientID1;
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item));

  PrefetchItem item2;
  item2.offline_id = kTestOfflineID2;
  item2.guid = kTestGUID2;
  item2.state = PrefetchItemState::NEW_REQUEST;
  item2.creation_time = base::Time::Now();
  item2.freshness_time = item.creation_time;
  item.client_id.name_space = kSuggestedArticlesNamespace;
  item.client_id.id = kClientID2;
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item2));

  histogram_tester_.reset(new base::HistogramTester());
}

TEST_F(DownloadCompletedTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  PrefetchDownloadResult download_result(kTestGUID, TestFilePath(),
                                         kTestFileSize);
  RunTask(CreateTask(download_result));
  std::vector<std::pair<int64_t, ClientId>> want;
  ExpectItemDownloaded({});
}

TEST_F(DownloadCompletedTaskTest, UpdateItemOnDownloadSuccess) {
  PrefetchDownloadResult download_result(kTestGUID, TestFilePath(),
                                         kTestFileSize);
  RunTask(CreateTask(download_result));

  std::unique_ptr<PrefetchItem> item =
      store_util()->GetPrefetchItem(kTestOfflineID);
  EXPECT_EQ(PrefetchItemState::DOWNLOADED, item->state);
  EXPECT_EQ(kTestGUID, item->guid);
  EXPECT_EQ(TestFilePath(), item->file_path);
  EXPECT_EQ(kTestFileSize, item->file_size);
  EXPECT_EQ(1, dispatcher()->processing_schedule_count);
  ExpectItemDownloaded(
      {{kTestOfflineID, ClientId(kSuggestedArticlesNamespace, kClientID1)}});

  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.Prefetching.DownloadedFileSize", kTestFileSize / 1024, 1);
  // Bucket 0 reflects DownloadOutcome::DOWNLOAD_SUCCEEDED_ITEM_UPDATED.
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.Prefetching.DownloadFinishedUpdate", 0, 1);
}

TEST_F(DownloadCompletedTaskTest, UpdateItemOnDownloadError) {
  PrefetchDownloadResult download_result;
  download_result.download_id = kTestGUID;
  download_result.success = false;
  RunTask(CreateTask(download_result));

  std::unique_ptr<PrefetchItem> item =
      store_util()->GetPrefetchItem(kTestOfflineID);
  EXPECT_EQ(PrefetchItemState::FINISHED, item->state);
  EXPECT_EQ(PrefetchItemErrorCode::DOWNLOAD_ERROR, item->error_code);
  EXPECT_EQ(kTestGUID, item->guid);
  EXPECT_TRUE(item->file_path.empty());
  EXPECT_EQ(-1, item->file_size);
  EXPECT_EQ(1, dispatcher()->processing_schedule_count);
  ExpectItemDownloaded({});

  histogram_tester()->ExpectTotalCount(
      "OfflinePages.Prefetching.DownloadedFileSize", 0);
  // Bucket 1 reflects DownloadOutcome::DOWNLOAD_SUCCEEDED_ITEM_UPDATED.
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.Prefetching.DownloadFinishedUpdate", 1, 1);
}

TEST_F(DownloadCompletedTaskTest, NoUpdateOnMismatchedDownloadSuccess) {
  PrefetchDownloadResult download_result(kTestGUID2, TestFilePath(),
                                         kTestFileSize);
  RunTask(CreateTask(download_result));

  // Item will only be updated when both offline_id and state match.
  std::unique_ptr<PrefetchItem> item =
      store_util()->GetPrefetchItem(kTestOfflineID);
  EXPECT_EQ(PrefetchItemState::DOWNLOADING, item->state);

  std::unique_ptr<PrefetchItem> item2 =
      store_util()->GetPrefetchItem(kTestOfflineID2);
  EXPECT_EQ(PrefetchItemState::NEW_REQUEST, item2->state);

  EXPECT_EQ(0, dispatcher()->processing_schedule_count);
  ExpectItemDownloaded({});

  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.Prefetching.DownloadedFileSize", kTestFileSize / 1024, 1);
  // Bucket 2 reflects DownloadOutcome::DOWNLOAD_SUCCEEDED_ITEM_NOT_FOUND.
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.Prefetching.DownloadFinishedUpdate", 2, 1);
}

TEST_F(DownloadCompletedTaskTest, NoUpdateOnMismatchedDownloadError) {
  PrefetchDownloadResult download_result;
  download_result.download_id = kTestGUID2;
  download_result.success = false;
  RunTask(CreateTask(download_result));

  // Item will only be updated when both offline_id and state match.
  std::unique_ptr<PrefetchItem> item =
      store_util()->GetPrefetchItem(kTestOfflineID);
  EXPECT_EQ(PrefetchItemState::DOWNLOADING, item->state);

  std::unique_ptr<PrefetchItem> item2 =
      store_util()->GetPrefetchItem(kTestOfflineID2);
  EXPECT_EQ(PrefetchItemState::NEW_REQUEST, item2->state);

  EXPECT_EQ(0, dispatcher()->processing_schedule_count);
  ExpectItemDownloaded({});

  histogram_tester()->ExpectTotalCount(
      "OfflinePages.Prefetching.DownloadedFileSize", 0);
  // Bucket 3 reflects DownloadOutcome::DOWNLOAD_FAILED_ITEM_NOT_FOUND.
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.Prefetching.DownloadFinishedUpdate", 3, 1);
}

}  // namespace
}  // namespace offline_pages
