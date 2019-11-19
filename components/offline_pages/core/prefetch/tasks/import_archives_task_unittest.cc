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
GURL TestURL() {
  return GURL("http://sample.org");
}
GURL TestURL2() {
  return GURL("http://sample.org");
}
GURL TestFinalURL() {
  return GURL("https://sample.org/foo");
}
GURL TestFinalURL2() {
  return GURL("https://sample.org/foo");
}
std::string TestGUID() {
  return "C56A4180-65AA-42EC-A945-5FD21DEC0538";
}
ClientId TestClientID() {
  return ClientId("Foo", TestGUID());
}
std::string TestGUID2() {
  return "784f1b8b-6a32-4535-9751-ade05f947aa9";
}
ClientId TestClientID2() {
  return ClientId("Foo2", TestGUID2());
}
base::string16 TestTitle() {
  return base::UTF8ToUTF16("Hello");
}
base::string16 TestTitle2() {
  return base::UTF8ToUTF16("Hello2");
}
base::FilePath TestFilePath() {
  return base::FilePath(FILE_PATH_LITERAL("foo"));
}
base::FilePath TestFilePath2() {
  return base::FilePath(FILE_PATH_LITERAL("foo2"));
}
const int64_t kTestFileSize = 88888;
const int64_t kTestFileSize2 = 999;
GURL TestFaviconURL() {
  return GURL("http://sample.org/favicon.png");
}
GURL TestFaviconURL2() {
  return GURL("http://sample.org/favicon2.png");
}
std::string TestSnippet() {
  return "test snippet";
}
std::string TestSnippet2() {
  return "test snippet 2";
}
std::string TestAttribution() {
  return "test attribution";
}
std::string TestAttribution2() {
  return "test attribution 2";
}

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
  item.url = TestURL();
  item.guid = TestGUID();
  item.final_archived_url = TestFinalURL();
  item.client_id = TestClientID();
  item.title = TestTitle();
  item.file_path = TestFilePath();
  item.file_size = kTestFileSize;
  item.creation_time = base::Time::Now();
  item.freshness_time = item.creation_time;
  item.favicon_url = TestFaviconURL();
  item.snippet = TestSnippet();
  item.attribution = TestAttribution();
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item));

  PrefetchItem item2;
  item2.offline_id = kTestOfflineID2;
  item2.state = PrefetchItemState::DOWNLOADED;
  item2.url = TestURL2();
  item2.guid = TestGUID2();
  item2.final_archived_url = TestFinalURL2();
  item2.client_id = TestClientID2();
  item2.title = TestTitle2();
  item2.file_path = TestFilePath2();
  item2.file_size = kTestFileSize2;
  item2.creation_time = base::Time::Now();
  item2.freshness_time = item.creation_time;
  item2.favicon_url = TestFaviconURL2();
  item2.snippet = TestSnippet2();
  item2.attribution = TestAttribution2();
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
  EXPECT_EQ(TestClientID(), archive1.client_id);
  EXPECT_EQ(TestURL(), archive1.url);
  EXPECT_EQ(TestFinalURL(), archive1.final_archived_url);
  EXPECT_EQ(TestTitle(), archive1.title);
  EXPECT_EQ(TestFilePath(), archive1.file_path);
  EXPECT_EQ(kTestFileSize, archive1.file_size);
  EXPECT_EQ(TestFaviconURL(), archive1.favicon_url);
  EXPECT_EQ(TestSnippet(), archive1.snippet);
  EXPECT_EQ(TestAttribution(), archive1.attribution);

  EXPECT_EQ(kTestOfflineID2, archive2.offline_id);
  EXPECT_EQ(TestClientID2(), archive2.client_id);
  EXPECT_EQ(TestURL2(), archive2.url);
  EXPECT_EQ(TestFinalURL2(), archive2.final_archived_url);
  EXPECT_EQ(TestTitle2(), archive2.title);
  EXPECT_EQ(TestFilePath2(), archive2.file_path);
  EXPECT_EQ(kTestFileSize2, archive2.file_size);
  EXPECT_EQ(TestFaviconURL2(), archive2.favicon_url);
  EXPECT_EQ(TestSnippet2(), archive2.snippet);
  EXPECT_EQ(TestAttribution2(), archive2.attribution);
}

}  // namespace offline_pages
