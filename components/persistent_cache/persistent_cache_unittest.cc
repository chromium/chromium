// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
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
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/mock/mock_backend.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/persistent_cache/sqlite/test_utils.h"
#include "components/persistent_cache/test_utils.h"
#include "components/persistent_cache/transaction_error.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {

constexpr const char* kKey = "foo";

using testing::_;
using testing::Return;

class PersistentCacheMockedBackendTest : public testing::Test {
 protected:
  void SetUp() override {
    backend_ = std::make_unique<persistent_cache::MockBackend>();
  }

  void CreateCache(bool successful) {
    EXPECT_CALL(*backend_, Initialize()).WillOnce(Return(successful));
    cache_ = std::make_unique<persistent_cache::PersistentCache>(
        std::move(backend_));
  }

  persistent_cache::MockBackend* GetBackend() {
    // Can't be called without a cache.
    CHECK(cache_);
    return static_cast<persistent_cache::MockBackend*>(
        cache_->GetBackendForTesting());
  }

  persistent_cache::BackendParams params_;
  std::unique_ptr<persistent_cache::MockBackend> backend_;
  std::unique_ptr<persistent_cache::PersistentCache> cache_;
};

}  // namespace

namespace persistent_cache {

TEST_F(PersistentCacheMockedBackendTest, CreatingCacheInitializesBackend) {
  EXPECT_CALL(*backend_, Initialize()).WillOnce(Return(true));

  std::unique_ptr<PersistentCache> cache =
      std::make_unique<PersistentCache>(std::move(backend_));
  EXPECT_NE(cache->GetBackendForTesting(), nullptr);
}

TEST_F(PersistentCacheMockedBackendTest, CacheFindCallsBackendFind) {
  CreateCache(true);
  EXPECT_CALL(*GetBackend(), Find(kKey)).WillOnce(Return(base::ok(nullptr)));

  EXPECT_THAT(cache_->Find(kKey), base::test::ValueIs(testing::IsNull()));
}

TEST_F(PersistentCacheMockedBackendTest, FindReturnsBackendError) {
  CreateCache(true);
  EXPECT_CALL(*GetBackend(), Find(kKey))
      .WillOnce(Return(base::unexpected(TransactionError::kTransient)));
  EXPECT_THAT(cache_->Find(kKey),
              base::test::ErrorIs(TransactionError::kTransient));
}

TEST_F(PersistentCacheMockedBackendTest, InsertReturnsBackendError) {
  CreateCache(true);
  EXPECT_CALL(*GetBackend(), Insert(_, _, _))
      .WillOnce(Return(base::unexpected(TransactionError::kTransient)));
  EXPECT_THAT(cache_->Insert(kKey, base::byte_span_from_cstring("1")),
              base::test::ErrorIs(TransactionError::kTransient));
}

TEST_F(PersistentCacheMockedBackendTest, CacheInsertCallsBackendInsert) {
  CreateCache(true);
  EXPECT_CALL(*GetBackend(), Insert(kKey, _, _));
  EXPECT_THAT(cache_->Insert(kKey, base::byte_span_from_cstring("1")),
              base::test::HasValue());
}

TEST_F(PersistentCacheMockedBackendTest,
       FailedBackendInitializationMeansNoFurtherCalls) {
  EXPECT_CALL(*backend_, Insert(kKey, _, _)).Times(0);
  EXPECT_CALL(*backend_, Find(kKey)).Times(0);

  CreateCache(false);
  EXPECT_THAT(cache_->Insert(kKey, base::byte_span_from_cstring("1")),
              base::test::ErrorIs(TransactionError::kPermanent));
  EXPECT_THAT(cache_->Find(kKey),
              base::test::ErrorIs(TransactionError::kPermanent));
}

#if !BUILDFLAG(IS_FUCHSIA)

class PersistentCacheTest : public testing::Test,
                            public testing::WithParamInterface<BackendType> {
 protected:
  // Used to creates a new cache independent from any other.
  std::unique_ptr<PersistentCache> OpenCache() {
    auto backend = params_provider_.CreateBackendWithFiles(GetParam());
    if (!backend) {
      ADD_FAILURE() << "Failed to create backend";
      return nullptr;
    }
    return std::make_unique<PersistentCache>(std::move(backend));
  }

  // Used to create a new cache with provided params. Use with params copied
  // from the creation of another cache to share backing files between the two.
  std::unique_ptr<PersistentCache> OpenCache(BackendParams backend_params) {
    auto cache = PersistentCache::Open(std::move(backend_params));
    CHECK(cache->GetBackendForTesting());
    return cache;
  }

  persistent_cache::test_utils::TestHelper params_provider_;
};

TEST_P(PersistentCacheTest, FindReturnsNullWhenEmpty) {
  auto cache = OpenCache();
  EXPECT_THAT(cache->Find(kKey), base::test::ValueIs(testing::IsNull()));
}

TEST_P(PersistentCacheTest, FindReturnsValueWhenPresent) {
  auto cache = OpenCache();
  for (int i = 0; i < 20; ++i) {
    std::string key = base::NumberToString(i);
    auto value = base::as_byte_span(key);

    EXPECT_THAT(cache->Find(key), base::test::ValueIs(testing::IsNull()));

    EXPECT_THAT(cache->Insert(key, value), base::test::HasValue());
    ASSERT_THAT(cache->Find(key), base::test::ValueIs(HasContents(value)));
  }
}

TEST_P(PersistentCacheTest, EmptyValueIsStorable) {
  auto cache = OpenCache();
  EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("")),
              base::test::HasValue());
  ASSERT_THAT(
      cache->Find(kKey),
      base::test::ValueIs(HasContents(base::span<const std::uint8_t>{})));
}

