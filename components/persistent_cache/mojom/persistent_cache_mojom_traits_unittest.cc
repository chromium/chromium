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

// Tests that a read-only PendingBackend for the SQLite backend can be
// deserialized..
TEST(PersistentCacheReadOnlyMojomTraitsTest, Do) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create an instance with a pair of read-only file handles and lock memory.
  PendingBackend source;
  source.pending_file_set.db_file =
      base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("one")),
                 base::File::FLAG_CREATE | base::File::FLAG_READ);
  source.pending_file_set.journal_file =
      base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("two")),
                 base::File::FLAG_CREATE | base::File::FLAG_READ);
  source.pending_file_set.shared_lock =
      base::UnsafeSharedMemoryRegion::Create(4);
  source.pending_file_set.read_write = false;

  // Serialize and deserialize the pending backend.
  PendingBackend result;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PendingReadOnlyBackend>(
          source, result));

  // This test simply checks whether SerializeAndDeserialize succeeds.
  // The code to verify the result of SerializeAndDeserialize is written in
  // components/sqlite_vfs/mojom/sqlite_vfs_mojom_traits_unittest.cc.
}

// Tests that a read-write PendingBackend for the SQLite backend can be
// deserialized..
TEST(PersistentCacheReadWriteMojomTraitsTest, Do) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create an instance with a pair of read-write file handles and lock memory.
  PendingBackend source;
  source.pending_file_set.db_file =
      base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("one")),
                 base::File::FLAG_CREATE | base::File::FLAG_READ);
  source.pending_file_set.journal_file =
      base::File(temp_dir.GetPath().Append(FILE_PATH_LITERAL("two")),
                 base::File::FLAG_CREATE | base::File::FLAG_READ);
  source.pending_file_set.shared_lock =
      base::UnsafeSharedMemoryRegion::Create(4);
  source.pending_file_set.read_write = true;

  // Serialize and deserialize the pending backend.
  PendingBackend result;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PendingReadWriteBackend>(
          source, result));

  // This test simply checks whether SerializeAndDeserialize succeeds.
  // The code to verify the result of SerializeAndDeserialize is written in
  // components/sqlite_vfs/mojom/sqlite_vfs_mojom_traits_unittest.cc.
}

}  // namespace persistent_cache
