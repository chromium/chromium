// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/mojom/persistent_cache_mojom_traits.h"

#include <ostream>

#include "base/files/file.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/persistent_cache/mojom/persistent_cache.mojom.h"
#include "components/persistent_cache/pending_backend.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {

namespace {

// Tests that a read-only PendingBackend for the SQLite backend can be
// deserialized..
TEST(PersistentCacheReadOnlyMojomTraitsTest, Do) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create an instance with a pair of read-only file handles and lock memory.
  PendingBackend source;
  source.sqlite_data.db_file =
      base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("one")),
                 base::File::FLAG_CREATE | base::File::FLAG_READ);
  ASSERT_TRUE(source.sqlite_data.db_file.IsValid());
  source.sqlite_data.journal_file =
      base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("two")),
                 base::File::FLAG_CREATE | base::File::FLAG_READ);
  ASSERT_TRUE(source.sqlite_data.journal_file.IsValid());
  source.sqlite_data.shared_lock = base::UnsafeSharedMemoryRegion::Create(4);
  ASSERT_TRUE(source.sqlite_data.shared_lock.IsValid());
  source.read_write = false;

  // Remember the original handles.
  base::PlatformFile db_file = source.sqlite_data.db_file.GetPlatformFile();
  base::PlatformFile journal_file =
      source.sqlite_data.journal_file.GetPlatformFile();
  std::optional<base::subtle::PlatformSharedMemoryHandle> shared_lock =
      source.sqlite_data.shared_lock.GetPlatformHandle();

  // Serialize and deserialize the pending backend.
  PendingBackend result;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PendingReadOnlyBackend>(
          source, result));

  // The files and memory should have been taken away from `source`.
  EXPECT_FALSE(source.sqlite_data.db_file.IsValid());
  EXPECT_FALSE(source.sqlite_data.journal_file.IsValid());
  EXPECT_FALSE(source.sqlite_data.shared_lock.IsValid());

  // The result should be populated.
  EXPECT_TRUE(result.sqlite_data.db_file.IsValid());
  EXPECT_TRUE(result.sqlite_data.journal_file.IsValid());
  EXPECT_TRUE(result.sqlite_data.shared_lock.IsValid());
  EXPECT_FALSE(result.read_write);

  // And the handles should match.
  EXPECT_EQ(result.sqlite_data.db_file.GetPlatformFile(), db_file);
  EXPECT_EQ(result.sqlite_data.journal_file.GetPlatformFile(), journal_file);
  EXPECT_EQ(result.sqlite_data.shared_lock.GetPlatformHandle(), shared_lock);
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

// The first boolean parameter is true for a single-connection backend, or false
// for a multi-connection backend. The second boolean parameter is true for
// write-ahead log journaling mode, or false for rollback journaling mode.
using PersistentCacheReadWriteMojomTraitsTest =
    testing::TestWithParam<TestVariant>;

// Tests that a read-write PendingBackend for the SQLite backend can be
// deserialized..
TEST_P(PersistentCacheReadWriteMojomTraitsTest, Do) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create an instance with a pair of read-write file handles and lock memory.
  PendingBackend source;
  source.sqlite_data.db_file = base::File(
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("one")),
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE);
  ASSERT_TRUE(source.sqlite_data.db_file.IsValid());
  source.sqlite_data.journal_file = base::File(
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("two")),
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE);
  ASSERT_TRUE(source.sqlite_data.journal_file.IsValid());
  if (GetParam() == TestVariant::kJournalModeWal) {
    source.sqlite_data.wal_file =
        base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("three")),
                   base::File::FLAG_CREATE | base::File::FLAG_READ |
                       base::File::FLAG_WRITE);
    ASSERT_TRUE(source.sqlite_data.wal_file.IsValid());
  }

  if (GetParam() == TestVariant::kMultipleConnections) {
    source.sqlite_data.shared_lock = base::UnsafeSharedMemoryRegion::Create(4);
    ASSERT_TRUE(source.sqlite_data.shared_lock.IsValid());
  }
  source.read_write = true;

  // Remember the original handles.
  base::PlatformFile db_file = source.sqlite_data.db_file.GetPlatformFile();
  base::PlatformFile journal_file =
      source.sqlite_data.journal_file.GetPlatformFile();
  base::PlatformFile wal_file = base::kInvalidPlatformFile;
  if (GetParam() == TestVariant::kJournalModeWal) {
    wal_file = source.sqlite_data.wal_file.GetPlatformFile();
  }
  std::optional<base::subtle::PlatformSharedMemoryHandle> shared_lock;
  if (GetParam() == TestVariant::kMultipleConnections) {
    shared_lock = source.sqlite_data.shared_lock.GetPlatformHandle();
  }

  // Serialize and deserialize the pending backend.
  PendingBackend result;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PendingReadWriteBackend>(
          source, result));

  // The files should have been taken away from `source`.
  EXPECT_FALSE(source.sqlite_data.db_file.IsValid());
  EXPECT_FALSE(source.sqlite_data.journal_file.IsValid());
  EXPECT_FALSE(source.sqlite_data.wal_file.IsValid());
  EXPECT_FALSE(source.sqlite_data.shared_lock.IsValid());

  // The result should be populated.
  EXPECT_TRUE(result.sqlite_data.db_file.IsValid());
  EXPECT_TRUE(result.sqlite_data.journal_file.IsValid());
  EXPECT_EQ(result.sqlite_data.wal_file.IsValid(),
            GetParam() == TestVariant::kJournalModeWal);
  EXPECT_TRUE(result.read_write);
  EXPECT_EQ(result.sqlite_data.shared_lock.IsValid(),
            GetParam() == TestVariant::kMultipleConnections);

  // And the handles should match.
  EXPECT_EQ(result.sqlite_data.db_file.GetPlatformFile(), db_file);
  EXPECT_EQ(result.sqlite_data.journal_file.GetPlatformFile(), journal_file);
  if (GetParam() == TestVariant::kJournalModeWal) {
    EXPECT_EQ(result.sqlite_data.wal_file.GetPlatformFile(), wal_file);
  }
  if (GetParam() == TestVariant::kMultipleConnections) {
    EXPECT_EQ(result.sqlite_data.shared_lock.GetPlatformHandle(), shared_lock);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         PersistentCacheReadWriteMojomTraitsTest,
                         testing::Values(TestVariant::kMultipleConnections,
                                         TestVariant::kSingleConnection,
                                         TestVariant::kJournalModeWal),
                         testing::PrintToStringParamName());

}  // namespace

}  // namespace persistent_cache
