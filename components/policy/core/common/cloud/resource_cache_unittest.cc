// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/resource_cache.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kKey1[] = "key 1";
const char kKey2[] = "key 2";
const char kKey3[] = "key 3";
const char kSubA[] = "a";
const char kSubB[] = "bb";
const char kSubC[] = "ccc";
const char kSubD[] = "dddd";
const char kSubE[] = "eeeee";

const char kData0[] = "{ \"key\": \"value\" }";
const char kData1[] = "{}";

const int kMaxCacheSize = 1024 * 10;
const std::string kData1Kb = std::string(1024, ' ');
const std::string kData2Kb = std::string(1024 * 2, ' ');
const std::string kData9Kb = std::string(1024 * 9, ' ');
const std::string kData10Kb = std::string(1024 * 10, ' ');
const std::string kData9KbUpdated = std::string(1024 * 9, '*');

bool Matches(const std::string& expected, const std::string& subkey) {
  return subkey == expected;
}

}  // namespace

class ResourceCacheTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(ResourceCacheTest, StoreAndLoad) {
  ResourceCache cache(temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
                      /* max_cache_size */ base::nullopt);

  // No data initially.
  std::string data;
  EXPECT_TRUE(cache.Load(kKey1, kSubA, &data).empty());

  // Store some data and load it.
  base::FilePath file_path = cache.Store(kKey1, kSubA, kData0);
  EXPECT_FALSE(file_path.empty());
  std::string file_content;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_content));
  EXPECT_EQ(kData0, file_content);

  file_path = cache.Load(kKey1, kSubA, &data);
  EXPECT_FALSE(file_path.empty());
  file_content.clear();
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_content));
  EXPECT_EQ(kData0, file_content);
  EXPECT_EQ(kData0, data);

  // Store more data in another subkey.
  EXPECT_FALSE(cache.Store(kKey1, kSubB, kData1).empty());

  // Write subkeys to two other keys.
  EXPECT_FALSE(cache.Store(kKey2, kSubA, kData0).empty());
  EXPECT_FALSE(cache.Store(kKey2, kSubB, kData1).empty());
  EXPECT_FALSE(cache.Store(kKey3, kSubA, kData0).empty());
  EXPECT_FALSE(cache.Store(kKey3, kSubB, kData1).empty());

  // Enumerate all the subkeys.
  std::map<std::string, std::string> contents;
  cache.LoadAllSubkeys(kKey1, &contents);
  EXPECT_EQ(2u, contents.size());
  EXPECT_EQ(kData0, contents[kSubA]);
  EXPECT_EQ(kData1, contents[kSubB]);

  // Store more subkeys.
  EXPECT_FALSE(cache.Store(kKey1, kSubC, kData1).empty());
  EXPECT_FALSE(cache.Store(kKey1, kSubD, kData1).empty());
  EXPECT_FALSE(cache.Store(kKey1, kSubE, kData1).empty());

  // Now purge some of them.
  std::set<std::string> keep;
  keep.insert(kSubB);
  keep.insert(kSubD);
  cache.PurgeOtherSubkeys(kKey1, keep);

  // Enumerate all the remaining subkeys.
  cache.LoadAllSubkeys(kKey1, &contents);
  EXPECT_EQ(2u, contents.size());
  EXPECT_EQ(kData1, contents[kSubB]);
  EXPECT_EQ(kData1, contents[kSubD]);

  // Delete subkeys directly.
  cache.Delete(kKey1, kSubB);
  cache.Delete(kKey1, kSubD);
  cache.LoadAllSubkeys(kKey1, &contents);
  EXPECT_EQ(0u, contents.size());

  // The other two keys were not affected.
  cache.LoadAllSubkeys(kKey2, &contents);
  EXPECT_EQ(2u, contents.size());
  EXPECT_EQ(kData0, contents[kSubA]);
  EXPECT_EQ(kData1, contents[kSubB]);
  cache.LoadAllSubkeys(kKey3, &contents);
  EXPECT_EQ(2u, contents.size());
  EXPECT_EQ(kData0, contents[kSubA]);
  EXPECT_EQ(kData1, contents[kSubB]);

  // Now purge all keys except the third.
  keep.clear();
  keep.insert(kKey3);
  cache.PurgeOtherKeys(keep);

  // The first two keys are empty.
  cache.LoadAllSubkeys(kKey1, &contents);
  EXPECT_EQ(0u, contents.size());
  cache.LoadAllSubkeys(kKey1, &contents);
  EXPECT_EQ(0u, contents.size());

  // The third key is unaffected.
  cache.LoadAllSubkeys(kKey3, &contents);
  EXPECT_EQ(2u, contents.size());
  EXPECT_EQ(kData0, contents[kSubA]);
  EXPECT_EQ(kData1, contents[kSubB]);
}