TEST_P(PersistentCacheTest, ValueContainingNullCharIsStorable) {
  auto cache = OpenCache();
  constexpr std::array<std::uint8_t, 5> value_array{'\0', 'a', 'b', 'c', '\0'};
  const base::span<const std::uint8_t> value_span(value_array);
  CHECK_EQ(value_span.size(), value_array.size())
      << "All characters must be included in span";

  EXPECT_THAT(cache->Insert(kKey, value_span), base::test::HasValue());
  ASSERT_THAT(cache->Find(kKey), base::test::ValueIs(HasContents(value_span)));
}

TEST_P(PersistentCacheTest, ValueContainingInvalidUtf8IsStorable) {
  auto cache = OpenCache();
  constexpr std::array<std::uint8_t, 4> value_array{0x20, 0x0F, 0xFF, 0xFF};
  const base::span<const std::uint8_t> value_span(value_array);
  CHECK(
      !base::IsStringUTF8(std::string(value_array.begin(), value_array.end())))
      << "Test needs invalid utf8";

  EXPECT_THAT(cache->Insert(kKey, value_span), base::test::HasValue());
  ASSERT_THAT(cache->Find(kKey), base::test::ValueIs(HasContents(value_span)));
}

TEST_P(PersistentCacheTest, OverwritingChangesValue) {
  auto cache = OpenCache();
  EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1")),
              base::test::HasValue());
  EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("2")),
              base::test::HasValue());

  ASSERT_THAT(
      cache->Find(kKey),
      base::test::ValueIs(HasContents(base::byte_span_from_cstring("2"))));
}

TEST_P(PersistentCacheTest, MetadataIsRetrievable) {
  EntryMetadata metadata{.input_signature =
                             base::Time::Now().InMillisecondsSinceUnixEpoch()};

  auto cache = OpenCache();
  EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1"), metadata),
              base::test::HasValue());

  int64_t seconds_since_epoch =
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000;

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Entry> entry, cache->Find(kKey));
  EXPECT_EQ(entry->GetMetadata().input_signature, metadata.input_signature);
  EXPECT_GE(entry->GetMetadata().write_timestamp, seconds_since_epoch);
  // The test is supposed to time out before it takes this long to insert a
  // value.
  EXPECT_LE(entry->GetMetadata().write_timestamp, seconds_since_epoch + 30);
}

