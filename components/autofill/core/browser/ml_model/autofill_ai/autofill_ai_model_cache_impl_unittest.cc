// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache_impl.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "components/autofill/core/common/signatures.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class AutofillAiModelCacheImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());
    RecreateCache();
  }

  void TearDown() override {
    cache_.reset();
    db_provider_.reset();
    // Allow for destruction on a different sequence.
    task_environment_.RunUntilIdle();
  }

  // Simulates restart of the browser by recreating the cache.
  void RecreateCache() {
    // Process remaining operations.
    task_environment_.RunUntilIdle();
    cache_ = std::make_unique<AutofillAiModelCacheImpl>(db_provider_.get(),
                                                        temp_dir_.GetPath());
    // Wait until database has loaded.
    task_environment_.RunUntilIdle();
  }

  AutofillAiModelCache& cache() { return *cache_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<AutofillAiModelCache> cache_;
};

TEST_F(AutofillAiModelCacheImplTest, AddNewEntry) {
  constexpr auto signature1 = FormSignature(123);
  constexpr auto signature2 = FormSignature(234);

  EXPECT_FALSE(cache().Contains(signature1));
  EXPECT_FALSE(cache().Contains(signature2));
  cache().Update(signature1, AutofillAiModelCache::CacheEntry());
  EXPECT_TRUE(cache().Contains(signature1));
  EXPECT_FALSE(cache().Contains(signature2));
}

// Tests that recreating the cache repopulates it with the data persisted on
// disk.
TEST_F(AutofillAiModelCacheImplTest, CacheSurvivesRestart) {
  constexpr auto signature = FormSignature(123);

  EXPECT_FALSE(cache().Contains(signature));
  cache().Update(signature, AutofillAiModelCache::CacheEntry());
  EXPECT_TRUE(cache().Contains(signature));

  // Simulate restart.
  RecreateCache();
  EXPECT_TRUE(cache().Contains(signature));
}

}  // namespace

}  // namespace autofill
