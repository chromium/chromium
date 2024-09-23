// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/index_storage.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "chromeos/ash/components/file_manager/indexing/ram_storage.h"
#include "chromeos/ash/components/file_manager/indexing/sql_storage.h"
#include "chromeos/ash/components/file_manager/indexing/term.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::file_manager {
namespace {

// Enumerates the types of storage technology used by test.
enum StorageType {
  RAM = 0,
  SQL,
};

static constexpr base::Time::Exploded kTestTimeExploded = {
    .year = 2020,
    .month = 11,
    .day_of_month = 4,
};

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("SqlStorageTest.db");

class IndexStorageTest : public testing::TestWithParam<StorageType> {
 public:
  IndexStorageTest()
      : pinned_("label", u"pinned"), downloaded_("label", u"downloaded") {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    if (GetParam() == StorageType::RAM) {
      storage_ = std::make_unique<RamStorage>();
    } else {
      storage_ = std::make_unique<SqlStorage>(db_file_path(), "test_uma_tag");
    }
    foo_url_ =
        GURL("filesystem:file://file-manager/external/Downloads-u123/foo.txt");
    bar_url_ =
        GURL("filesystem:file://file-manager/external/Downloads-u123/bar.png");
    EXPECT_TRUE(
        base::Time::FromUTCExploded(kTestTimeExploded, &foo_modified_time_));
  }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  base::FilePath db_file_path() {
    return temp_dir_.GetPath().Append(kDatabaseName);
  }

 protected:
  Term pinned_;
  Term downloaded_;
  GURL foo_url_;
  GURL bar_url_;
  base::Time foo_modified_time_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<IndexStorage> storage_;
};

TEST_P(IndexStorageTest, Init) {
  EXPECT_TRUE(storage_->Init());
}

TEST_P(IndexStorageTest, Close) {
  EXPECT_TRUE(storage_->Close());
}

TEST_P(IndexStorageTest, GetTokenId) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  EXPECT_EQ(storage_->GetTokenId("foo"), -1);
  EXPECT_EQ(storage_->GetOrCreateTokenId("foo"), 1);
  EXPECT_EQ(storage_->GetTokenId("foo"), 1);
  // Adding the same token twice does not create a second version of "foo".
  EXPECT_EQ(storage_->GetOrCreateTokenId("foo"), 1);
}

TEST_P(IndexStorageTest, GetTermId) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  EXPECT_EQ(storage_->GetTermId(pinned_), -1);
  EXPECT_EQ(storage_->GetOrCreateTermId(pinned_), 1);
  EXPECT_EQ(storage_->GetTermId(pinned_), 1);
  EXPECT_EQ(storage_->GetTermId(downloaded_), -1);
  EXPECT_EQ(storage_->GetOrCreateTermId(downloaded_), 2);
}

TEST_P(IndexStorageTest, GetOrCreateUrlId) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  EXPECT_EQ(storage_->GetOrCreateUrlId(foo_url_), 1);
}

TEST_P(IndexStorageTest, GetUrlId) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  EXPECT_EQ(storage_->GetUrlId(foo_url_), -1);
  EXPECT_EQ(storage_->GetOrCreateUrlId(foo_url_), 1);
  EXPECT_EQ(storage_->GetUrlId(foo_url_), 1);
}

TEST_P(IndexStorageTest, DeleteUrl) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  EXPECT_EQ(storage_->DeleteUrl(foo_url_), -1);
  EXPECT_EQ(storage_->GetOrCreateUrlId(foo_url_), 1);
  EXPECT_EQ(storage_->DeleteUrl(foo_url_), 1);
}

TEST_P(IndexStorageTest, MoveUrl) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  // Cannot move non-existing URL.
  EXPECT_EQ(storage_->MoveUrl(foo_url_, bar_url_), -1);

  // Store a URL and a file info connected to it.
  EXPECT_EQ(storage_->GetOrCreateUrlId(foo_url_), 1);
  FileInfo foo_file_info(foo_url_, 100, base::Time());
  EXPECT_EQ(1, storage_->PutFileInfo(foo_file_info));

  // Move foo_url_ to foo_url_.
  EXPECT_EQ(storage_->MoveUrl(foo_url_, foo_url_), 1);

  // Move foo_url_ to bar_url_.
  EXPECT_EQ(storage_->MoveUrl(foo_url_, bar_url_), 1);

  // Expect to be able to retrieve foo_file_info by bar_url_.
  std::optional<FileInfo> file_info = storage_->GetFileInfo(1);
  EXPECT_TRUE(file_info.has_value());
  EXPECT_EQ(file_info.value().file_url, bar_url_);

  // Expect that one no longer can find foo_url_.
  EXPECT_EQ(-1, storage_->GetUrlId(foo_url_));
}

