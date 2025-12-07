// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/persistent_cache/backend_storage.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/mock/mock_backend.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/persistent_cache/test_utils.h"
#include "components/persistent_cache/transaction_error.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {

constexpr const char* kKey = "foo";

using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;
using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Field;
using testing::Ge;
using testing::Le;
using testing::Ne;
using testing::Optional;
using testing::Return;

class PersistentCacheMockedBackendTest : public testing::Test {
 protected:
  void SetUp() override {
    backend_ = std::make_unique<persistent_cache::MockBackend>();
  }

  void CreateCache() {
    cache_ = std::make_unique<persistent_cache::PersistentCache>(
        std::move(backend_));
  }

  persistent_cache::MockBackend* GetBackend() {
    // Can't be called without a cache.
    CHECK(cache_);
    return static_cast<persistent_cache::MockBackend*>(
        cache_->GetBackendForTesting());
  }

  std::unique_ptr<persistent_cache::MockBackend> backend_;
  std::unique_ptr<persistent_cache::PersistentCache> cache_;
};

}  // namespace

namespace persistent_cache {

TEST_F(PersistentCacheMockedBackendTest, CacheFindCallsBackendFind) {
  CreateCache();
  EXPECT_CALL(*GetBackend(), Find(kKey, _))
      .WillOnce(Return(base::ok(std::nullopt)));

  EXPECT_THAT(cache_->Find(kKey, [](size_t) { return base::span<uint8_t>(); }),
              ValueIs(Eq(std::nullopt)));
}

TEST_F(PersistentCacheMockedBackendTest, FindReturnsBackendError) {
  CreateCache();
  EXPECT_CALL(*GetBackend(), Find(kKey, _))
      .WillOnce(Return(base::unexpected(TransactionError::kTransient)));
  EXPECT_THAT(cache_->Find(kKey, [](size_t) { return base::span<uint8_t>(); }),
              ErrorIs(TransactionError::kTransient));
}

TEST_F(PersistentCacheMockedBackendTest, InsertReturnsBackendError) {
  CreateCache();
  EXPECT_CALL(*GetBackend(), Insert(_, _, _))
      .WillOnce(Return(base::unexpected(TransactionError::kTransient)));
  EXPECT_THAT(cache_->Insert(kKey, base::byte_span_from_cstring("1")),
              ErrorIs(TransactionError::kTransient));
}

TEST_F(PersistentCacheMockedBackendTest, CacheInsertCallsBackendInsert) {
  CreateCache();
  EXPECT_CALL(*GetBackend(), Insert(kKey, _, _));
  EXPECT_THAT(cache_->Insert(kKey, base::byte_span_from_cstring("1")),
              HasValue());
}

#if !BUILDFLAG(IS_FUCHSIA)

class PersistentCacheTest : public testing::Test,
                            public testing::WithParamInterface<BackendType> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_storage_.emplace(GetParam(), temp_dir_.GetPath());
  }

  // Returns the cache base name for a new cache and the new cache itself.
  std::pair<base::FilePath, std::unique_ptr<PersistentCache>> OpenCache(
      bool single_connection = false,
      bool journal_mode_wal = false) {
    auto [cache_name, pending_backend] =
        MakePendingBackend(single_connection, journal_mode_wal);
    auto cache = PersistentCache::Bind(*std::move(pending_backend));
    if (!cache) {
      ADD_FAILURE() << "Failed to bind PersistentCache";
      return {};
    }
    return {std::move(cache_name), std::move(cache)};
  }

  // Returns the cache base name for a new cache and a pending backend for it.
  std::pair<base::FilePath, std::optional<PendingBackend>> MakePendingBackend(
      bool single_connection,
      bool journal_mode_wal) {
    auto cache_name = base::FilePath::FromASCII(
        base::StrCat({"Cache", base::NumberToString(next_backend_index_++)}));
    auto pending_backend = backend_storage_->MakePendingBackend(
        cache_name, single_connection, journal_mode_wal);
    if (!pending_backend) {
      ADD_FAILURE() << "Failed to make PendingBackend";
      return {};
    }
    return {std::move(cache_name), std::move(pending_backend)};
  }

  BackendStorage& backend_storage() { return *backend_storage_; }

 private:
  base::ScopedTempDir temp_dir_;
  std::optional<BackendStorage> backend_storage_;
  int next_backend_index_ = 0;
};

