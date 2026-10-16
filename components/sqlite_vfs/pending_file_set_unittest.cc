// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/pending_file_set.h"

#include <stdint.h>

#include <string_view>

#include "base/byte_size.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sqlite_vfs {

namespace {

class PendingFileSetTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Returns a new file named `name` that is `length` bytes long.
  base::File CreateFile(std::string_view name, int64_t length) {
    base::File file(temp_dir_.GetPath().AppendASCII(name),
                    base::File::FLAG_CREATE | base::File::FLAG_READ |
                        base::File::FLAG_WRITE);
    EXPECT_TRUE(file.IsValid());
    EXPECT_TRUE(file.SetLength(length));
    return file;
  }

  // Returns the file named `name`, which is created if it doesn't exist yet,
  // as MakePendingFileSet() does.
  base::File OpenOrCreateFile(std::string_view name) {
    base::File file(temp_dir_.GetPath().AppendASCII(name),
                    base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                        base::File::FLAG_WRITE);
    EXPECT_TRUE(file.IsValid());
    return file;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(PendingFileSetTest, ComputeTotalFileSizeWithoutFiles) {
  EXPECT_EQ(PendingFileSet().ComputeTotalFileSize(), base::ByteSize());
}

TEST_F(PendingFileSetTest, ComputeTotalFileSize) {
  PendingFileSet pending_file_set;
  pending_file_set.db_file = CreateFile("db", 4096);
  EXPECT_EQ(pending_file_set.ComputeTotalFileSize(), base::ByteSize(4096));

  pending_file_set.journal_file = CreateFile("journal", 512);
  pending_file_set.wal_file = CreateFile("wal", 1024);
  EXPECT_EQ(pending_file_set.ComputeTotalFileSize(), base::ByteSize(5632));

  // The WAL-index file isn't included.
  pending_file_set.wal_index_file = CreateFile("wal_index", 32768);
  EXPECT_EQ(pending_file_set.ComputeTotalFileSize(), base::ByteSize(5632));
}

TEST_F(PendingFileSetTest, IsNewDatabase) {
  EXPECT_FALSE(PendingFileSet().IsNewDatabase());

  PendingFileSet new_file_set;
  new_file_set.db_file = OpenOrCreateFile("db");
  EXPECT_TRUE(new_file_set.IsNewDatabase());

  // Opening the existing database file doesn't make it new, even if its
  // journal file is created.
  PendingFileSet existing_file_set;
  existing_file_set.db_file = OpenOrCreateFile("db");
  existing_file_set.journal_file = OpenOrCreateFile("journal");
  EXPECT_FALSE(existing_file_set.IsNewDatabase());
}

}  // namespace

}  // namespace sqlite_vfs
