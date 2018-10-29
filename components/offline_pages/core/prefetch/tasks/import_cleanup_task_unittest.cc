// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/import_cleanup_task.h"

#include "components/offline_pages/core/prefetch/prefetch_importer.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

class TestPrefetchImporter : public PrefetchImporter {
 public:
  TestPrefetchImporter() : PrefetchImporter(nullptr) {}
  ~TestPrefetchImporter() override = default;

  void ImportArchive(const PrefetchArchiveInfo& archive) override {
    outstanding_import_offline_ids_.emplace(archive.offline_id);
  }

  void MarkImportCompleted(int64_t offline_id) override {
    outstanding_import_offline_ids_.erase(offline_id);
  }

  std::set<int64_t> GetOutstandingImports() const override {
    return outstanding_import_offline_ids_;
  }

 private:
  std::set<int64_t> outstanding_import_offline_ids_;
};

}  // namespace

class ImportCleanupTaskTest : public PrefetchTaskTestBase {
 public:
  ImportCleanupTaskTest() = default;
  ~ImportCleanupTaskTest() override = default;

  TestPrefetchImporter* importer() { return &test_importer_; }

 private:
  TestPrefetchImporter test_importer_;
};

TEST_F(ImportCleanupTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  RunTask(std::make_unique<ImportCleanupTask>(store(), importer()));
}

TEST_F(ImportCleanupTaskTest, DoCleanup) {
  // Create item 1 in IMPORTING state.
  PrefetchItem item1 =
      item_generator()->CreateItem(PrefetchItemState::IMPORTING);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item1));

  // Create item 2 not in IMPORTING state.
  PrefetchItem item2 =
      item_generator()->CreateItem(PrefetchItemState::DOWNLOADING);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item2));

  // Create item 3 in IMPORTING state.
  PrefetchItem item3 =
      item_generator()->CreateItem(PrefetchItemState::IMPORTING);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item3));

  // Clean up the imports.
  RunTask(std::make_unique<ImportCleanupTask>(store(), importer()));

  // Item 1 is cleaned up.
  std::unique_ptr<PrefetchItem> store_item1 =
      store_util()->GetPrefetchItem(item1.offline_id);
  ASSERT_TRUE(store_item1);
  EXPECT_EQ(PrefetchItemState::FINISHED, store_item1->state);
  EXPECT_EQ(PrefetchItemErrorCode::IMPORT_LOST, store_item1->error_code);

  // Item 2 is not cleaned up since it is not in IMPORTING state.
  std::unique_ptr<PrefetchItem> store_item2 =
      store_util()->GetPrefetchItem(item2.offline_id);
  ASSERT_TRUE(store_item2);
  EXPECT_EQ(item2, *store_item2);

  // Item 3 is cleaned up.
  std::unique_ptr<PrefetchItem> store_item3 =
      store_util()->GetPrefetchItem(item3.offline_id);
  ASSERT_TRUE(store_item3);
  EXPECT_EQ(PrefetchItemState::FINISHED, store_item3->state);
  EXPECT_EQ(PrefetchItemErrorCode::IMPORT_LOST, store_item3->error_code);
}

TEST_F(ImportCleanupTaskTest, NoCleanupForOutstandingImport) {
  // Create item 1 in IMPORTING state. This item is also in the outstanding
  // import list.
  PrefetchItem item1 =
      item_generator()->CreateItem(PrefetchItemState::IMPORTING);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item1));

  PrefetchArchiveInfo archive_info1;
  archive_info1.offline_id = item1.offline_id;
  importer()->ImportArchive(archive_info1);

  // Create item 2 in IMPORTING state. This item is not in the outstanding
  // import list.
  PrefetchItem item2 =
      item_generator()->CreateItem(PrefetchItemState::IMPORTING);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item2));

  // Create item 3 in IMPORTING state. This item is also in the outstanding
  // import list.
  PrefetchItem item3 =
      item_generator()->CreateItem(PrefetchItemState::IMPORTING);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item3));

  PrefetchArchiveInfo archive_info3;
  archive_info3.offline_id = item3.offline_id;
  importer()->ImportArchive(archive_info3);

  // Clean up the imports.
  RunTask(std::make_unique<ImportCleanupTask>(store(), importer()));

  // Item 1 is intact since it is in the outstanding list.
  std::unique_ptr<PrefetchItem> store_item1 =
      store_util()->GetPrefetchItem(item1.offline_id);
  ASSERT_TRUE(store_item1);
  EXPECT_EQ(item1, *store_item1);

  // Item 2 is cleaned up.
  std::unique_ptr<PrefetchItem> store_item2 =
      store_util()->GetPrefetchItem(item2.offline_id);
  ASSERT_TRUE(store_item2);
  EXPECT_EQ(PrefetchItemState::FINISHED, store_item2->state);
  EXPECT_EQ(PrefetchItemErrorCode::IMPORT_LOST, store_item2->error_code);

  // Item 3 is intact since it is in the outstanding list.
  std::unique_ptr<PrefetchItem> store_item3 =
      store_util()->GetPrefetchItem(item3.offline_id);
  ASSERT_TRUE(store_item3);
  EXPECT_EQ(item3, *store_item3);

  // Mark item 1 as completed in order to remove it from the outstanding list.
  importer()->MarkImportCompleted(item1.offline_id);

  // Trigger another import cleanup.
  RunTask(std::make_unique<ImportCleanupTask>(store(), importer()));

  // Item 1 should now be cleaned up.
  store_item1 = store_util()->GetPrefetchItem(item1.offline_id);
  ASSERT_TRUE(store_item1);
  EXPECT_EQ(PrefetchItemState::FINISHED, store_item1->state);
  EXPECT_EQ(PrefetchItemErrorCode::IMPORT_LOST, store_item1->error_code);

  // Item 3 is still intact since it is in the outstanding list.
  store_item3 = store_util()->GetPrefetchItem(item3.offline_id);
  ASSERT_TRUE(store_item3);
  EXPECT_EQ(item3, *store_item3);
}

}  // namespace offline_pages
