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

enum class TestVariant {
  kRollbackMultipleConnections,
  kWalMultipleConnections,
  kRollbackSingleConnection,
  kWalSingleConnection,
};

// A printer for `TestVariant`; used by GoogleTest for more friendly output.
void PrintTo(TestVariant test_variant, std::ostream* os) {
  switch (test_variant) {
    case TestVariant::kRollbackMultipleConnections:
      *os << "RollbackMultipleConnections";
      break;
    case TestVariant::kWalMultipleConnections:
      *os << "WalMultipleConnections";
      break;
    case TestVariant::kRollbackSingleConnection:
      *os << "RollbackSingleConnection";
      break;
    case TestVariant::kWalSingleConnection:
      *os << "WalSingleConnection";
      break;
  }
}

using SqliteVfsReadOnlyMojomTraitsTest = testing::TestWithParam<TestVariant>;

// Tests that a read-only PendingFileSet can be deserialized.
TEST_P(SqliteVfsReadOnlyMojomTraitsTest, Do) {
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

  if (GetParam() == TestVariant::kWalMultipleConnections) {
    source.wal_file =
        base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("three")),
                   base::File::FLAG_CREATE | base::File::FLAG_READ);
    ASSERT_TRUE(source.wal_file.IsValid());
    source.wal_index_file =
        base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("four")),
                   base::File::FLAG_CREATE | base::File::FLAG_READ);
    ASSERT_TRUE(source.wal_index_file.IsValid());
  }

  // Read-only connections always have multiple connections enabled.
  source.shared_lock = base::UnsafeSharedMemoryRegion::Create(4);
  ASSERT_TRUE(source.shared_lock.IsValid());
  source.read_write = false;

  // Remember the original handles.
  base::PlatformFile db_file = source.db_file.GetPlatformFile();
  base::PlatformFile journal_file = source.journal_file.GetPlatformFile();
  base::PlatformFile wal_file = source.wal_file.GetPlatformFile();
  base::PlatformFile wal_index_file = source.wal_index_file.GetPlatformFile();
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
  EXPECT_FALSE(source.wal_file.IsValid());
  EXPECT_FALSE(source.wal_index_file.IsValid());
  EXPECT_FALSE(source.shared_lock.IsValid());

  // The result should be populated.
  EXPECT_TRUE(result.db_file.IsValid());
  EXPECT_TRUE(result.journal_file.IsValid());
  EXPECT_EQ(result.wal_file.IsValid(),
            GetParam() == TestVariant::kWalMultipleConnections);
  EXPECT_EQ(result.wal_index_file.IsValid(),
            GetParam() == TestVariant::kWalMultipleConnections);
  EXPECT_TRUE(result.shared_lock.IsValid());
  EXPECT_FALSE(result.read_write);

  // And the handles should match.
  EXPECT_EQ(result.db_file.GetPlatformFile(), db_file);
  EXPECT_EQ(result.journal_file.GetPlatformFile(), journal_file);
  if (GetParam() == TestVariant::kWalMultipleConnections) {
    EXPECT_EQ(result.wal_file.GetPlatformFile(), wal_file);
    EXPECT_EQ(result.wal_index_file.GetPlatformFile(), wal_index_file);
  }
  EXPECT_EQ(result.shared_lock.GetPlatformHandle(), shared_lock);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SqliteVfsReadOnlyMojomTraitsTest,
    testing::Values(TestVariant::kRollbackMultipleConnections,
                    TestVariant::kWalMultipleConnections),
    testing::PrintToStringParamName());

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

  if (GetParam() == TestVariant::kWalMultipleConnections ||
      GetParam() == TestVariant::kWalSingleConnection) {
    source.wal_file =
        base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("three")),
                   base::File::FLAG_CREATE | base::File::FLAG_READ |
                       base::File::FLAG_WRITE);
    ASSERT_TRUE(source.wal_file.IsValid());
  }

  if (GetParam() == TestVariant::kWalMultipleConnections) {
    source.wal_index_file =
        base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("four")),
                   base::File::FLAG_CREATE | base::File::FLAG_READ |
                       base::File::FLAG_WRITE);
    ASSERT_TRUE(source.wal_index_file.IsValid());
