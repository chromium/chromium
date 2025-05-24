// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_cache.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commerce {

namespace {

const std::vector<uint64_t> kClusterIds1 = {1, 2, 3};
const std::vector<uint64_t> kClusterIds1AltOrder = {2, 1, 3};
const std::vector<uint64_t> kClusterIds2 = {1, 2, 4};
const std::string kProductName1 = "abc";
const std::string kProductName2 = "xyz";

MATCHER_P2(HasNameForDimensionId, cluster_id, product_name, "") {
  if (arg == nullptr) {
    return false;
  }

  auto it = arg->product_dimension_map.find(cluster_id);
  if (it == arg->product_dimension_map.end() || it->second != product_name) {
    return false;
  }

  return true;
}

}  // namespace

class ProductSpecificationsCacheTest : public testing::Test {
 public:
  const uint64_t kCacheSize = ProductSpecificationsCache::kCacheSize;
  const base::TimeDelta kEntryInvalidationTime =
      ProductSpecificationsCache::kEntryInvalidationTime;
  base::test::ScopedFeatureList test_features_;

  void SetUp() override {
    test_features_.InitWithFeatures({kProductSpecificationsCache}, {});
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ProductSpecificationsCacheTest, EntryMaintained) {
  ProductSpecificationsCache cache;

  ASSERT_TRUE(cache.GetEntry(kClusterIds1) == nullptr);

  ProductSpecifications specs = ProductSpecifications();
  specs.product_dimension_map[1L] = kProductName1;
  cache.SetEntry(kClusterIds1, std::move(specs));

  EXPECT_THAT(cache.GetEntry(kClusterIds1),
              HasNameForDimensionId(1L, kProductName1));
}

TEST_F(ProductSpecificationsCacheTest, MultipleEntriesMaintained) {
  ProductSpecificationsCache cache;

  ASSERT_TRUE(cache.GetEntry(kClusterIds1) == nullptr);
  ASSERT_TRUE(cache.GetEntry(kClusterIds2) == nullptr);

  ProductSpecifications specs1;
  specs1.product_dimension_map[1L] = kProductName1;
  cache.SetEntry(kClusterIds1, std::move(specs1));
  ProductSpecifications specs2;
  specs2.product_dimension_map[1L] = kProductName2;
  cache.SetEntry(kClusterIds2, std::move(specs2));

  EXPECT_THAT(cache.GetEntry(kClusterIds1),
              HasNameForDimensionId(1L, kProductName1));
  EXPECT_THAT(cache.GetEntry(kClusterIds2),
              HasNameForDimensionId(1L, kProductName2));
}

TEST_F(ProductSpecificationsCacheTest, ClusterIdsOrderDoesNotMatter) {
  ProductSpecificationsCache cache;

  ASSERT_TRUE(cache.GetEntry(kClusterIds1) == nullptr);
  ASSERT_TRUE(cache.GetEntry(kClusterIds1AltOrder) == nullptr);

  ProductSpecifications specs;
  specs.product_dimension_map[1L] = kProductName1;
  cache.SetEntry(kClusterIds1, std::move(specs));

  EXPECT_THAT(cache.GetEntry(kClusterIds1AltOrder),
              HasNameForDimensionId(1L, kProductName1));
}

TEST_F(ProductSpecificationsCacheTest, LeastRecentlyUsedEvicted) {
  ProductSpecificationsCache cache;

  // Fill the cache with the max number of entries.
  ProductSpecifications specs;
  specs.product_dimension_map[1L] = kProductName1;
  for (uint64_t i = 0; i < kCacheSize + 1; ++i) {
    cache.SetEntry({i}, specs);
  }

  // Get entry 1 so it is at the front of the ordering list.
  cache.GetEntry({1});

  // Add an entry to ensure entry 2 is removed while all others are maintained.
  cache.SetEntry({kCacheSize + 1}, specs);
  ASSERT_FALSE(cache.GetEntry({1}) == nullptr);
  ASSERT_TRUE(cache.GetEntry({2}) == nullptr);
  for (uint64_t i = 3; i < kCacheSize + 2; i++) {
    ASSERT_FALSE(cache.GetEntry({i}) == nullptr);
  }
}

TEST_F(ProductSpecificationsCacheTest, NoOpWhenDisabled) {
  // Disable the caching feature.
  test_features_.Reset();
  test_features_.InitWithFeatures({}, {kProductSpecificationsCache});

  ProductSpecificationsCache cache;

  ProductSpecifications specs1;
  specs1.product_dimension_map[1L] = kProductName1;
  cache.SetEntry(kClusterIds1, std::move(specs1));

  // The cache should always return a nullptr when disabled.
  ASSERT_TRUE(cache.GetEntry(kClusterIds1) == nullptr);
  ASSERT_TRUE(cache.GetEntry(kClusterIds2) == nullptr);
}

TEST_F(ProductSpecificationsCacheTest, EntryInvalidatedAfterTime) {
  ProductSpecificationsCache cache;

  ProductSpecifications specs1;
  specs1.product_dimension_map[1L] = kProductName1;
  cache.SetEntry(kClusterIds1, std::move(specs1));

  // The entry should still be valid up to the invalidation time.
  task_environment_.FastForwardBy(kEntryInvalidationTime);
  EXPECT_THAT(cache.GetEntry(kClusterIds1),
              HasNameForDimensionId(1L, kProductName1));

  // The entry should have expired a second after the invalidation time.
  task_environment_.FastForwardBy(base::Seconds(1));
  ASSERT_TRUE(cache.GetEntry(kClusterIds1) == nullptr);
}

}  // namespace commerce
