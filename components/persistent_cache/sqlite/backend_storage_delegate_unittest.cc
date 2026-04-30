// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/backend_storage_delegate.h"

#include <tuple>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/client.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/sqlite_vfs/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache::sqlite {

namespace {

class SqliteBackendStorageDelegateTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const base::FilePath& temp_path() const { return temp_dir_.GetPath(); }

  BackendStorageDelegate& delegate() { return delegate_; }

 private:
  base::ScopedTempDir temp_dir_;
  BackendStorageDelegate delegate_;
};

TEST_F(SqliteBackendStorageDelegateTest, GetBaseName) {
  ASSERT_EQ(delegate().GetBaseName(base::FilePath()), base::FilePath());
  ASSERT_EQ(delegate().GetBaseName(temp_path()), base::FilePath());
  ASSERT_EQ(delegate().GetBaseName(temp_path().AppendASCII("spam").AddExtension(
                sqlite_vfs::kDbFileExtension)),
            base::FilePath::FromASCII("spam"));
  ASSERT_EQ(delegate().GetBaseName(temp_path().AppendASCII("spam").AddExtension(
                sqlite_vfs::kJournalFileExtension)),
            base::FilePath());
  ASSERT_EQ(delegate().GetBaseName(temp_path().AppendASCII("spam").AddExtension(
                sqlite_vfs::kWalJournalFileExtension)),
            base::FilePath());
}

class SqliteBackendStorageDelegateParamTest
    : public SqliteBackendStorageDelegateTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  static bool is_single_connection() { return std::get<0>(GetParam()); }
  static bool journal_mode_wal() { return std::get<1>(GetParam()); }
};

TEST_P(SqliteBackendStorageDelegateParamTest, CreateAndDelete) {
  base::FilePath base_name = base::FilePath::FromASCII("base_name");
  auto pending_backend =
      delegate().MakePendingBackend(Client::kTest, temp_path(), base_name,
                                    is_single_connection(), journal_mode_wal());
  ASSERT_NE(pending_backend, std::nullopt);
  ASSERT_OK_AND_ASSIGN(
      auto backend,
      SqliteBackendImpl::Bind(*std::move(pending_backend), Client::kTest));
  ASSERT_NE(backend, nullptr);

  // The backend should have created some files.
  ASSERT_FALSE(base::IsDirectoryEmpty(temp_path()));

  // Close the files.
  backend.reset();

  int64_t dir_size = base::ComputeDirectorySize(temp_path());

  // Ask the delegate to delete them.
  ASSERT_EQ(delegate().DeleteFiles(Client::kTest, temp_path(), base_name),
            dir_size);

  // The files should now be gone.
  ASSERT_TRUE(base::IsDirectoryEmpty(temp_path()));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SqliteBackendStorageDelegateParamTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<
        SqliteBackendStorageDelegateParamTest::ParamType>& info) {
      bool single_connection = std::get<0>(info.param);
      bool journal_mode_wal = std::get<1>(info.param);
      return base::StrCat(
          {single_connection ? "SingleConnection" : "MultipleConnections",
           journal_mode_wal ? "Wal" : "Rollback"});
    });

}  // namespace

}  // namespace persistent_cache::sqlite
