// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/service_worker/service_worker_disk_cache.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

class ServiceWorkerDiskCacheTest : public testing::Test {
 public:
  ServiceWorkerDiskCacheTest() = default;

  void SetUp() override { ASSERT_TRUE(directory_.CreateUniqueTempDir()); }

  void TearDown() override { FlushCacheTasks(); }

  void FlushCacheTasks() {
    disk_cache::FlushCacheThreadForTesting();
    task_environment_.RunUntilIdle();
  }

  void InitializeDiskCache(ServiceWorkerDiskCache* disk_cache) {
    base::RunLoop loop;
    disk_cache->InitWithDiskBackend(GetPath(),
                                    /*post_cleanup_callback=*/base::DoNothing(),
                                    base::BindLambdaForTesting([&](int rv) {
                                      ASSERT_EQ(rv, net::OK);
                                      loop.Quit();
                                    }));
    loop.Run();
  }

  base::FilePath GetPath() { return directory_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir directory_;
};

// Tests that callbacks of operations are invoked even when these operations are
// called at the same time for the same key.
TEST_F(ServiceWorkerDiskCacheTest, MultipleCallsForSameKey) {
  auto disk_cache = std::make_unique<ServiceWorkerDiskCache>();

  bool create_entry_called = false;
  bool open_entry_called = false;
  bool doom_entry_called = false;

  const int64_t kKey = 1;
  disk_cache->CreateEntry(
      kKey, base::BindLambdaForTesting(
                [&](int rv, std::unique_ptr<ServiceWorkerDiskCacheEntry>) {
                  create_entry_called = true;
                }));
  disk_cache->OpenEntry(
      kKey, base::BindLambdaForTesting(
                [&](int rv, std::unique_ptr<ServiceWorkerDiskCacheEntry>) {
                  open_entry_called = true;
                }));
  disk_cache->DoomEntry(kKey, base::BindLambdaForTesting(
                                  [&](int rv) { doom_entry_called = true; }));

  InitializeDiskCache(disk_cache.get());
  FlushCacheTasks();

  EXPECT_TRUE(create_entry_called);
  EXPECT_TRUE(open_entry_called);
  EXPECT_TRUE(doom_entry_called);
}

TEST_F(ServiceWorkerDiskCacheTest, DisablePriorToInitCompletion) {
  // Create an instance and start it initializing, queue up
  // one of each kind of "entry" function.
  auto disk_cache = std::make_unique<ServiceWorkerDiskCache>();
  EXPECT_FALSE(disk_cache->is_disabled());

  size_t callback_count = 0;
  auto completion_callback = base::BindLambdaForTesting([&](int rv) {
    EXPECT_EQ(rv, net::ERR_ABORTED);
    ++callback_count;
  });
  auto entry_callback = base::BindLambdaForTesting(
      [&](int rv, std::unique_ptr<ServiceWorkerDiskCacheEntry> entry) {
        EXPECT_EQ(rv, net::ERR_ABORTED);
        EXPECT_FALSE(entry);
        ++callback_count;
      });
  disk_cache->InitWithDiskBackend(GetPath(),
                                  /*post_cleanup_callback=*/base::DoNothing(),
                                  completion_callback);
  disk_cache->CreateEntry(1, entry_callback);
  disk_cache->OpenEntry(2, entry_callback);
  disk_cache->DoomEntry(3, completion_callback);

  // Pull the plug on all that.
  EXPECT_FALSE(disk_cache->is_disabled());
  disk_cache->Disable();
  EXPECT_TRUE(disk_cache->is_disabled());

  FlushCacheTasks();

  EXPECT_EQ(callback_count, 4u);

  // Ensure the directory can be deleted at this point.
  EXPECT_TRUE(base::DirectoryExists(GetPath()));
  EXPECT_FALSE(base::IsDirectoryEmpty(GetPath()));
  EXPECT_TRUE(base::DeletePathRecursively(GetPath()));
  EXPECT_FALSE(base::DirectoryExists(GetPath()));
}

TEST_F(ServiceWorkerDiskCacheTest, DisableAfterInitted) {
  // Create an instance and start it initializing, queue up
  // one of each kind of "entry" function.
  auto disk_cache = std::make_unique<ServiceWorkerDiskCache>();
  EXPECT_FALSE(disk_cache->is_disabled());
  InitializeDiskCache(disk_cache.get());

  // Pull the plug
  disk_cache->Disable();
  FlushCacheTasks();

  // Ensure the directory can be deleted at this point.
  EXPECT_TRUE(base::DirectoryExists(GetPath()));
  EXPECT_FALSE(base::IsDirectoryEmpty(GetPath()));
  EXPECT_TRUE(base::DeletePathRecursively(GetPath()));
  EXPECT_FALSE(base::DirectoryExists(GetPath()));

  // Methods should fail.
  size_t callback_count = 0;
  auto completion_callback = base::BindLambdaForTesting([&](int rv) {
    EXPECT_EQ(rv, net::ERR_ABORTED);
    ++callback_count;
  });
  auto entry_callback = base::BindLambdaForTesting(
      [&](int rv, std::unique_ptr<ServiceWorkerDiskCacheEntry> entry) {
        EXPECT_EQ(rv, net::ERR_ABORTED);
        EXPECT_FALSE(entry);
        ++callback_count;
      });
  disk_cache->CreateEntry(1, entry_callback);
  disk_cache->OpenEntry(2, entry_callback);
  disk_cache->DoomEntry(3, completion_callback);
  FlushCacheTasks();

  EXPECT_EQ(callback_count, 3u);
}

TEST_F(ServiceWorkerDiskCacheTest, CleanupCallback) {
  // Test that things delete fine when we disable the cache and wait for
  // the cleanup callback.

  net::TestClosure cleanup_done;
  net::TestCompletionCallback init_done;
  auto disk_cache = std::make_unique<ServiceWorkerDiskCache>();
  EXPECT_FALSE(disk_cache->is_disabled());
  disk_cache->InitWithDiskBackend(GetPath(), cleanup_done.closure(),
                                  init_done.callback());
  EXPECT_EQ(net::OK, init_done.WaitForResult());

  disk_cache->Disable();
  cleanup_done.WaitForResult();

  // Ensure the directory can be deleted at this point.
  EXPECT_TRUE(base::DirectoryExists(GetPath()));
  EXPECT_FALSE(base::IsDirectoryEmpty(GetPath()));
  EXPECT_TRUE(base::DeletePathRecursively(GetPath()));
  EXPECT_FALSE(base::DirectoryExists(GetPath()));
}

}  // namespace storage