TEST_P(PersistentCacheTest, FindReturnsNullWhenEmpty) {
  auto [cache_name, cache] = OpenCache();
  EXPECT_THAT(cache->Find(kKey, [](size_t) { return base::span<uint8_t>(); }),
              ValueIs(Eq(std::nullopt)));
}

TEST_P(PersistentCacheTest, FindReturnsValueWhenPresent) {
  auto [cache_name, cache] = OpenCache();
  for (int i = 0; i < 20; ++i) {
    std::string key = base::NumberToString(i);
    auto value = base::as_byte_span(key);

    EXPECT_THAT(FindEntry(*cache, key), ValueIs(Eq(std::nullopt)));

    EXPECT_THAT(cache->Insert(key, value), HasValue());
    ASSERT_THAT(FindEntry(*cache, key), ValueIs(Optional(ContentEq(value))));
  }
}

TEST_P(PersistentCacheTest, EmptyValueIsStorable) {
  auto [cache_name, cache] = OpenCache();
  EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("")),
              HasValue());
  ASSERT_THAT(FindEntry(*cache, kKey),
              ValueIs(Optional(ContentEq(base::span<uint8_t>()))));
}

TEST_P(PersistentCacheTest, ValueContainingNullCharIsStorable) {
  auto [cache_name, cache] = OpenCache();
  constexpr std::array<std::uint8_t, 5> value_array{'\0', 'a', 'b', 'c', '\0'};
  const base::span<const std::uint8_t> value_span(value_array);
  CHECK_EQ(value_span.size(), value_array.size())
      << "All characters must be included in span";

  EXPECT_THAT(cache->Insert(kKey, value_span), HasValue());
  ASSERT_THAT(FindEntry(*cache, kKey),
              ValueIs(Optional(ContentEq(value_span))));
}

TEST_P(PersistentCacheTest, ValueContainingInvalidUtf8IsStorable) {
  auto [cache_name, cache] = OpenCache();
  constexpr std::array<std::uint8_t, 4> value_array{0x20, 0x0F, 0xFF, 0xFF};
  const base::span<const std::uint8_t> value_span(value_array);
  CHECK(
      !base::IsStringUTF8(std::string(value_array.begin(), value_array.end())))
      << "Test needs invalid utf8";

  EXPECT_THAT(cache->Insert(kKey, value_span), HasValue());
  ASSERT_THAT(FindEntry(*cache, kKey),
              ValueIs(Optional(ContentEq(value_span))));
}

TEST_P(PersistentCacheTest, OverwritingChangesValue) {
  auto [cache_name, cache] = OpenCache();
  EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1")),
              HasValue());
  EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("2")),
              HasValue());

  ASSERT_THAT(FindEntry(*cache, kKey),
              ValueIs(Optional(ContentEq(base::byte_span_from_cstring("2")))));
}

TEST_P(PersistentCacheTest, OverwritingChangesValueVaryingSizes) {
  auto [cache_name, cache] = OpenCache();
  EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1")),
              HasValue());
  EXPECT_THAT(
      cache->Insert(kKey, base::as_byte_span(std::string(1024 * 7, 'b'))),
      HasValue());

  ASSERT_THAT(FindEntry(*cache, kKey),
              ValueIs(Optional(
                  ContentEq(base::as_byte_span(std::string(1024 * 7, 'b'))))));
}

TEST_P(PersistentCacheTest, MetadataIsRetrievable) {
  EntryMetadata metadata{.input_signature =
                             base::Time::Now().InMillisecondsSinceUnixEpoch()};

  auto [cache_name, cache] = OpenCache();

  int64_t seconds_since_epoch =
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000;

  EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1"), metadata),
              HasValue());

  EXPECT_THAT(FindEntry(*cache, kKey),
              ValueIs(Optional(Field(
                  &Entry::metadata,
                  AllOf(Field(&EntryMetadata::input_signature,
                              metadata.input_signature),
                        Field(&EntryMetadata::write_timestamp,
                              AllOf(Ge(seconds_since_epoch),
                                    // The test is supposed to time out before
                                    // it takes this long to insert a value.
                                    Le(seconds_since_epoch + 30))))))));
}