#if !BUILDFLAG(IS_WIN)
    source.wal_index_file_read_only =
        base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("four")),
                   base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(source.wal_index_file_read_only.IsValid());
#endif
  }

  if (GetParam() == TestVariant::kRollbackMultipleConnections ||
      GetParam() == TestVariant::kWalMultipleConnections) {
    source.shared_lock = base::UnsafeSharedMemoryRegion::Create(4);
    ASSERT_TRUE(source.shared_lock.IsValid());
  }
  source.read_write = true;

  // Remember the original handles.
  base::PlatformFile db_file = source.db_file.GetPlatformFile();
  base::PlatformFile journal_file = source.journal_file.GetPlatformFile();
  base::PlatformFile wal_file = source.wal_file.GetPlatformFile();
  base::PlatformFile wal_index_file = source.wal_index_file.GetPlatformFile();
#if !BUILDFLAG(IS_WIN)
  base::PlatformFile wal_index_file_read_only =
      source.wal_index_file_read_only.GetPlatformFile();
#endif
  std::optional<base::subtle::PlatformSharedMemoryHandle> shared_lock;
  if (source.shared_lock.IsValid()) {
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
  EXPECT_FALSE(source.wal_index_file.IsValid());
#if !BUILDFLAG(IS_WIN)
  EXPECT_FALSE(source.wal_index_file_read_only.IsValid());
#endif
  EXPECT_FALSE(source.shared_lock.IsValid());

  // The result should be populated.
  EXPECT_TRUE(result.db_file.IsValid());
  EXPECT_TRUE(result.journal_file.IsValid());
  EXPECT_EQ(result.wal_file.IsValid(),
            GetParam() == TestVariant::kWalMultipleConnections ||
                GetParam() == TestVariant::kWalSingleConnection);
  EXPECT_EQ(result.wal_index_file.IsValid(),
            GetParam() == TestVariant::kWalMultipleConnections);
#if !BUILDFLAG(IS_WIN)
  EXPECT_EQ(result.wal_index_file_read_only.IsValid(),
            GetParam() == TestVariant::kWalMultipleConnections);
#endif
  EXPECT_TRUE(result.read_write);
  EXPECT_EQ(result.shared_lock.IsValid(),
            GetParam() == TestVariant::kRollbackMultipleConnections ||
                GetParam() == TestVariant::kWalMultipleConnections);

  // And the handles should match.
  EXPECT_EQ(result.db_file.GetPlatformFile(), db_file);
  EXPECT_EQ(result.journal_file.GetPlatformFile(), journal_file);
  if (wal_file != base::kInvalidPlatformFile) {
    EXPECT_EQ(result.wal_file.GetPlatformFile(), wal_file);
  }
  if (wal_index_file != base::kInvalidPlatformFile) {
    EXPECT_EQ(result.wal_index_file.GetPlatformFile(), wal_index_file);
  }
#if !BUILDFLAG(IS_WIN)
  if (wal_index_file_read_only != base::kInvalidPlatformFile) {
    EXPECT_EQ(result.wal_index_file_read_only.GetPlatformFile(),
              wal_index_file_read_only);
  }
#endif
  if (shared_lock) {
    EXPECT_EQ(result.shared_lock.GetPlatformHandle(), shared_lock);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SqliteVfsReadWriteMojomTraitsTest,
    testing::Values(TestVariant::kRollbackMultipleConnections,
                    TestVariant::kWalMultipleConnections,
                    TestVariant::kRollbackSingleConnection,
                    TestVariant::kWalSingleConnection),
    testing::PrintToStringParamName());

}  // namespace

}  // namespace sqlite_vfs