TEST_P(IndexStorageTest, GetFileInfo) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  int64_t foo_url_id = storage_->GetOrCreateUrlId(foo_url_);

  EXPECT_FALSE(storage_->GetFileInfo(-1).has_value());
  EXPECT_FALSE(storage_->GetFileInfo(foo_url_id).has_value());

  FileInfo put_file_info(foo_url_, 100, base::Time());
  EXPECT_FALSE(put_file_info.remote_id.has_value());
  EXPECT_EQ(foo_url_id, storage_->PutFileInfo(put_file_info));
  std::optional<FileInfo> file_info = storage_->GetFileInfo(foo_url_id);
  EXPECT_TRUE(file_info.has_value());
  EXPECT_EQ(put_file_info.file_url, file_info.value().file_url);
  EXPECT_EQ(put_file_info.last_modified, file_info.value().last_modified);
  EXPECT_EQ(put_file_info.size, file_info.value().size);
  EXPECT_FALSE(file_info.value().remote_id.has_value());

  put_file_info.size = put_file_info.size + 100;
  EXPECT_EQ(foo_url_id, storage_->PutFileInfo(put_file_info));
  file_info = storage_->GetFileInfo(foo_url_id);
  EXPECT_TRUE(file_info.has_value());
  EXPECT_EQ(put_file_info.size, file_info.value().size);

  const std::string remote_id = "i-am-a-remote-id";
  put_file_info.remote_id = remote_id;
  EXPECT_EQ(foo_url_id, storage_->PutFileInfo(put_file_info));

  file_info = storage_->GetFileInfo(foo_url_id);
  EXPECT_TRUE(file_info.has_value());
  EXPECT_EQ(put_file_info.remote_id, file_info.value().remote_id);
  EXPECT_EQ(remote_id, file_info.value().remote_id);
}

TEST_P(IndexStorageTest, PutFileInfo) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  FileInfo file_info(foo_url_, 100, base::Time());
  // Inserting file is always successful and the returned ID is equal to that
  // of the ID generated from file_info.file_url.
  int64_t gotten_url_id = storage_->PutFileInfo(file_info);
  int64_t foo_url_id = storage_->GetUrlId(foo_url_);
  EXPECT_EQ(foo_url_id, gotten_url_id);
}

TEST_P(IndexStorageTest, DeleteFileInfo) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  // You cannot delete file info by the invalid URL ID.
  EXPECT_EQ(-1, storage_->DeleteFileInfo(-1));

  int64_t foo_url_id = storage_->GetOrCreateUrlId(foo_url_);
  EXPECT_EQ(foo_url_id, 1);
  // Not deletion needed, but still signals that the file was "deleted"
  // successfully, as it is no longer in the index.
  EXPECT_EQ(foo_url_id, storage_->DeleteFileInfo(foo_url_id));

  FileInfo put_file_info(foo_url_, 100, base::Time());
  EXPECT_EQ(foo_url_id, storage_->PutFileInfo(put_file_info));
  EXPECT_EQ(foo_url_id, storage_->DeleteFileInfo(foo_url_id));
  EXPECT_EQ(foo_url_id, storage_->DeleteFileInfo(foo_url_id));
}

TEST_P(IndexStorageTest, AddToPostingList) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  int64_t pinned_id = storage_->GetOrCreateTermId(pinned_);
  int64_t foo_url_id = storage_->GetOrCreateUrlId(foo_url_);

  EXPECT_EQ(1u, storage_->AddToPostingList(pinned_id, foo_url_id));
  // Second time adding the term does not change the database.
  EXPECT_EQ(0u, storage_->AddToPostingList(pinned_id, foo_url_id));
}

TEST_P(IndexStorageTest, DeleteFromPostingList) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  int64_t pinned_id = storage_->GetOrCreateTermId(pinned_);
  int64_t foo_url_id = storage_->GetOrCreateUrlId(foo_url_);

  // Can delete something that was not added. Results in 0 changes.
  EXPECT_EQ(0u, storage_->DeleteFromPostingList(pinned_id, foo_url_id));

  // Add and delete, expect it to succeed.
  EXPECT_EQ(1u, storage_->AddToPostingList(pinned_id, foo_url_id));
  EXPECT_EQ(1u, storage_->DeleteFromPostingList(pinned_id, foo_url_id));
  // No more deletion after the first one.
  EXPECT_EQ(0u, storage_->DeleteFromPostingList(pinned_id, foo_url_id));
}

