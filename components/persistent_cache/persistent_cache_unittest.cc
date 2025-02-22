// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include <memory>

#include "components/persistent_cache/mock/mock_backend_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr const char* kKey = "foo";

using testing::_;
using testing::Return;

class PersistentCacheTest : public testing::Test {
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

TEST_F(PersistentCacheTest, CreatingCacheInitializesBackend) {
  EXPECT_CALL(*backend_, Initialize()).WillOnce(Return(true));

  std::unique_ptr<PersistentCache> cache =
      std::make_unique<PersistentCache>(std::move(backend_));
  ASSERT_NE(cache->GetBackendForTesting(), nullptr);
}

TEST_F(PersistentCacheTest, CacheFindCallsBackendFind) {
  CreateCache(true);
  EXPECT_CALL(*GetBackend(), Find(kKey)).WillOnce(Return(nullptr));
  cache_->Find(kKey);
}

TEST_F(PersistentCacheTest, CacheInsertCallsBackendInsert) {
  CreateCache(true);
  EXPECT_CALL(*GetBackend(), Insert(kKey, _));
  cache_->Insert(kKey, base::byte_span_from_cstring("1"));
}

TEST_F(PersistentCacheTest, FailedBackendInitializationMeansNoFurtherCalls) {
  EXPECT_CALL(*backend_, Insert(kKey, _)).Times(0);
  EXPECT_CALL(*backend_, Find(kKey)).Times(0);

  CreateCache(false);
  cache_->Insert(kKey, base::byte_span_from_cstring("1"));
  cache_->Find(kKey);
}

}  // namespace persistent_cache
