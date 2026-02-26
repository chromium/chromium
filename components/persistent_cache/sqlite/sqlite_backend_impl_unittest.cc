// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"

#include <stdint.h>

#include <array>
#include <optional>

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/byte_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "components/persistent_cache/backend_storage.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/client.h"
#include "components/persistent_cache/persistent_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {

namespace {

using ::base::test::ValueIs;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::Optional;

// See https://sqlite.org/fileformat2.html#user_version_number
constexpr int64_t kUserVersionOffset = 60;

// Make sure the files are initialized with an invalid version.
// 0 is chosen for two reasons: It's a value this is not equal to
// `kCurrentUserVersion` so validates that mismatches are not tolerated. It's
// also the default value present prior to the use of user_version and real
// files exist in clients with no specific user versions and tables which need
// to be discarded.
constexpr int kInvalidVersion = 0;

}  // namespace

class SQLiteBackendImplTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_storage_.emplace(Client::kTest, BackendType::kSqlite,
                             temp_dir_.GetPath());
  }

  base::ScopedTempDir temp_dir_;
  std::optional<BackendStorage> backend_storage_;
};

TEST_F(SQLiteBackendImplTest, ReopeningFilesWithSameUserVersionWorks) {
  const base::FilePath db_basename = base::FilePath::FromASCII("Cache");

  ASSERT_OK_AND_ASSIGN(
      auto pending_backend,
      backend_storage_->MakePendingBackend(db_basename, false, false));

  // Initialize the backend and close it.
  ASSERT_NE(SqliteBackendImpl::Bind(std::move(pending_backend), Client::kTest),
            nullptr);

  ASSERT_OK_AND_ASSIGN(pending_backend, backend_storage_->MakePendingBackend(
                                            db_basename, false, false));

  // NOTE: This does not respect the database locks and so is done when there
  // are no live backends.
  std::array<uint8_t, sizeof(int)> data;
  ASSERT_THAT(
      pending_backend.pending_file_set.db_file.Read(kUserVersionOffset, data),
      Optional(sizeof(int)));
  ASSERT_EQ(base::I32FromBigEndian(data),
            SqliteBackendImpl::kCurrentUserVersion);

  ASSERT_NE(SqliteBackendImpl::Bind(std::move(pending_backend), Client::kTest),
            nullptr);
}

TEST_F(SQLiteBackendImplTest,
       VersionMismatchLeadsToFailedInitializeWhenReadOnly) {
  const base::FilePath db_path = base::FilePath::FromASCII("Cache");

  ASSERT_OK_AND_ASSIGN(
      auto pending_backend,
      backend_storage_->MakePendingBackend(db_path, false, false));
  base::File db_file = pending_backend.pending_file_set.db_file.Duplicate();

  std::unique_ptr<PersistentCache> cache =
      PersistentCache::Bind(Client::kTest, std::move(pending_backend));
  ASSERT_NE(cache, nullptr);

  ASSERT_OK_AND_ASSIGN(
      pending_backend,
      backend_storage_->ShareReadOnlyConnection(db_path, *cache));

  // Manually set the user_version in the db file to an invalid value.
  // NOTE: This does not respect the database locks and so is done when there
  // are no live backends.
  cache.reset();
  ASSERT_THAT(
      db_file.Write(kUserVersionOffset, base::I32ToBigEndian(kInvalidVersion)),
      Optional(sizeof(int)));

  // Mismatched version numbers makes Bind() fail.
  ASSERT_EQ(SqliteBackendImpl::Bind(std::move(pending_backend), Client::kTest),
            nullptr);
}

TEST_F(SQLiteBackendImplTest,
       VersionMismatchDropsTablesWithReadWriteConnection) {
  const base::FilePath db_basename = base::FilePath::FromASCII("Cache");

  ASSERT_OK_AND_ASSIGN(
      auto pending_backend,
      backend_storage_->MakePendingBackend(db_basename, false, false));

  std::unique_ptr<PersistentCache> cache =
      PersistentCache::Bind(Client::kTest, std::move(pending_backend));
  ASSERT_NE(cache, nullptr);

  const base::span<const uint8_t> kKey = base::byte_span_from_cstring("key");
  EXPECT_OK(cache->Insert(kKey, base::byte_span_from_cstring("1")));
  EXPECT_THAT(cache->Find(kKey, [](size_t) { return base::span<uint8_t>(); }),
              ValueIs(Ne(std::nullopt)));

  ASSERT_OK_AND_ASSIGN(
      pending_backend,
      backend_storage_->ShareReadWriteConnection(db_basename, *cache));

  // Manually set the user_version in the db file to an invalid value.
  // NOTE: This does not respect the database locks and so is done when there
  // are no live backends.
  cache.reset();
  ASSERT_THAT(pending_backend.pending_file_set.db_file.Write(
                  kUserVersionOffset, base::I32ToBigEndian(kInvalidVersion)),
              Optional(sizeof(int)));

  // Bind succeeds.
  std::unique_ptr<Backend> backend =
      SqliteBackendImpl::Bind(std::move(pending_backend), Client::kTest);
  ASSERT_NE(backend, nullptr);

  // The files were wiped so the value inserted is no longer present.
  EXPECT_THAT(backend->Find(kKey, [](size_t) { return base::span<uint8_t>(); }),
              ValueIs(Eq(std::nullopt)));
}

}  // namespace persistent_cache