TEST_P(PersistentCacheTest, OverwritingChangesMetadata) {
  EntryMetadata metadata{.input_signature =
                             base::Time::Now().InMillisecondsSinceUnixEpoch()};

  auto [cache_name, cache] = OpenCache();
  EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1"), metadata),
              HasValue());

  EXPECT_THAT(FindEntry(*cache, kKey),
              ValueIs(Optional(
                  Field(&Entry::metadata, Field(&EntryMetadata::input_signature,
                                                metadata.input_signature)))));

  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      HasValue());

  EXPECT_THAT(
      FindEntry(*cache, kKey),
      ValueIs(Optional(
          Field(&Entry::metadata, Field(&EntryMetadata::input_signature, 0)))));
}

TEST_P(PersistentCacheTest, MultipleEphemeralCachesAreIndependent) {
  for (int i = 0; i < 3; ++i) {
    auto [cache_name, cache] = OpenCache();

    // `kKey` never inserted in this cache so not found.
    EXPECT_THAT(FindEntry(*cache, kKey), ValueIs(Eq(std::nullopt)));

    EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1")),
                HasValue());

    // `kKey` now present.
    EXPECT_THAT(FindEntry(*cache, kKey), HasValue());
  }
}

TEST_P(PersistentCacheTest, MultipleLiveCachesAreIndependent) {
  std::vector<std::unique_ptr<PersistentCache>> caches;
  for (int i = 0; i < 3; ++i) {
    auto [cache_name, new_cache] = OpenCache();
    caches.push_back(std::move(new_cache));
    std::unique_ptr<PersistentCache>& cache = caches.back();

    // `kKey` never inserted in this cache so not found.
    EXPECT_THAT(FindEntry(*cache, kKey), ValueIs(Eq(std::nullopt)));

    EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1")),
                HasValue());
    // `kKey` now present.
    EXPECT_THAT(FindEntry(*cache, kKey), ValueIs(Ne(std::nullopt)));
  }
}

TEST_P(PersistentCacheTest, EphemeralCachesSharingParamsShareData) {
  auto [cache_name, main_cache] = OpenCache();
  ASSERT_TRUE(main_cache);
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK_AND_ASSIGN(
        auto pending_backend,
        backend_storage().ShareReadWriteConnection(cache_name, *main_cache));
    auto cache = PersistentCache::Bind(std::move(pending_backend));
    ASSERT_TRUE(cache);

    // First run, setup.
    if (i == 0) {
      // `kKey` never inserted so not found.
      EXPECT_THAT(FindEntry(*cache, kKey), ValueIs(Eq(std::nullopt)));

      EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1")),
                  HasValue());

      // `kKey` now present.
      EXPECT_THAT(FindEntry(*cache, kKey), ValueIs(Ne(std::nullopt)));
    } else {
      // `kKey` is present because data is shared.
      EXPECT_THAT(FindEntry(*cache, kKey), ValueIs(Ne(std::nullopt)));
    }
  }
}

TEST_P(PersistentCacheTest, LiveCachesSharingParamsShareData) {
  auto [cache_name, main_cache] = OpenCache();
  std::vector<std::unique_ptr<PersistentCache>> caches;

  for (int i = 0; i < 3; ++i) {
    ASSERT_OK_AND_ASSIGN(
        auto pending_backend,
        backend_storage().ShareReadWriteConnection(cache_name, *main_cache));
    caches.push_back(PersistentCache::Bind(std::move(pending_backend)));
    std::unique_ptr<PersistentCache>& cache = caches.back();
    ASSERT_TRUE(cache);

    // First run, setup.
    if (i == 0) {
      // `kKey` never inserted so not found.
      EXPECT_THAT(FindEntry(*cache, kKey), ValueIs(Eq(std::nullopt)));

      EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1")),
                  HasValue());

      // `kKey` now present.
      EXPECT_THAT(FindEntry(*cache, kKey), ValueIs(Ne(std::nullopt)));
    } else {
      EXPECT_THAT(FindEntry(*cache, kKey), ValueIs(Ne(std::nullopt)));
    }
  }
}