TEST_P(PersistentCacheTest, OverwritingChangesMetadata) {
  EntryMetadata metadata{.input_signature =
                             base::Time::Now().InMillisecondsSinceUnixEpoch()};

  auto cache = OpenCache();
  EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1"), metadata),
              base::test::HasValue());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Entry> entry, cache->Find(kKey));
  EXPECT_EQ(entry->GetMetadata().input_signature, metadata.input_signature);

  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      base::test::HasValue());

  ASSERT_OK_AND_ASSIGN(entry, cache->Find(kKey));
  EXPECT_EQ(entry->GetMetadata().input_signature, 0);
}

TEST_P(PersistentCacheTest, MultipleEphemeralCachesAreIndependent) {
  for (int i = 0; i < 3; ++i) {
    auto cache = OpenCache();

    // `kKey` never inserted in this cache so not found.
    EXPECT_THAT(cache->Find(kKey), base::test::ValueIs(testing::IsNull()));

    EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1")),
                base::test::HasValue());

    // `kKey` now present.
    EXPECT_THAT(cache->Find(kKey), base::test::HasValue());
  }
}

TEST_P(PersistentCacheTest, MultipleLiveCachesAreIndependent) {
  std::vector<std::unique_ptr<PersistentCache>> caches;
  for (int i = 0; i < 3; ++i) {
    caches.push_back(OpenCache());
    std::unique_ptr<PersistentCache>& cache = caches.back();

    // `kKey` never inserted in this cache so not found.
    EXPECT_THAT(cache->Find(kKey), base::test::ValueIs(testing::IsNull()));

    EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1")),
                base::test::HasValue());
    // `kKey` now present.
    EXPECT_THAT(cache->Find(kKey), base::test::ValueIs(testing::NotNull()));
  }
}

TEST_P(PersistentCacheTest, EphemeralCachesSharingParamsShareData) {
  std::unique_ptr<Backend> backend =
      params_provider_.CreateBackendWithFiles(GetParam());
  ASSERT_TRUE(backend);
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK_AND_ASSIGN(auto params, backend->ExportReadWriteParams());
    auto cache = OpenCache(std::move(params));

    // First run, setup.
    if (i == 0) {
      // `kKey` never inserted so not found.
      EXPECT_THAT(cache->Find(kKey), base::test::ValueIs(testing::IsNull()));

      EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1")),
                  base::test::HasValue());

      // `kKey` now present.
      EXPECT_THAT(cache->Find(kKey), base::test::ValueIs(testing::NotNull()));
    } else {
      // `kKey` is present because data is shared.
      EXPECT_THAT(cache->Find(kKey), base::test::ValueIs(testing::NotNull()));
    }
  }
}

TEST_P(PersistentCacheTest, LiveCachesSharingParamsShareData) {
  std::unique_ptr<Backend> backend =
      params_provider_.CreateBackendWithFiles(GetParam());
  std::vector<std::unique_ptr<PersistentCache>> caches;

  for (int i = 0; i < 3; ++i) {
    ASSERT_OK_AND_ASSIGN(auto params, backend->ExportReadWriteParams());
    caches.push_back(OpenCache(std::move(params)));
    std::unique_ptr<PersistentCache>& cache = caches.back();

    // First run, setup.
    if (i == 0) {
      // `kKey` never inserted so not found.
      EXPECT_THAT(cache->Find(kKey), base::test::ValueIs(testing::IsNull()));

      EXPECT_THAT(cache->Insert(kKey, base::byte_span_from_cstring("1")),
                  base::test::HasValue());

      // `kKey` now present.
      EXPECT_THAT(cache->Find(kKey), base::test::ValueIs(testing::NotNull()));
    } else {
      EXPECT_THAT(cache->Find(kKey), base::test::ValueIs(testing::NotNull()));
    }
  }
}

