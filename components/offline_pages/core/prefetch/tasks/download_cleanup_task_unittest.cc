// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/download_cleanup_task.h"

#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const base::FilePath kTestFilePath(FILE_PATH_LITERAL("foo"));
const int64_t kTestFileSize = 88888;
}  // namespace

class DownloadCleanupTaskTest : public PrefetchTaskTestBase {
 public:
  DownloadCleanupTaskTest() = default;
  ~DownloadCleanupTaskTest() override = default;

  void CreateAndRunDownloadCleanupTask(
      const std::string& outstanding_download_id,
      const std::string& success_download_id) {
    std::set<std::string> outstanding_download_ids;
    if (!outstanding_download_id.empty())
      outstanding_download_ids.emplace(outstanding_download_id);

    std::map<std::string, std::pair<base::FilePath, int64_t>> success_downloads;
    if (!success_download_id.empty()) {
      success_downloads.emplace(success_download_id,
                                std::make_pair(kTestFilePath, kTestFileSize));
    }

    RunTask(std::make_unique<DownloadCleanupTask>(
        &dispatcher_, store(), outstanding_download_ids, success_downloads));
  }

 private:
  TestPrefetchDispatcher dispatcher_;
};

TEST_F(DownloadCleanupTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  CreateAndRunDownloadCleanupTask(std::string(), std::string());
}

TEST_F(DownloadCleanupTaskTest, Retry) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::DOWNLOADING);
  item.download_initiation_attempts =
      DownloadCleanupTask::kMaxDownloadAttempts - 1;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  CreateAndRunDownloadCleanupTask(std::string(), std::string());

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::RECEIVED_BUNDLE, store_item->state);
  EXPECT_EQ(item.download_initiation_attempts,
            store_item->download_initiation_attempts);
  EXPECT_EQ(PrefetchItemErrorCode::SUCCESS, store_item->error_code);
}

TEST_F(DownloadCleanupTaskTest, NoRetryForOngoingDownload) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::DOWNLOADING);
  item.download_initiation_attempts =
      DownloadCleanupTask::kMaxDownloadAttempts - 1;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  CreateAndRunDownloadCleanupTask(item.guid, std::string());

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(item, *store_item);
}

TEST_F(DownloadCleanupTaskTest, ErrorOnMaxAttempts) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::DOWNLOADING);
  item.download_initiation_attempts = DownloadCleanupTask::kMaxDownloadAttempts;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  CreateAndRunDownloadCleanupTask(std::string(), std::string());

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::FINISHED, store_item->state);
  EXPECT_EQ(PrefetchItemErrorCode::DOWNLOAD_MAX_ATTEMPTS_REACHED,
            store_item->error_code);
  EXPECT_EQ(item.download_initiation_attempts,
            store_item->download_initiation_attempts);
}

TEST_F(DownloadCleanupTaskTest, SkipForOngoingDownloadWithMaxAttempts) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::DOWNLOADING);
  item.download_initiation_attempts = DownloadCleanupTask::kMaxDownloadAttempts;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  CreateAndRunDownloadCleanupTask(item.guid, std::string());

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(item, *store_item);
}

TEST_F(DownloadCleanupTaskTest, NoUpdateForOtherStates) {
  std::set<PrefetchItem> items;
  std::vector<PrefetchItemState> all_other_states =
      GetAllStatesExcept({PrefetchItemState::DOWNLOADING});
  for (const auto& state : all_other_states) {
    PrefetchItem item = item_generator()->CreateItem(state);
    item.download_initiation_attempts =
        DownloadCleanupTask::kMaxDownloadAttempts;
    ASSERT_TRUE(store_util()->InsertPrefetchItem(item));
    items.insert(item);
  }

  CreateAndRunDownloadCleanupTask(std::string(), std::string());

  std::set<PrefetchItem> store_items;
  store_util()->GetAllItems(&store_items);
  EXPECT_EQ(items, store_items);
}

TEST_F(DownloadCleanupTaskTest, MarkDownloadCompleted) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::DOWNLOADING);
  item.download_initiation_attempts =
      DownloadCleanupTask::kMaxDownloadAttempts - 1;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  CreateAndRunDownloadCleanupTask(std::string(), item.guid);

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::DOWNLOADED, store_item->state);
  EXPECT_EQ(kTestFilePath, store_item->file_path);
  EXPECT_EQ(kTestFileSize, store_item->file_size);
}

}  // namespace offline_pages