// Create an instance and share it for read-only access to others.
TEST_P(PersistentCacheTest, MultipleInstancesShareData) {
  // The main read-write instance.
  auto [cache_name, main_cache] = OpenCache();

  std::vector<std::unique_ptr<PersistentCache>> caches;
  for (int i = 0; i < 3; ++i) {
    // Export a read-only view to the main instance.
    ASSERT_OK_AND_ASSIGN(
        auto pending_backend,
        backend_storage().ShareReadOnlyConnection(cache_name, *main_cache));
    // Create a new instance that will read from the original.
    caches.push_back(PersistentCache::Bind(std::move(pending_backend)));
    std::unique_ptr<PersistentCache>& ro_cache = caches.back();
    ASSERT_TRUE(ro_cache);

    if (i == 0) {
      // The db is empty when the first client connects.
      EXPECT_THAT(FindEntry(*ro_cache, kKey), ValueIs(Eq(std::nullopt)));

      // Insert a value via the read-write instance.
      EXPECT_THAT(main_cache->Insert(kKey, base::byte_span_from_cstring("1")),
                  HasValue());

      // It should be there.
      EXPECT_THAT(FindEntry(*ro_cache, kKey), ValueIs(Ne(std::nullopt)));
    }

    // The new read-only client should see the value that was previously
    // inserted.
    EXPECT_THAT(FindEntry(*ro_cache, kKey), ValueIs(Ne(std::nullopt)));
  }
}

// Create an instance and share it for read-write access to others.
TEST_P(PersistentCacheTest, MultipleInstancesCanWriteData) {
  static constexpr char kThisKeyPrefix[] = "thiskey-";
  static constexpr char kOtherKeyPrefix[] = "otherkey-";

  // The main read-write instance.
  auto [cache_name, main_cache] = OpenCache();

  std::vector<std::unique_ptr<PersistentCache>> caches;
  for (int i = 0; i < 3; ++i) {
    // Share a read-write view to the main instance.
    ASSERT_OK_AND_ASSIGN(
        auto pending_backend,
        backend_storage().ShareReadWriteConnection(cache_name, *main_cache));
    // Create a new instance that will read/write from/to the original.
    caches.push_back(PersistentCache::Bind(std::move(pending_backend)));
    std::unique_ptr<PersistentCache>& rw_cache = caches.back();
    ASSERT_TRUE(rw_cache);

    // This new cache has access to all previous values.
    for (int j = 0; j < i; ++j) {
      std::string value = base::NumberToString(j);

      EXPECT_THAT(FindEntry(*rw_cache, base::StrCat({kThisKeyPrefix, value})),
                  ValueIs(Ne(std::nullopt)));

      EXPECT_THAT(FindEntry(*rw_cache, base::StrCat({kOtherKeyPrefix, value})),
                  ValueIs(Ne(std::nullopt)));
    }

    // A new value added from the original is seen here.
    std::string value = base::NumberToString(i);
    std::string other_key = base::StrCat({kOtherKeyPrefix, value});

    EXPECT_THAT(FindEntry(*main_cache, other_key), ValueIs(Eq(std::nullopt)));
    EXPECT_THAT(FindEntry(*rw_cache, other_key), ValueIs(Eq(std::nullopt)));

    EXPECT_THAT(main_cache->Insert(other_key, base::as_byte_span(value)),
                HasValue());

    EXPECT_THAT(FindEntry(*main_cache, other_key), ValueIs(Ne(std::nullopt)));
    EXPECT_THAT(FindEntry(*rw_cache, other_key), ValueIs(Ne(std::nullopt)));

    // A new value added here is seen in the original.
    std::string this_key = base::StrCat({kThisKeyPrefix, value});

    EXPECT_THAT(FindEntry(*main_cache, this_key), ValueIs(Eq(std::nullopt)));
    EXPECT_THAT(FindEntry(*rw_cache, this_key), ValueIs(Eq(std::nullopt)));

    EXPECT_THAT(rw_cache->Insert(this_key, base::as_byte_span(value)),
                HasValue());

    EXPECT_THAT(FindEntry(*main_cache, this_key), ValueIs(Ne(std::nullopt)));
    EXPECT_THAT(FindEntry(*rw_cache, this_key), ValueIs(Ne(std::nullopt)));
  }
}

TEST_P(PersistentCacheTest, ThreadSafeAccess) {
  base::test::TaskEnvironment env;

  // Create the cache and insert on this sequence.
  auto value = base::byte_span_from_cstring("1");
  auto [cache_name, main_cache] = OpenCache();
  EXPECT_THAT(main_cache->Insert(kKey, value), HasValue());

  // FindEntry() on ThreadPool. Result should be expected and there are no
  // sequence checkers tripped.
  base::test::TestFuture<base::expected<std::optional<Entry>, TransactionError>>
      future_entry;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](PersistentCache* cache,
             base::OnceCallback<void(
                 base::expected<std::optional<Entry>, TransactionError>)>
                 on_entry) {
            auto entry = FindEntry(*cache, kKey);
            std::move(on_entry).Run(std::move(entry));
          },
          main_cache.get(), future_entry.GetSequenceBoundCallback()));

  // Wait for result availability and check.
  ASSERT_OK_AND_ASSIGN(std::optional<Entry> entry, future_entry.Take());
  ASSERT_THAT(entry, Optional(ContentEq(value)));
}