TEST_F(ResourceCacheTest, FilterSubkeys) {
  ResourceCache cache(temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
                      /* max_cache_size */ base::nullopt);

  // Store some data.
  EXPECT_FALSE(cache.Store(kKey1, kSubA, kData0).empty());
  EXPECT_FALSE(cache.Store(kKey1, kSubB, kData1).empty());
  EXPECT_FALSE(cache.Store(kKey1, kSubC, kData0).empty());
  EXPECT_FALSE(cache.Store(kKey2, kSubA, kData0).empty());
  EXPECT_FALSE(cache.Store(kKey2, kSubB, kData1).empty());
  EXPECT_FALSE(cache.Store(kKey3, kSubA, kData0).empty());
  EXPECT_FALSE(cache.Store(kKey3, kSubB, kData1).empty());

  // Check the contents of kKey1.
  std::map<std::string, std::string> contents;
  cache.LoadAllSubkeys(kKey1, &contents);
  EXPECT_EQ(3u, contents.size());
  EXPECT_EQ(kData0, contents[kSubA]);
  EXPECT_EQ(kData1, contents[kSubB]);
  EXPECT_EQ(kData0, contents[kSubC]);

  // Filter some subkeys.
  cache.FilterSubkeys(kKey1, base::Bind(&Matches, kSubA));

  // Check the contents of kKey1 again.
  cache.LoadAllSubkeys(kKey1, &contents);
  EXPECT_EQ(2u, contents.size());
  EXPECT_EQ(kData1, contents[kSubB]);
  EXPECT_EQ(kData0, contents[kSubC]);

  // Other keys weren't affected.
  cache.LoadAllSubkeys(kKey2, &contents);
  EXPECT_EQ(2u, contents.size());
  cache.LoadAllSubkeys(kKey3, &contents);
  EXPECT_EQ(2u, contents.size());
}

TEST_F(ResourceCacheTest, StoreWithEnabledCacheLimit) {
  ResourceCache cache(temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
                      kMaxCacheSize);
  task_environment_.RunUntilIdle();

  // Put first subkey with 9Kb data in cache.
  EXPECT_FALSE(cache.Store(kKey1, kSubA, kData9Kb).empty());
  // Try to put second subkey with 2Kb data in cache, expected to fail while
  // total size exceeds 10Kb.
  EXPECT_TRUE(cache.Store(kKey2, kSubB, kData2Kb).empty());
  // Put second subkey with 1Kb data in cache.
  EXPECT_FALSE(cache.Store(kKey2, kSubC, kData1Kb).empty());
  // Try to put third subkey with 2 bytes data in cache, expected to fail while
  // total size exceeds 10Kb.
  EXPECT_TRUE(cache.Store(kKey1, kSubB, kData1).empty());

  // Remove keys with all subkeys.
  cache.Clear(kKey1);
  cache.Clear(kKey2);

  // Put first subkey with 9Kb data in cache.
  EXPECT_FALSE(cache.Store(kKey3, kSubA, kData9Kb).empty());
  // Put second subkey with 1Kb data in cache.
  EXPECT_FALSE(cache.Store(kKey3, kSubB, kData1Kb).empty());
  // Try to put third subkey with 2 bytes data in cache, expected to fail while
  // total size exceeds 10Kb.
  EXPECT_TRUE(cache.Store(kKey1, kSubB, kData1).empty());

  // Replace data in first subkey with another 9Kb data.
  EXPECT_FALSE(cache.Store(kKey3, kSubA, kData9KbUpdated).empty());

  // Remove this key with 9Kb data.
  cache.Delete(kKey3, kSubA);

  // Put second subkey with 2 bytes data in cache.
  EXPECT_FALSE(cache.Store(kKey1, kSubB, kData1).empty());
}

#if defined(OS_POSIX)  // Because of symbolic links.

TEST_F(ResourceCacheTest, StoreInDirectoryWithCycleSymlinks) {
  base::FilePath inner_dir = temp_dir_.GetPath().AppendASCII("inner");
  ASSERT_TRUE(base::CreateDirectory(inner_dir));
  base::FilePath symlink_to_parent = inner_dir.AppendASCII("symlink");
  ASSERT_TRUE(base::CreateSymbolicLink(temp_dir_.GetPath(), symlink_to_parent));

  ResourceCache cache(temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
                      kMaxCacheSize);
  task_environment_.RunUntilIdle();

  // Check if the cache is empty
  EXPECT_FALSE(cache.Store(kKey1, kSubA, kData10Kb).empty());
}

TEST_F(ResourceCacheTest, StoreInDirectoryWithSymlinkToRoot) {
  base::FilePath inner_dir = temp_dir_.GetPath().AppendASCII("inner");
  ASSERT_TRUE(base::CreateDirectory(inner_dir));
  base::FilePath root_path(FILE_PATH_LITERAL("/"));
  base::FilePath symlink_to_root = temp_dir_.GetPath().AppendASCII("symlink");
  ASSERT_TRUE(base::CreateSymbolicLink(root_path, symlink_to_root));

  ResourceCache cache(temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
                      kMaxCacheSize);
  task_environment_.RunUntilIdle();

  // Check if the cache is empty
  EXPECT_FALSE(cache.Store(kKey1, kSubA, kData10Kb).empty());
}

#endif  // defined(OS_POSIX)

}  // namespace policy
