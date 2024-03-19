// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_info_cache.h"

#include <memory>

#include "base/functional/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {

const char kUrl1[] = "http://example.com/product";
const char kUrl2[] = "http://example.com/product/green";

class CommerceInfoCacheTest : public testing::Test {};

TEST_F(CommerceInfoCacheTest, CacheEntryMaintained) {
  CommerceInfoCache cache;

  ASSERT_FALSE(cache.IsUrlReferenced(GURL(kUrl1)));

  CommerceInfoCache::CacheEntry* entry = cache.GetEntryForUrl(GURL(kUrl1));
  ASSERT_TRUE(entry == nullptr);

  cache.AddRef(GURL(kUrl1));

  ASSERT_TRUE(cache.IsUrlReferenced(GURL(kUrl1)));

  entry = cache.GetEntryForUrl(GURL(kUrl1));
  ASSERT_FALSE(entry == nullptr);

  cache.RemoveRef(GURL(kUrl1));

  entry = cache.GetEntryForUrl(GURL(kUrl1));
  ASSERT_TRUE(entry == nullptr);
}

TEST_F(CommerceInfoCacheTest, CacheEntryMaintained_MultipleUrlInstances) {
  CommerceInfoCache cache;

  ASSERT_FALSE(cache.IsUrlReferenced(GURL(kUrl1)));

  ASSERT_TRUE(cache.GetEntryForUrl(GURL(kUrl1)) == nullptr);

  cache.AddRef(GURL(kUrl1));
  CommerceInfoCache::CacheEntry* entry_before =
      cache.GetEntryForUrl(GURL(kUrl1));
  ASSERT_FALSE(entry_before == nullptr);

  ASSERT_TRUE(cache.IsUrlReferenced(GURL(kUrl1)));

  // Add another instance of the URL.
  cache.AddRef(GURL(kUrl1));

  // The original pointer should match the current.
  ASSERT_EQ(entry_before, cache.GetEntryForUrl(GURL(kUrl1)));

  // Removing one instance should not affect the cache entry.
  cache.RemoveRef(GURL(kUrl1));
  ASSERT_EQ(entry_before, cache.GetEntryForUrl(GURL(kUrl1)));

  // Clear the last instance.
  cache.RemoveRef(GURL(kUrl1));

  ASSERT_TRUE(cache.GetEntryForUrl(GURL(kUrl1)) == nullptr);
}

TEST_F(CommerceInfoCacheTest, CacheEntryMaintained_MultipleUrls) {
  CommerceInfoCache cache;

  ASSERT_FALSE(cache.IsUrlReferenced(GURL(kUrl1)));
  ASSERT_FALSE(cache.IsUrlReferenced(GURL(kUrl2)));

  cache.AddRef(GURL(kUrl1));

  ASSERT_FALSE(cache.GetEntryForUrl(GURL(kUrl1)) == nullptr);
  ASSERT_TRUE(cache.GetEntryForUrl(GURL(kUrl2)) == nullptr);

  cache.AddRef(GURL(kUrl2));

  ASSERT_FALSE(cache.GetEntryForUrl(GURL(kUrl1)) == nullptr);
  ASSERT_FALSE(cache.GetEntryForUrl(GURL(kUrl2)) == nullptr);

  cache.RemoveRef(GURL(kUrl1));

  ASSERT_TRUE(cache.GetEntryForUrl(GURL(kUrl1)) == nullptr);
  ASSERT_FALSE(cache.GetEntryForUrl(GURL(kUrl2)) == nullptr);

  cache.RemoveRef(GURL(kUrl2));

  ASSERT_TRUE(cache.GetEntryForUrl(GURL(kUrl1)) == nullptr);
  ASSERT_TRUE(cache.GetEntryForUrl(GURL(kUrl2)) == nullptr);
}

}  // namespace
}  // namespace commerce
