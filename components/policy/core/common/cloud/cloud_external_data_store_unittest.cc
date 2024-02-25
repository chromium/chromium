// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_external_data_store.h"

#include <stddef.h>

#include <memory>
#include <optional>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_simple_task_runner.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kKey1[] = "Key 1";
const char kKey2[] = "Key 2";
const char kPolicy1[] = "Test policy 1";
const char kPolicy2[] = "Test policy 2";
std::string kPolicy1Key =
    CloudExternalDataManager::MetadataKey(kPolicy1).ToString();
std::string kPolicy2Key =
    CloudExternalDataManager::MetadataKey(kPolicy2).ToString();
const char kData1[] = "Testing data 1";
const char kData2[] = "Testing data 2";
const size_t kMaxSize = 100;

}  // namespace

class CloudExternalDataStoreTest : public testing::Test {
 public:
  CloudExternalDataStoreTest();
  CloudExternalDataStoreTest(const CloudExternalDataStoreTest&) = delete;
  CloudExternalDataStoreTest& operator=(const CloudExternalDataStoreTest&) =
      delete;

  void SetUp() override;

 protected:
  const std::string kData1Hash;
  const std::string kData2Hash;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ResourceCache> resource_cache_;
};

CloudExternalDataStoreTest::CloudExternalDataStoreTest()
    : kData1Hash(crypto::SHA256HashString(kData1)),
      kData2Hash(crypto::SHA256HashString(kData2)),
      task_runner_(new base::TestSimpleTaskRunner) {}

void CloudExternalDataStoreTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  resource_cache_ =
      std::make_unique<ResourceCache>(temp_dir_.GetPath(), task_runner_,
                                      /* max_cache_size */ std::nullopt);
}

TEST_F(CloudExternalDataStoreTest, StoreAndLoad) {
  // Write an entry to a store.
  CloudExternalDataStore store(kKey1, task_runner_, resource_cache_.get());
  EXPECT_FALSE(store.Store(kPolicy1, kData1Hash, kData1).empty());

  // Check that loading and verifying the entry against an invalid hash fails.
  std::string data;
  EXPECT_TRUE(store.Load(kPolicy1, kData2Hash, kMaxSize, &data).empty());

  // Check that loading and verifying the entry against its hash succeeds.
  EXPECT_FALSE(store.Load(kPolicy1, kData1Hash, kMaxSize, &data).empty());
  EXPECT_EQ(kData1, data);
}

TEST_F(CloudExternalDataStoreTest, StoreTooLargeAndLoad) {
  // Write an entry to a store.
  CloudExternalDataStore store(kKey1, task_runner_, resource_cache_.get());
  EXPECT_FALSE(store.Store(kPolicy1, kData1Hash, kData2).empty());

  // Check that the entry has been written to the resource cache backing the
  // store.
  std::map<std::string, std::string> contents;
  resource_cache_->LoadAllSubkeys(kKey1, &contents);
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(kData2, contents.begin()->second);

  // Check that loading the entry fails when the maximum allowed data size is
  // smaller than the entry size.
  std::string data;
  EXPECT_TRUE(store.Load(kPolicy1, kData1Hash, 1, &data).empty());

  // Verify that the oversized entry has been detected and removed from the
  // resource cache.
  resource_cache_->LoadAllSubkeys(kKey1, &contents);
  EXPECT_TRUE(contents.empty());
}

TEST_F(CloudExternalDataStoreTest, StoreInvalidAndLoad) {
  // Construct a store entry whose hash and contents do not match.
  CloudExternalDataStore store(kKey1, task_runner_, resource_cache_.get());
  EXPECT_FALSE(store.Store(kPolicy1, kData1Hash, kData2).empty());

  // Check that the entry has been written to the resource cache backing the
  // store.
  std::map<std::string, std::string> contents;
  resource_cache_->LoadAllSubkeys(kKey1, &contents);
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(kData2, contents.begin()->second);

  // Check that loading and verifying the entry against its hash fails.
  std::string data;
  EXPECT_TRUE(store.Load(kPolicy1, kData1Hash, kMaxSize, &data).empty());

  // Verify that the corrupted entry has been detected and removed from the
  // resource cache.
  resource_cache_->LoadAllSubkeys(kKey1, &contents);
  EXPECT_TRUE(contents.empty());
}