// Create an instance and share it for read-only access to others.
TEST_P(PersistentCacheTest, MultipleInstancesShareData) {
  // The main read-write instance.
  auto main_cache = OpenCache();

  std::vector<std::unique_ptr<PersistentCache>> caches;
  for (int i = 0; i < 3; ++i) {
    // Export a read-only view to the main instance.
    ASSERT_OK_AND_ASSIGN(auto params,
                         main_cache->ExportReadOnlyBackendParams());
    // Create a new instance that will read from the original.
    caches.push_back(OpenCache(std::move(params)));
    std::unique_ptr<PersistentCache>& ro_cache = caches.back();

    if (i == 0) {
      // The db is empty when the first client connects.
      EXPECT_THAT(ro_cache->Find(kKey), base::test::ValueIs(testing::IsNull()));

      // Insert a value via the read-write instance.
      EXPECT_THAT(main_cache->Insert(kKey, base::byte_span_from_cstring("1")),
                  base::test::HasValue());

      // It should be there.
      EXPECT_THAT(ro_cache->Find(kKey),
                  base::test::ValueIs(testing::NotNull()));
    }

    // The new read-only client should see the value that was previously
    // inserted.
    EXPECT_THAT(ro_cache->Find(kKey), base::test::ValueIs(testing::NotNull()));
  }
}

// Create an instance and share it for read-write access to others.
TEST_P(PersistentCacheTest, MultipleInstancesCanWriteData) {
  static constexpr char kThisKeyPrefix[] = "thiskey-";
  static constexpr char kOtherKeyPrefix[] = "otherkey-";

  // The main read-write instance.
  auto main_cache = OpenCache();

  std::vector<std::unique_ptr<PersistentCache>> caches;
  for (int i = 0; i < 3; ++i) {
    // Export a read-only view to the main instance.
    ASSERT_OK_AND_ASSIGN(auto params,
                         main_cache->ExportReadWriteBackendParams());
    // Create a new instance that will read/write from/to the original.
    caches.push_back(OpenCache(std::move(params)));
    std::unique_ptr<PersistentCache>& rw_cache = caches.back();

    // This new cache has access to all previous values.
    for (int j = 0; j < i; ++j) {
      std::string value = base::NumberToString(j);

      EXPECT_THAT(rw_cache->Find(base::StrCat({kThisKeyPrefix, value})),
                  base::test::ValueIs(testing::NotNull()));

      EXPECT_THAT(rw_cache->Find(base::StrCat({kOtherKeyPrefix, value})),
                  base::test::ValueIs(testing::NotNull()));
    }

    // A new value added from the original is seen here.
    std::string value = base::NumberToString(i);
    std::string other_key = base::StrCat({kOtherKeyPrefix, value});

    EXPECT_THAT(main_cache->Find(other_key),
                base::test::ValueIs(testing::IsNull()));
    EXPECT_THAT(rw_cache->Find(other_key),
                base::test::ValueIs(testing::IsNull()));

    EXPECT_THAT(main_cache->Insert(other_key, base::as_byte_span(value)),
                base::test::HasValue());

    EXPECT_THAT(main_cache->Find(other_key),
                base::test::ValueIs(testing::NotNull()));
    EXPECT_THAT(rw_cache->Find(other_key),
                base::test::ValueIs(testing::NotNull()));

    // A new value added here is seen in the original.
    std::string this_key = base::StrCat({kThisKeyPrefix, value});

    EXPECT_THAT(main_cache->Find(this_key),
                base::test::ValueIs(testing::IsNull()));
    EXPECT_THAT(rw_cache->Find(this_key),
                base::test::ValueIs(testing::IsNull()));

    EXPECT_THAT(rw_cache->Insert(this_key, base::as_byte_span(value)),
                base::test::HasValue());

    EXPECT_THAT(main_cache->Find(this_key),
                base::test::ValueIs(testing::NotNull()));
    EXPECT_THAT(rw_cache->Find(this_key),
                base::test::ValueIs(testing::NotNull()));
  }
}

TEST_P(PersistentCacheTest, ThreadSafeAccess) {
  base::test::TaskEnvironment env;

  // Create the cache and insert on this sequence.
  auto value = base::byte_span_from_cstring("1");
  auto cache = OpenCache();
  EXPECT_THAT(cache->Insert(kKey, value), base::test::HasValue());

  // Find() on ThreadPool. Result should be expected and there are no sequence
  // checkers tripped.
  base::test::TestFuture<
      base::expected<std::unique_ptr<Entry>, TransactionError>>
      future_entry;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](PersistentCache* cache,
             base::OnceCallback<void(
                 base::expected<std::unique_ptr<Entry>, TransactionError>)>
                 on_entry) {
            auto entry = cache->Find(kKey);
            std::move(on_entry).Run(std::move(entry));
          },
          cache.get(), future_entry.GetSequenceBoundCallback()));

  // Wait for result availability and check.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Entry> entry, future_entry.Take());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->GetContentSpan(), value);
}

