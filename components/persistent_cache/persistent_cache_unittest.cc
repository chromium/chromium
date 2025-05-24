// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include <cstdint>
#include <memory>

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/mock/mock_backend_impl.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/persistent_cache/sqlite/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr const char* kKey = "foo";

using testing::_;
using testing::Return;

class PersistentCacheMockedBackendTest : public testing::Test {
 protected:
  void SetUp() override {
    backend_ = std::make_unique<persistent_cache::MockBackendImpl>(params_);
  }

  void CreateCache(bool successful) {
    EXPECT_CALL(*backend_, Initialize()).WillOnce(Return(successful));
    cache_ = std::make_unique<persistent_cache::PersistentCache>(
        std::move(backend_));
  }

  persistent_cache::MockBackendImpl* GetBackend() {
    // Can't be called without a cache.
    CHECK(cache_);
    return static_cast<persistent_cache::MockBackendImpl*>(
        cache_->GetBackendForTesting());
  }

  persistent_cache::BackendParams params_;
  std::unique_ptr<persistent_cache::MockBackendImpl> backend_;
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
  EXPECT_CALL(*GetBackend(), Find(kKey)).WillOnce(Return(nullptr));
  cache_->Find(kKey);
}

TEST_F(PersistentCacheMockedBackendTest, CacheInsertCallsBackendInsert) {
  CreateCache(true);
  EXPECT_CALL(*GetBackend(), Insert(kKey, _, _));
  cache_->Insert(kKey, base::byte_span_from_cstring("1"));
}

TEST_F(PersistentCacheMockedBackendTest,
       FailedBackendInitializationMeansNoFurtherCalls) {
  EXPECT_CALL(*backend_, Insert(kKey, _, _)).Times(0);
  EXPECT_CALL(*backend_, Find(kKey)).Times(0);

  CreateCache(false);
  cache_->Insert(kKey, base::byte_span_from_cstring("1"));
  cache_->Find(kKey);
}

#if !BUILDFLAG(IS_FUCHSIA)

class PersistentCacheTest : public testing::Test,
                            public testing::WithParamInterface<BackendType> {
 protected:
  // Used to creates a new cache independent from any other.
  std::unique_ptr<PersistentCache> OpenCache() {
    auto cache = PersistentCache::Open(
        (params_provider_.CreateBackendFilesAndBuildParams(GetParam())));
    CHECK(cache->GetBackendForTesting());
    return cache;
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
  EXPECT_EQ(cache->Find(kKey), nullptr);
}

TEST_P(PersistentCacheTest, FindReturnsValueWhenPresent) {
  auto cache = OpenCache();
  for (int i = 0; i < 20; ++i) {
    std::string key = base::NumberToString(i);
    auto value = base::as_byte_span(key);
    EXPECT_EQ(cache->Find(key), nullptr);
    cache->Insert(key, value);
    std::unique_ptr<Entry> entry = cache->Find(key);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->GetContentSpan(), value);
  }
}

TEST_P(PersistentCacheTest, EmptyValueIsStorable) {
  auto cache = OpenCache();
  cache->Insert(kKey, base::byte_span_from_cstring(""));
  EXPECT_EQ(cache->Find(kKey)->GetContentSpan(),
            base::span<const std::uint8_t>{});
}

TEST_P(PersistentCacheTest, ValueContainingNullCharIsStorable) {
  auto cache = OpenCache();
  constexpr std::array<std::uint8_t, 5> value_array{'\0', 'a', 'b', 'c', '\0'};
  const base::span<const std::uint8_t> value_span(value_array);
  CHECK_EQ(value_span.size(), value_array.size())
      << "All characters must be included in span";

  cache->Insert(kKey, value_span);
  EXPECT_EQ(cache->Find(kKey)->GetContentSpan(), value_span);
}

TEST_P(PersistentCacheTest, ValueContainingInvalidUtf8IsStorable) {
  auto cache = OpenCache();
  constexpr std::array<std::uint8_t, 4> value_array{0x20, 0x0F, 0xFF, 0xFF};
  const base::span<const std::uint8_t> value_span(value_array);
  CHECK(
      !base::IsStringUTF8(std::string(value_array.begin(), value_array.end())))
      << "Test needs invalid utf8";

  cache->Insert(kKey, value_span);
  EXPECT_EQ(cache->Find(kKey)->GetContentSpan(), value_span);
}

