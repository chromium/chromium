// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/import_completed_task.h"

#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/test_prefetch_importer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class ImportCompletedTaskTest : public PrefetchTaskTestBase {
 public:
  ImportCompletedTaskTest() = default;
  ~ImportCompletedTaskTest() override = default;

  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }
  TestPrefetchImporter* importer() { return &test_importer_; }

 private:
  TestPrefetchDispatcher dispatcher_;
  TestPrefetchImporter test_importer_;
};

TEST_F(ImportCompletedTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  RunTask(std::make_unique<ImportCompletedTask>(dispatcher(), store(),
                                                importer(), 1, true));
}

TEST_F(ImportCompletedTaskTest, ImportSuccess) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::IMPORTING);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item));

  RunTask(std::make_unique<ImportCompletedTask>(dispatcher(), store(),
                                                importer(), item.offline_id,
                                                /*success*/ true));

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::FINISHED, store_item->state);
  EXPECT_EQ(PrefetchItemErrorCode::SUCCESS, store_item->error_code);

  EXPECT_EQ(1, dispatcher()->processing_schedule_count);
  ASSERT_EQ(1u, importer()->latest_completed_offline_id.size());
  EXPECT_EQ(item.offline_id, importer()->latest_completed_offline_id.back());
}

TEST_F(ImportCompletedTaskTest, ImportError) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::IMPORTING);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item));

  RunTask(std::make_unique<ImportCompletedTask>(dispatcher(), store(),
                                                importer(), item.offline_id,
                                                /*success*/ false));

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::FINISHED, store_item->state);
  EXPECT_EQ(PrefetchItemErrorCode::IMPORT_ERROR, store_item->error_code);

  EXPECT_EQ(1, dispatcher()->processing_schedule_count);
  ASSERT_EQ(1u, importer()->latest_completed_offline_id.size());
  EXPECT_EQ(item.offline_id, importer()->latest_completed_offline_id.back());
}

TEST_F(ImportCompletedTaskTest, NoUpdateOnMismatchedImport) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::IMPORTING);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item));
  PrefetchItem item2 =
      item_generator()->CreateItem(PrefetchItemState::NEW_REQUEST);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item2));

  // Trigger an import sucecss task.
  RunTask(std::make_unique<ImportCompletedTask>(dispatcher(), store(),
                                                importer(), item2.offline_id,
                                                /*success*/ true));

  // Item will only be updated when both guid and state match.
  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(item, *store_item);

  std::unique_ptr<PrefetchItem> store_item2 =
      store_util()->GetPrefetchItem(item2.offline_id);
  ASSERT_TRUE(store_item2);
  EXPECT_EQ(item2, *store_item2);

  EXPECT_EQ(0, dispatcher()->processing_schedule_count);
  ASSERT_EQ(1u, importer()->latest_completed_offline_id.size());
  EXPECT_EQ(item2.offline_id, importer()->latest_completed_offline_id.back());

  // Trigger an import error task.
  RunTask(std::make_unique<ImportCompletedTask>(dispatcher(), store(),
                                                importer(), item2.offline_id,
                                                /*success*/ false));

  // Item will only be updated when both guid and state match.
  store_item = store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(item, *store_item);

  store_item2 = store_util()->GetPrefetchItem(item2.offline_id);
  ASSERT_TRUE(store_item2);
  EXPECT_EQ(item2, *store_item2);

  EXPECT_EQ(0, dispatcher()->processing_schedule_count);
  ASSERT_EQ(2u, importer()->latest_completed_offline_id.size());
  EXPECT_EQ(item2.offline_id, importer()->latest_completed_offline_id.back());
}

}  // namespace offline_pages