TEST_P(PersistentCacheTest, MultipleLiveEntries) {
  auto cache = OpenCache();
  absl::flat_hash_map<std::string, std::unique_ptr<Entry>> entries;

  for (size_t i = 0; i < 20; ++i) {
    std::string key = base::NumberToString(i);
    auto value = base::as_byte_span(key);
    EXPECT_THAT(cache->Insert(key, value), base::test::HasValue());
    // Create an entry where the value is equal to the key.
    ASSERT_OK_AND_ASSIGN(entries[key], cache->Find(key));
  }

  // Verify that entries have the expected content.
  for (auto& [key, entry] : entries) {
    ASSERT_NE(entry, nullptr);
    ASSERT_EQ(entry->GetContentSpan(), base::as_byte_span(key));
  }
}

TEST_P(PersistentCacheTest, MultipleLiveEntriesWithVaryingLifetime) {
  static constexpr size_t kNumberOfEntries = 40;

  auto cache = OpenCache();
  absl::flat_hash_map<std::string, std::unique_ptr<Entry>> entries;

  for (size_t i = 0; i < kNumberOfEntries; ++i) {
    std::string key = base::NumberToString(i);
    auto value = base::as_byte_span(key);
    EXPECT_THAT(cache->Insert(key, value), base::test::HasValue());
    // Create an entry where the value is equal to the key.
    ASSERT_OK_AND_ASSIGN(entries[key], cache->Find(key));

    // Every other iteration delete an entry that came before.
    if (i && i % 2 == 0) {
      entries.erase(base::NumberToString(i / 2));
    }
  }

  // Assert that some entries remain to be verified in the next loop.
  ASSERT_GE(entries.size(), kNumberOfEntries / 2);

  // Verify that entries have the expected content.
  for (auto& [key, entry] : entries) {
    ASSERT_NE(entry, nullptr);
    ASSERT_EQ(entry->GetContentSpan(), base::as_byte_span(key));
  }
}

TEST_P(PersistentCacheTest, AbandonementDetected) {
  auto cache = OpenCache();

  // Value is correctly inserted.
  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      base::test::HasValue());
  ASSERT_OK_AND_ASSIGN(auto entry, cache->Find(kKey));
  EXPECT_NE(entry, nullptr);

  // Abandon cache, no further operations will succeed.
  cache->Abandon();

  // Calling Find() is no longer successful.
  EXPECT_THAT(cache->Find(kKey),
              base::test::ErrorIs(TransactionError::kConnectionError));

  // Calling Insert() is no longer successful.
  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      base::test::ErrorIs(TransactionError::kConnectionError));
}

TEST_P(PersistentCacheTest, RecoveryFromTransientError) {
  auto cache = OpenCache();

  ASSERT_OK_AND_ASSIGN(auto reader_params,
                       cache->ExportReadOnlyBackendParams());

  // Baseline insert works.
  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      base::test::HasValue());

  // Lock the db file in shared mode.
  auto reader_vfs_file_set =
      SqliteBackendImpl::GetVfsFileSetFromParams(std::move(reader_params));
  auto reader_files = reader_vfs_file_set.GetFiles();
  auto reader_db_file = reader_files[0].second;
  ASSERT_EQ(reader_db_file->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);

  // Held lock causes transient error.
  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      base::test::ErrorIs(TransactionError::kTransient));

  // Unlock works.
  ASSERT_EQ(reader_db_file->Unlock(SQLITE_LOCK_NONE), SQLITE_OK);
  ASSERT_EQ(reader_db_file->LockModeForTesting(), SQLITE_LOCK_NONE);

  // Insert now succeeds.
  EXPECT_THAT(
      cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{}),
      base::test::HasValue());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PersistentCacheTest,
                         testing::Values(BackendType::kSqlite));
#endif

}  // namespace persistent_cache