TEST_P(IndexStorageTest, GetUrlIdsForTerm) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  // Setup: prefetch URL IDs.
  int64_t foo_url_id = storage_->GetOrCreateUrlId(foo_url_);
  int64_t bar_url_id = storage_->GetOrCreateUrlId(bar_url_);
  int64_t pinned_id = storage_->GetOrCreateTermId(pinned_);

  // No terms were associated with any files, so the results must be empty.
  EXPECT_TRUE(storage_->GetUrlIdsForTermId(pinned_id).empty());

  // Associate pinned with foo.
  EXPECT_EQ(1u, storage_->AddToPostingList(pinned_id, foo_url_id));
  EXPECT_THAT(storage_->GetUrlIdsForTermId(pinned_id),
              testing::UnorderedElementsAre(foo_url_id));

  // Associate downloaded_ with foo.
  int64_t downloaded_term_id = storage_->GetOrCreateTermId(downloaded_);
  EXPECT_EQ(1u, storage_->AddToPostingList(downloaded_term_id, foo_url_id));
  EXPECT_THAT(storage_->GetUrlIdsForTermId(pinned_id),
              testing::UnorderedElementsAre(foo_url_id));
  EXPECT_THAT(storage_->GetUrlIdsForTermId(downloaded_term_id),
              testing::UnorderedElementsAre(foo_url_id));

  // Associate downloaded with bar.
  EXPECT_EQ(1u, storage_->AddToPostingList(downloaded_term_id, bar_url_id));
  EXPECT_THAT(storage_->GetUrlIdsForTermId(pinned_id),
              testing::UnorderedElementsAre(foo_url_id));
  EXPECT_THAT(storage_->GetUrlIdsForTermId(downloaded_term_id),
              testing::UnorderedElementsAre(foo_url_id, bar_url_id));
}

TEST_P(IndexStorageTest, GetTermIdsForUrl) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  // Setup: prefetch URL IDs.
  int64_t foo_url_id = storage_->GetOrCreateUrlId(foo_url_);
  int64_t pinned_id = storage_->GetOrCreateTermId(pinned_);
  int64_t downloaded_id = storage_->GetOrCreateTermId(downloaded_);

  // Before anything is associated with a given URL expect empty set.
  EXPECT_TRUE(storage_->GetTermIdsForUrl(foo_url_id).empty());

  EXPECT_EQ(1u, storage_->AddToPostingList(pinned_id, foo_url_id));
  EXPECT_THAT(storage_->GetTermIdsForUrl(foo_url_id),
              testing::UnorderedElementsAre(pinned_id));

  EXPECT_EQ(1u, storage_->AddToPostingList(downloaded_id, foo_url_id));
  std::set<int64_t> ids_of_foo = storage_->GetTermIdsForUrl(foo_url_id);
  EXPECT_THAT(ids_of_foo,
              testing::UnorderedElementsAre(pinned_id, downloaded_id));

  // Expect that no terms are left once we delete them for the given URL ID.
  storage_->DeleteTermIdsForUrl(ids_of_foo, foo_url_id);
  EXPECT_TRUE(storage_->GetTermIdsForUrl(foo_url_id).empty());
}

TEST_P(IndexStorageTest, CatastrophicError) {
  if (GetParam() == StorageType::RAM) {
    return;
  }
  base::FilePath db_path = temp_dir_.GetPath().Append("CatastrophicError.db");
  auto db_under_test = std::make_unique<SqlStorage>(db_path, "test_uma_tag");

  // Initialize the database and store token "foo" in it. Check that we can
  // retrieve.
  ASSERT_TRUE(db_under_test->Init());
  EXPECT_EQ(db_under_test->GetOrCreateTokenId("foo"), 1);
  EXPECT_EQ(db_under_test->GetTokenId("foo"), 1);

  // Drop the token table to cause a catastrophic error.
  ASSERT_TRUE(
      db_under_test->GetDbForTests()->Execute("DROP TABLE token_table"));

  // Expect the db to recover but not to have any data.
  EXPECT_EQ(db_under_test->GetTokenId("foo"), -1);
  // Test that the sql storage works after a recovery.
  EXPECT_EQ(db_under_test->GetOrCreateTokenId("foo"), 1);
}

INSTANTIATE_TEST_SUITE_P(Sql,
                         IndexStorageTest,
                         testing::ValuesIn<StorageType>({StorageType::SQL}));
INSTANTIATE_TEST_SUITE_P(Ram,
                         IndexStorageTest,
                         testing::ValuesIn<StorageType>({StorageType::RAM}));

}  // namespace
}  // namespace ash::file_manager
