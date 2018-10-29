// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/import_archives_task.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/core/prefetch/prefetch_importer.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const int64_t kTestOfflineID = 1111;
const int64_t kTestOfflineID2 = 223344;
const int64_t kTestOfflineID3 = 987;
const GURL kTestURL("http://sample.org");
const GURL kTestURL2("http://sample.org");
const GURL kTestFinalURL("https://sample.org/foo");
const GURL kTestFinalURL2("https://sample.org/foo");
const std::string kTestGUID("C56A4180-65AA-42EC-A945-5FD21DEC0538");
const ClientId kTestClientID("Foo", kTestGUID);
const std::string kTestGUID2("784f1b8b-6a32-4535-9751-ade05f947aa9");
const ClientId kTestClientID2("Foo2", kTestGUID2);
const base::string16 kTestTitle = base::ASCIIToUTF16("Hello");
const base::string16 kTestTitle2 = base::ASCIIToUTF16("Hello2");
const base::FilePath kTestFilePath(FILE_PATH_LITERAL("foo"));
const base::FilePath kTestFilePath2(FILE_PATH_LITERAL("foo2"));
const int64_t kTestFileSize = 88888;
const int64_t kTestFileSize2 = 999;

class TestPrefetchImporter : public PrefetchImporter {
 public:
  TestPrefetchImporter() : PrefetchImporter(nullptr) {}
  ~TestPrefetchImporter() override = default;

  void ImportArchive(const PrefetchArchiveInfo& archive) override {
    archives_.push_back(archive);
  }
  void MarkImportCompleted(int64_t offline_id) override {}
  std::set<int64_t> GetOutstandingImports() const override {
    return std::set<int64_t>();
  }

  const std::vector<PrefetchArchiveInfo>& archives() const { return archives_; }

 private:
  std::vector<PrefetchArchiveInfo> archives_;
};

}  // namespace

class ImportArchivesTaskTest : public PrefetchTaskTestBase {
 public:
  ~ImportArchivesTaskTest() override = default;

  void SetUp() override;

  TestPrefetchImporter* importer() { return &test_importer_; }

 private:
  TestPrefetchImporter test_importer_;
};

void ImportArchivesTaskTest::SetUp() {
  PrefetchTaskTestBase::SetUp();
  PrefetchItem item;
  item.offline_id = kTestOfflineID;
  item.state = PrefetchItemState::DOWNLOADED;
  item.url = kTestURL;
  item.guid = kTestGUID;
  item.final_archived_url = kTestFinalURL;
  item.client_id = kTestClientID;
  item.title = kTestTitle;
  item.file_path = kTestFilePath;
  item.file_size = kTestFileSize;
  item.creation_time = base::Time::Now();
  item.freshness_time = item.creation_time;
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item));

  PrefetchItem item2;
  item2.offline_id = kTestOfflineID2;
  item2.state = PrefetchItemState::DOWNLOADED;
  item2.url = kTestURL2;
  item2.guid = kTestGUID2;
  item2.final_archived_url = kTestFinalURL2;
  item2.client_id = kTestClientID2;
  item2.title = kTestTitle2;
  item2.file_path = kTestFilePath2;
  item2.file_size = kTestFileSize2;
  item2.creation_time = base::Time::Now();
  item2.freshness_time = item.creation_time;
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item2));

  PrefetchItem item3;
  item3.offline_id = kTestOfflineID3;
  item3.state = PrefetchItemState::NEW_REQUEST;
  item3.creation_time = base::Time::Now();
  item3.freshness_time = item.creation_time;
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item3));
}

TEST_F(ImportArchivesTaskTest, NullConnection) {
  store_util()->SimulateInitializationError();
  RunTask(std::make_unique<ImportArchivesTask>(store(), importer()));
}

TEST_F(ImportArchivesTaskTest, Importing) {
  RunTask(std::make_unique<ImportArchivesTask>(store(), importer()));

  // Two items are updated.
  std::unique_ptr<PrefetchItem> item =
      store_util()->GetPrefetchItem(kTestOfflineID);
  EXPECT_EQ(PrefetchItemState::IMPORTING, item->state);

  item = store_util()->GetPrefetchItem(kTestOfflineID2);
  EXPECT_EQ(PrefetchItemState::IMPORTING, item->state);

  // One item is not updated.
  item = store_util()->GetPrefetchItem(kTestOfflineID3);
  EXPECT_EQ(PrefetchItemState::NEW_REQUEST, item->state);

  // Validate the info passed to PrefetchImporter.
  ASSERT_EQ(2u, importer()->archives().size());

  PrefetchArchiveInfo archive1;
  PrefetchArchiveInfo archive2;
  if (importer()->archives()[0].offline_id == kTestOfflineID) {
    archive1 = importer()->archives()[0];
    archive2 = importer()->archives()[1];
  }
  if (importer()->archives()[1].offline_id == kTestOfflineID) {
    archive1 = importer()->archives()[1];
    archive2 = importer()->archives()[0];
  }
  EXPECT_EQ(kTestOfflineID, archive1.offline_id);
  EXPECT_EQ(kTestClientID, archive1.client_id);
  EXPECT_EQ(kTestURL, archive1.url);
  EXPECT_EQ(kTestFinalURL, archive1.final_archived_url);
  EXPECT_EQ(kTestTitle, archive1.title);
  EXPECT_EQ(kTestFilePath, archive1.file_path);
  EXPECT_EQ(kTestFileSize, archive1.file_size);

  EXPECT_EQ(kTestOfflineID2, archive2.offline_id);
  EXPECT_EQ(kTestClientID2, archive2.client_id);
  EXPECT_EQ(kTestURL2, archive2.url);
  EXPECT_EQ(kTestFinalURL2, archive2.final_archived_url);
  EXPECT_EQ(kTestTitle2, archive2.title);
  EXPECT_EQ(kTestFilePath2, archive2.file_path);
  EXPECT_EQ(kTestFileSize2, archive2.file_size);
}

}  // namespace offline_pages