TEST_F(CloudExternalDataStoreTest, Prune) {
  // Write two entries to a store.
  CloudExternalDataStore store(kKey1, task_runner_, resource_cache_.get());
  EXPECT_FALSE(store.Store(kPolicy1, kData1Hash, kData1).empty());
  EXPECT_FALSE(store.Store(kPolicy2, kData2Hash, kData2).empty());

  // Check that loading and verifying the entries against their hashes succeeds.
  std::string data;
  EXPECT_FALSE(store.Load(kPolicy1, kData1Hash, kMaxSize, &data).empty());
  EXPECT_EQ(kData1, data);
  EXPECT_FALSE(store.Load(kPolicy2, kData2Hash, kMaxSize, &data).empty());
  EXPECT_EQ(kData2, data);

  // Prune the store, allowing only an entry for the first policy with its
  // current hash to be kept.
  std::vector<std::pair<std::string, std::string>> prune_data;
  prune_data.emplace_back(kPolicy1, kData1Hash);
  store.Prune(prune_data);

  // Check that the entry for the second policy has been removed from the
  // resource cache backing the store.
  std::map<std::string, std::string> contents;
  resource_cache_->LoadAllSubkeys(kKey1, &contents);
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(kData1, contents.begin()->second);

  // Prune the store, allowing only an entry for the first policy with a
  // different hash to be kept.
  prune_data.clear();
  prune_data.emplace_back(kPolicy1, kData2Hash);
  store.Prune(prune_data);

  // Check that the entry for the first policy has been removed from the
  // resource cache.
  resource_cache_->LoadAllSubkeys(kKey1, &contents);
  EXPECT_TRUE(contents.empty());
}

TEST_F(CloudExternalDataStoreTest, SharedCache) {
  // Write entries to two stores for two different cache_keys sharing a cache.
  CloudExternalDataStore store1(kKey1, task_runner_, resource_cache_.get());
  EXPECT_FALSE(store1.Store(kPolicy1, kData1Hash, kData1).empty());
  CloudExternalDataStore store2(kKey2, task_runner_, resource_cache_.get());
  EXPECT_FALSE(store2.Store(kPolicy2, kData2Hash, kData2).empty());

  // Check that the entries have been assigned to the correct keys in the
  // resource cache backing the stores.
  std::map<std::string, std::string> contents;
  resource_cache_->LoadAllSubkeys(kKey1, &contents);
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(kData1, contents.begin()->second);
  resource_cache_->LoadAllSubkeys(kKey2, &contents);
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(kData2, contents.begin()->second);

  // Check that each entry can be loaded from the correct store.
  std::string data;
  EXPECT_FALSE(store1.Load(kPolicy1, kData1Hash, kMaxSize, &data).empty());
  EXPECT_EQ(kData1, data);
  EXPECT_TRUE(store1.Load(kPolicy2, kData2Hash, kMaxSize, &data).empty());

  EXPECT_TRUE(store2.Load(kPolicy1, kData1Hash, kMaxSize, &data).empty());
  EXPECT_FALSE(store2.Load(kPolicy2, kData2Hash, kMaxSize, &data).empty());
  EXPECT_EQ(kData2, data);

  // Prune the first store, allowing no entries to be kept.
  std::vector<std::pair<std::string, std::string>> prune_data;
  store1.Prune(prune_data);

  // Check that the part of the resource cache backing the first store is empty.
  resource_cache_->LoadAllSubkeys(kKey1, &contents);
  EXPECT_TRUE(contents.empty());

  // Check that the part of the resource cache backing the second store is
  // unaffected.
  resource_cache_->LoadAllSubkeys(kKey2, &contents);
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(kData2, contents.begin()->second);
}

}  // namespace policy
