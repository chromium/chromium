// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/mojom/persistent_cache_mojom_traits.h"

#include "base/files/file.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/mojom/persistent_cache.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {

namespace {

// Tests that a valid params is deserialized as a read-write BackendParams for
// the SQLite backend.
TEST(PersistentCacheMojomTraitsTest, ReadWrite) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create an instance with a pair of read-write file handles and lock memory.
  BackendParams source;
  source.type = BackendType::kSqlite;
  source.db_file = base::File(
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("one")),
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE);
  ASSERT_TRUE(source.db_file.IsValid());
  source.db_file_is_writable = true;
  source.journal_file = base::File(
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("two")),
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE);
  ASSERT_TRUE(source.journal_file.IsValid());
  source.journal_file_is_writable = true;
  source.shared_lock = base::UnsafeSharedMemoryRegion::Create(4);
  ASSERT_TRUE(source.shared_lock.IsValid());

  // Remember the original handles.
  base::PlatformFile db_file = source.db_file.GetPlatformFile();
  base::PlatformFile journal_file = source.journal_file.GetPlatformFile();
  base::subtle::PlatformSharedMemoryHandle shared_lock =
      source.shared_lock.GetPlatformHandle();

  // Serialize and deserialize the params.
  BackendParams result;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ReadWriteBackendParams>(
          source, result));

  // The files should have been taken away from `source`.
  EXPECT_FALSE(source.db_file.IsValid());
  EXPECT_FALSE(source.journal_file.IsValid());
  EXPECT_FALSE(source.shared_lock.IsValid());

  // The result should be populated.
  EXPECT_EQ(result.type, BackendType::kSqlite);
  EXPECT_TRUE(result.db_file.IsValid());
  EXPECT_TRUE(result.db_file_is_writable);
  EXPECT_TRUE(result.journal_file.IsValid());
  EXPECT_TRUE(result.journal_file_is_writable);
  EXPECT_TRUE(result.shared_lock.IsValid());

  // And the handles should match.
  EXPECT_EQ(result.db_file.GetPlatformFile(), db_file);
  EXPECT_EQ(result.journal_file.GetPlatformFile(), journal_file);
  EXPECT_EQ(result.shared_lock.GetPlatformHandle(), shared_lock);
}

}  // namespace

}  // namespace persistent_cache
