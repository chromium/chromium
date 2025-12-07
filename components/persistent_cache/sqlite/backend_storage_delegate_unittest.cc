// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/backend_storage_delegate.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/constants.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
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
  ASSERT_EQ(delegate().GetBaseName(
                temp_path().AppendASCII("spam").AddExtension(kDbFileExtension)),
            base::FilePath::FromASCII("spam"));
  ASSERT_EQ(delegate().GetBaseName(temp_path().AppendASCII("spam").AddExtension(
                kJournalFileExtension)),
            base::FilePath());
  ASSERT_EQ(delegate().GetBaseName(temp_path().AppendASCII("spam").AddExtension(
                kWalJournalFileExtension)),
            base::FilePath());
}

TEST_F(SqliteBackendStorageDelegateTest, CreateAndDelete) {
  base::FilePath base_name = base::FilePath::FromASCII("base_name");
  auto pending_backend =
      delegate().MakePendingBackend(temp_path(), base_name,
                                    /*single_connection=*/false,
                                    /*journal_mode_wal=*/false);
  ASSERT_NE(pending_backend, std::nullopt);
  auto backend = SqliteBackendImpl::Bind(*std::move(pending_backend));
  ASSERT_NE(backend, nullptr);

  // The backend should have created some files.
  ASSERT_FALSE(base::IsDirectoryEmpty(temp_path()));

  // Close the files.
  backend.reset();

  int64_t dir_size = base::ComputeDirectorySize(temp_path());

  // Ask the delegate to delete them.
  ASSERT_EQ(delegate().DeleteFiles(temp_path(), base_name), dir_size);

  // The files should now be gone.
  ASSERT_TRUE(base::IsDirectoryEmpty(temp_path()));
}

TEST_F(SqliteBackendStorageDelegateTest, CreateAndDeleteWal) {
  base::FilePath base_name = base::FilePath::FromASCII("base_name");
  auto pending_backend =
      delegate().MakePendingBackend(temp_path(), base_name,
                                    /*single_connection=*/true,
                                    /*journal_mode_wal=*/true);
  ASSERT_NE(pending_backend, std::nullopt);
  auto backend = SqliteBackendImpl::Bind(*std::move(pending_backend));
  ASSERT_NE(backend, nullptr);

  // The backend should have created some files.
  ASSERT_FALSE(base::IsDirectoryEmpty(temp_path()));

  // Close the files.
  backend.reset();

  int64_t dir_size = base::ComputeDirectorySize(temp_path());

  // Ask the delegate to delete them.
  ASSERT_EQ(delegate().DeleteFiles(temp_path(), base_name), dir_size);

  // The files should now be gone.
  ASSERT_TRUE(base::IsDirectoryEmpty(temp_path()));
}

}  // namespace

}  // namespace persistent_cache::sqlite