TEST_P(PersistentCacheTest, MultipleLiveEntries) {
  auto [cache_name, cache] = OpenCache();
  absl::flat_hash_map<std::string, std::optional<Entry>> entries;

  for (size_t i = 0; i < 20; ++i) {
    std::string key = base::NumberToString(i);
    auto value = base::as_byte_span(key);
    EXPECT_THAT(cache->Insert(key, value), HasValue());
    // Create an entry where the value is equal to the key.
    ASSERT_OK_AND_ASSIGN(entries[key], FindEntry(*cache, key));
  }

  // Verify that entries have the expected content.
  for (auto& [key, entry] : entries) {
    ASSERT_THAT(entry, Optional(ContentEq(base::as_byte_span(key))));
  }
}

TEST_P(PersistentCacheTest, MultipleLiveEntriesWithVaryingLifetime) {
  static constexpr size_t kNumberOfEntries = 40;

  auto [cache_name, cache] = OpenCache();
  absl::flat_hash_map<std::string, std::optional<Entry>> entries;

  for (size_t i = 0; i < kNumberOfEntries; ++i) {
    std::string key = base::NumberToString(i);
    auto value = base::as_byte_span(key);
    EXPECT_THAT(cache->Insert(key, value), HasValue());
    // Create an entry where the value is equal to the key.
    ASSERT_OK_AND_ASSIGN(entries[key], FindEntry(*cache, key));

    // Every other iteration delete an entry that came before.
    if (i && i % 2 == 0) {
      entries.erase(base::NumberToString(i / 2));
    }
  }

  // Assert that some entries remain to be verified in the next loop.
  ASSERT_GE(entries.size(), kNumberOfEntries / 2);

  // Verify that entries have the expected content.
  for (auto& [key, entry] : entries) {
    ASSERT_THAT(entry, Optional(ContentEq(base::as_byte_span(key))));
  }
}

TEST_P(PersistentCacheTest, AbandonementDetected) {
  auto [cache_name, cache] = OpenCache();

  // Value is correctly inserted.
  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      HasValue());
  ASSERT_OK_AND_ASSIGN(auto entry, FindEntry(*cache, kKey));
  EXPECT_NE(entry, std::nullopt);

  // Abandon cache, no further operations will succeed.
  EXPECT_EQ(cache->Abandon(), LockState::kNotHeld);

  // Calling FindEntry() is no longer successful.
  EXPECT_THAT(FindEntry(*cache, kKey),
              ErrorIs(TransactionError::kConnectionError));

  // Calling Insert() is no longer successful.
  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      ErrorIs(TransactionError::kConnectionError));
}

TEST_P(PersistentCacheTest, RecoveryFromTransientError) {
  auto [cache_name, cache] = OpenCache();

  ASSERT_OK_AND_ASSIGN(
      auto pending_reader,
      backend_storage().ShareReadOnlyConnection(cache_name, *cache));

  // Baseline insert works.
  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      HasValue());

  // Lock the db file in shared mode.
  ASSERT_OK_AND_ASSIGN(
      auto reader_vfs_file_set,
      SqliteBackendImpl::BindToFileSet(std::move(pending_reader)));
  SandboxedFile* reader_db_file = reader_vfs_file_set.GetSandboxedDbFile();
  reader_db_file->OnFileOpened(
      reader_db_file->TakeUnderlyingFile(SandboxedFile::FileType::kMainDb));
  ASSERT_EQ(reader_db_file->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);

  // Held lock causes transient error.
  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      ErrorIs(TransactionError::kTransient));

  // Unlock works.
  ASSERT_EQ(reader_db_file->Unlock(SQLITE_LOCK_NONE), SQLITE_OK);
  ASSERT_EQ(reader_db_file->LockModeForTesting(), SQLITE_LOCK_NONE);
  reader_db_file->Close();

  // Insert now succeeds.
  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      HasValue());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PersistentCacheTest,
                         testing::Values(BackendType::kSqlite));
#endif

}  // namespace persistent_cache
