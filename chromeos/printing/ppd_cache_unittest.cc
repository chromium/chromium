// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "chromeos/printing/ppd_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

// This fixture just points the cache at a temporary directory for the life of
// the test.
class PpdCacheTest : public ::testing::Test {
 public:
  PpdCacheTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(ppd_cache_temp_dir_.CreateUniqueTempDir());
  }

  // Make and return a cache for the test that uses a temporary directory
  // which is cleaned up at the end of the test.  Note that we pass
  // a (nonexistant) subdirectory of temp_dir_ to the cache to exercise
  // the lazy-creation-of-the-cache-directory code.
  scoped_refptr<PpdCache> CreateTestCache() {
    return PpdCache::CreateForTesting(
        ppd_cache_temp_dir_.GetPath().Append("Cache"),
        task_environment_.GetMainThreadTaskRunner());
  }

  void CaptureFindResult(const PpdCache::FindResult& result) {
    ++captured_find_results_;
    find_result_ = result;
  }

 protected:
  // Environment for task schedulers.
  base::test::TaskEnvironment task_environment_;

  // Number of find results we've captured.
  int captured_find_results_ = 0;

  // Most recent captured result.
  PpdCache::FindResult find_result_;

  // Overrider for DIR_CHROMEOS_PPD_CACHE that points it at a temporary
  // directory for the life of the test.
  base::ScopedTempDir ppd_cache_temp_dir_;
};


// Test that we miss on an empty cache.
TEST_F(PpdCacheTest, SimpleMiss) {
  auto cache = CreateTestCache();
  cache->Find("foo", base::BindOnce(&PpdCacheTest::CaptureFindResult,
                                    base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_results_, 1);
  EXPECT_FALSE(find_result_.success);
}

TEST_F(PpdCacheTest, MissThenHit) {
  auto cache = CreateTestCache();
  const char kTestKey[] = "My totally awesome key";
  const char kTestKey2[] = "A different key";
  const char kTestContents[] = "Like, totally awesome contents";

  cache->Find(kTestKey, base::BindOnce(&PpdCacheTest::CaptureFindResult,
                                       base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_results_, 1);
  EXPECT_FALSE(find_result_.success);

  cache->Store(kTestKey, kTestContents);

  task_environment_.RunUntilIdle();

  cache->Find(kTestKey, base::BindOnce(&PpdCacheTest::CaptureFindResult,
                                       base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_results_, 2);
  EXPECT_TRUE(find_result_.success);
  EXPECT_EQ(find_result_.contents, kTestContents);
  EXPECT_LT(find_result_.age, base::Minutes(5));

  cache->Find(kTestKey2, base::BindOnce(&PpdCacheTest::CaptureFindResult,
                                        base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_results_, 3);
  EXPECT_FALSE(find_result_.success);
}

// Test that we fill in the age field with something plausible.
TEST_F(PpdCacheTest, HitAge) {
  auto cache = CreateTestCache();
  const char kTestKey[] = "My totally awesome key";
  const char kTestContents[] = "Like, totally awesome contents";
  cache->Store(kTestKey, kTestContents);
  task_environment_.RunUntilIdle();

  cache->Find(kTestKey, base::BindOnce(&PpdCacheTest::CaptureFindResult,
                                       base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_results_, 1);
  // The age should be well under a second, but accept anything under an hour.
  EXPECT_LT(find_result_.age, base::Hours(1));
}

}  // namespace
}  // namespace chromeos