TEST_P(PersistentCacheTest, OverwritingChangesValue) {
  auto cache = OpenCache();
  cache->Insert(kKey, base::byte_span_from_cstring("1"));
  cache->Insert(kKey, base::byte_span_from_cstring("2"));
  EXPECT_EQ(cache->Find(kKey)->GetContentSpan(),
            base::byte_span_from_cstring("2"));
}

TEST_P(PersistentCacheTest, MetadataIsRetrievable) {
  EntryMetadata metadata{.input_signature =
                             base::Time::Now().InMillisecondsSinceUnixEpoch()};

  auto cache = OpenCache();
  cache->Insert(kKey, base::byte_span_from_cstring("1"), metadata);

  auto entry = cache->Find(kKey);
  EXPECT_EQ(entry->GetMetadata().input_signature, metadata.input_signature);
  EXPECT_NE(entry->GetMetadata().write_timestamp, 0);
}

TEST_P(PersistentCacheTest, OverwritingChangesMetadata) {
  EntryMetadata metadata{.input_signature =
                             base::Time::Now().InMillisecondsSinceUnixEpoch()};

  auto cache = OpenCache();
  cache->Insert(kKey, base::byte_span_from_cstring("1"), metadata);
  EXPECT_EQ(cache->Find(kKey)->GetMetadata().input_signature,
            metadata.input_signature);

  cache->Insert(kKey, base::byte_span_from_cstring("1"), EntryMetadata{});
  EXPECT_EQ(cache->Find(kKey)->GetMetadata().input_signature, 0);
}

TEST_P(PersistentCacheTest, MultipleEphemeralCachesAreIndependent) {
  for (int i = 0; i < 3; ++i) {
    auto cache = OpenCache();

    // `kKey` never inserted in this cache so not found.
    EXPECT_EQ(cache->Find(kKey), nullptr);
    cache->Insert(kKey, base::byte_span_from_cstring("1"));
    // `kKey` now present.
    EXPECT_NE(cache->Find(kKey), nullptr);
  }
}

TEST_P(PersistentCacheTest, MultipleLiveCachesAreIndependent) {
  std::vector<std::unique_ptr<PersistentCache>> caches;
  for (int i = 0; i < 3; ++i) {
    caches.push_back(OpenCache());
    std::unique_ptr<PersistentCache>& cache = caches.back();

    // `kKey` never inserted in this cache so not found.
    EXPECT_EQ(cache->Find(kKey), nullptr);
    cache->Insert(kKey, base::byte_span_from_cstring("1"));
    // `kKey` now present.
    EXPECT_NE(cache->Find(kKey), nullptr);
  }
}

TEST_P(PersistentCacheTest, EphemeralCachesSharingParamsShareData) {
  BackendParams backend_params =
      params_provider_.CreateBackendFilesAndBuildParams(GetParam());
  for (int i = 0; i < 3; ++i) {
    auto cache = OpenCache(backend_params.Copy());

    // First run, setup.
    if (i == 0) {
      // `kKey` never inserted so not found.
      EXPECT_EQ(cache->Find(kKey), nullptr);
      cache->Insert(kKey, base::byte_span_from_cstring("1"));
      // `kKey` now present.
      EXPECT_NE(cache->Find(kKey), nullptr);
    } else {
      // `kKey` is present because data is shared.
      EXPECT_NE(cache->Find(kKey), nullptr);
    }
  }
}

TEST_P(PersistentCacheTest, LiveCachesSharingParamsShareData) {
  BackendParams backend_params =
      params_provider_.CreateBackendFilesAndBuildParams(GetParam());
  std::vector<std::unique_ptr<PersistentCache>> caches;

  for (int i = 0; i < 3; ++i) {
    caches.push_back(OpenCache(backend_params.Copy()));
    std::unique_ptr<PersistentCache>& cache = caches.back();

    // First run, setup.
    if (i == 0) {
      // `kKey` never inserted so not found.
      EXPECT_EQ(cache->Find(kKey), nullptr);
      cache->Insert(kKey, base::byte_span_from_cstring("1"));
      // `kKey` now present.
      EXPECT_NE(cache->Find(kKey), nullptr);
    } else {
      // `kKey` is present because data is shared.
      EXPECT_NE(cache->Find(kKey), nullptr);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         PersistentCacheTest,
                         testing::Values(BackendType::kSqlite));
#endif

}  // namespace persistent_cache
