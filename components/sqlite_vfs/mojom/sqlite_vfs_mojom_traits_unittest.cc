// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/mojom/sqlite_vfs_mojom_traits.h"

#include <ostream>

#include "base/files/file.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/sqlite_vfs/mojom/sqlite_vfs.mojom.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sqlite_vfs {

namespace {

// Tests that a read-only PendingFileSet can be deserialized..
TEST(SqliteVfsReadOnlyMojomTraitsTest, Do) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create an instance with a pair of read-only file handles and lock memory.
  PendingFileSet source;
  source.db_file =
      base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("one")),
                 base::File::FLAG_CREATE | base::File::FLAG_READ);
  ASSERT_TRUE(source.db_file.IsValid());
  source.journal_file =
      base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("two")),
                 base::File::FLAG_CREATE | base::File::FLAG_READ);
  ASSERT_TRUE(source.journal_file.IsValid());
  source.shared_lock = base::UnsafeSharedMemoryRegion::Create(4);
  ASSERT_TRUE(source.shared_lock.IsValid());
  source.read_write = false;

  // Remember the original handles.
  base::PlatformFile db_file = source.db_file.GetPlatformFile();
  base::PlatformFile journal_file = source.journal_file.GetPlatformFile();
  std::optional<base::subtle::PlatformSharedMemoryHandle> shared_lock =
      source.shared_lock.GetPlatformHandle();

  // Serialize and deserialize the pending backend.
  PendingFileSet result;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PendingReadOnlyFileSet>(
          source, result));

  // The files and memory should have been taken away from `source`.
  EXPECT_FALSE(source.db_file.IsValid());
  EXPECT_FALSE(source.journal_file.IsValid());
  EXPECT_FALSE(source.shared_lock.IsValid());

  // The result should be populated.
  EXPECT_TRUE(result.db_file.IsValid());
  EXPECT_TRUE(result.journal_file.IsValid());
  EXPECT_TRUE(result.shared_lock.IsValid());
  EXPECT_FALSE(result.read_write);

  // And the handles should match.
  EXPECT_EQ(result.db_file.GetPlatformFile(), db_file);
  EXPECT_EQ(result.journal_file.GetPlatformFile(), journal_file);
  EXPECT_EQ(result.shared_lock.GetPlatformHandle(), shared_lock);
}

enum class TestVariant {
  kMultipleConnections,
  kSingleConnection,
  kJournalModeWal,
};

// A printer for `TestVariant`; used by GoogleTest for more friendly output.
void PrintTo(TestVariant test_variant, std::ostream* os) {
  switch (test_variant) {
    case TestVariant::kMultipleConnections:
      *os << "MultipleConnections";
      break;
    case TestVariant::kSingleConnection:
      *os << "SingleConnection";
      break;
    case TestVariant::kJournalModeWal:
      *os << "JournalModeWal";
      break;
  }
}

using SqliteVfsReadWriteMojomTraitsTest = testing::TestWithParam<TestVariant>;

// Tests that a PendingFileSet for the SQLite file set can be deserialized.
TEST_P(SqliteVfsReadWriteMojomTraitsTest, Do) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create an instance with a pair of read-write file handles and lock memory.
  PendingFileSet source;
  source.db_file = base::File(
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("one")),
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE);
  ASSERT_TRUE(source.db_file.IsValid());
  source.journal_file = base::File(
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("two")),
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE);
  ASSERT_TRUE(source.journal_file.IsValid());
  if (GetParam() == TestVariant::kJournalModeWal) {
    source.wal_file =
        base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("three")),
                   base::File::FLAG_CREATE | base::File::FLAG_READ |
                       base::File::FLAG_WRITE);
    ASSERT_TRUE(source.wal_file.IsValid());
  }

  if (GetParam() == TestVariant::kMultipleConnections) {
    source.shared_lock = base::UnsafeSharedMemoryRegion::Create(4);
    ASSERT_TRUE(source.shared_lock.IsValid());
  }
  source.read_write = true;

  // Remember the original handles.
  base::PlatformFile db_file = source.db_file.GetPlatformFile();
  base::PlatformFile journal_file = source.journal_file.GetPlatformFile();
  base::PlatformFile wal_file = base::kInvalidPlatformFile;
  if (GetParam() == TestVariant::kJournalModeWal) {
    wal_file = source.wal_file.GetPlatformFile();
  }
  std::optional<base::subtle::PlatformSharedMemoryHandle> shared_lock;
  if (GetParam() == TestVariant::kMultipleConnections) {
    shared_lock = source.shared_lock.GetPlatformHandle();
  }

  // Serialize and deserialize the pending file set.
  PendingFileSet result;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PendingReadWriteFileSet>(
          source, result));

  // The files should have been taken away from `source`.
  EXPECT_FALSE(source.db_file.IsValid());
  EXPECT_FALSE(source.journal_file.IsValid());
  EXPECT_FALSE(source.wal_file.IsValid());
  EXPECT_FALSE(source.shared_lock.IsValid());

  // The result should be populated.
  EXPECT_TRUE(result.db_file.IsValid());
  EXPECT_TRUE(result.journal_file.IsValid());
  EXPECT_EQ(result.wal_file.IsValid(),
            GetParam() == TestVariant::kJournalModeWal);
  EXPECT_TRUE(result.read_write);
  EXPECT_EQ(result.shared_lock.IsValid(),
            GetParam() == TestVariant::kMultipleConnections);

  // And the handles should match.
  EXPECT_EQ(result.db_file.GetPlatformFile(), db_file);
  EXPECT_EQ(result.journal_file.GetPlatformFile(), journal_file);
  if (GetParam() == TestVariant::kJournalModeWal) {
    EXPECT_EQ(result.wal_file.GetPlatformFile(), wal_file);
  }
  if (GetParam() == TestVariant::kMultipleConnections) {
    EXPECT_EQ(result.shared_lock.GetPlatformHandle(), shared_lock);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         SqliteVfsReadWriteMojomTraitsTest,
                         testing::Values(TestVariant::kMultipleConnections,
                                         TestVariant::kSingleConnection,
                                         TestVariant::kJournalModeWal),
                         testing::PrintToStringParamName());

}  // namespace

}  // namespace sqlite_vfs
