// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/lru_renderer_cache.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chromecast/browser/renderer_prelauncher.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#define EXPECT_CREATE_AND_PRELAUNCH(ptr, url)           \
  ptr = new MockPrelauncher(&browser_context_, url);    \
  EXPECT_CALL(*ptr, Prelaunch());                       \
  EXPECT_CALL(factory_, Create(&browser_context_, url)) \
      .WillOnce(Return(ByMove(std::unique_ptr<MockPrelauncher>(ptr))));

#define EXPECT_EVICTION(ptr) EXPECT_CALL(*ptr, Destroy());

using ::testing::_;
using ::testing::ByMove;
using ::testing::Expectation;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

namespace chromecast {

class MockPrelauncher : public RendererPrelauncher {
 public:
  MockPrelauncher(content::BrowserContext* browser_context,
                  const GURL& page_url)
      : RendererPrelauncher(browser_context,
                            page_url) {}
  ~MockPrelauncher() override { Destroy(); }

  MOCK_METHOD0(Prelaunch, void());
  MOCK_METHOD0(Destroy, void());
};

class MockFactory : public RendererPrelauncherFactory {
 public:
  MOCK_METHOD2(Create,
               std::unique_ptr<RendererPrelauncher>(
                   content::BrowserContext* browser_context,
                   const GURL& page_url));
};

class LRURendererCacheTest : public testing::Test {
 protected:
  void SetUp() override {}

  void SetFactory() {
    DCHECK(lru_cache_);
    lru_cache_->SetFactoryForTesting(&factory_);
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  MockFactory factory_;
  std::unique_ptr<LRURendererCache> lru_cache_;
};

TEST_F(LRURendererCacheTest, SimpleTakeAndRelease) {
  const GURL kUrl("https://www.one.com");

  lru_cache_ = std::make_unique<LRURendererCache>(&browser_context_, 1);
  SetFactory();
  MockPrelauncher* p1;
  std::unique_ptr<RendererPrelauncher> taken;

  // Don't return a prelauncher the first time, since the cache is empty.
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  taken = lru_cache_->TakeRendererPrelauncher(kUrl);
  ASSERT_FALSE(taken);
  // Cache: []
  // In-use: [ 1 ]

  // Releasing the prelauncher will cache it and prelaunch for later use.
  EXPECT_CREATE_AND_PRELAUNCH(p1, kUrl);
  lru_cache_->ReleaseRendererPrelauncher(kUrl);
  task_environment_.RunUntilIdle();
  // Cache: [ 1 ]
  // In-use: []

  // Get the cached prelauncher.
  taken = lru_cache_->TakeRendererPrelauncher(kUrl);
  ASSERT_TRUE(taken);
  ASSERT_TRUE(taken->IsForURL(kUrl));
  // Cache: [ ]
  // In-use: [ 1 ]

  // Return the prelauncher again, it should be cached the same as before.
  EXPECT_CREATE_AND_PRELAUNCH(p1, kUrl);
  lru_cache_->ReleaseRendererPrelauncher(kUrl);
  task_environment_.RunUntilIdle();
  // Cache: [ 1 ]
  // In-use: []
}

TEST_F(LRURendererCacheTest, SimpleCacheEviction) {
  const GURL kUrl("https://www.one.com");

  lru_cache_ = std::make_unique<LRURendererCache>(&browser_context_, 1);
  SetFactory();
  MockPrelauncher* p1;
  std::unique_ptr<RendererPrelauncher> taken;

  // Fill the cache.
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  taken = lru_cache_->TakeRendererPrelauncher(kUrl);
  ASSERT_FALSE(taken);
  EXPECT_CREATE_AND_PRELAUNCH(p1, kUrl);
  lru_cache_->ReleaseRendererPrelauncher(kUrl);
  task_environment_.RunUntilIdle();
  // Cache: [ 1 ]
  // In-use: []

  // Taking a different prelauncher destroys the cached one.
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  EXPECT_EVICTION(p1);
  taken = lru_cache_->TakeRendererPrelauncher(GURL("https://www.two.com"));
  ASSERT_FALSE(taken);
  // Cache: [ ]
  // In-use: [ 2 ]
}

TEST_F(LRURendererCacheTest, CapacityOne) {
  const GURL kUrl1("https://www.one.com");
  const GURL kUrl2("https://www.two.com");

  lru_cache_ = std::make_unique<LRURendererCache>(&browser_context_, 1);
  SetFactory();
  MockPrelauncher* p1;
  MockPrelauncher* p2;
  std::unique_ptr<RendererPrelauncher> taken;

  // Don't return a prelauncher the first time, since the cache is empty.
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  taken = lru_cache_->TakeRendererPrelauncher(kUrl1);
  ASSERT_FALSE(taken);
  // Cache: []
  // In-use: [ 1 ]

  // Releasing the prelauncher will cache it and prelaunch for later use.
  EXPECT_CREATE_AND_PRELAUNCH(p1, kUrl1);
  lru_cache_->ReleaseRendererPrelauncher(kUrl1);
  task_environment_.RunUntilIdle();
  // Cache: [ 1 ]
  // In-use: []

  // Get the cached prelauncher.
  taken = lru_cache_->TakeRendererPrelauncher(kUrl1);
  ASSERT_TRUE(taken);
  ASSERT_TRUE(taken->IsForURL(kUrl1));
  // Cache: [ ]
  // In-use: [ 1 ]

  // Return the prelauncher again, it should be cached the same as before.
  EXPECT_CREATE_AND_PRELAUNCH(p1, kUrl1);
  lru_cache_->ReleaseRendererPrelauncher(kUrl1);
  task_environment_.RunUntilIdle();
  // Cache: [ 1 ]
  // In-use: []

  // Getting the prelauncher for a non-cached URL will return nullptr. The cache
  // will evict 1 to stay below the renderer limit.
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  EXPECT_EVICTION(p1);
  taken = lru_cache_->TakeRendererPrelauncher(kUrl2);
  ASSERT_FALSE(taken);
  // Cache: [ ]
  // In-use: [ 2 ]

  // Return prelauncher 2, it should be cached.
  EXPECT_CREATE_AND_PRELAUNCH(p2, kUrl2);
  lru_cache_->ReleaseRendererPrelauncher(kUrl2);
  task_environment_.RunUntilIdle();
  // Cache: [ 2 ]
  // In-use: [ ]
  taken = lru_cache_->TakeRendererPrelauncher(kUrl2);
  ASSERT_TRUE(taken);
  ASSERT_TRUE(taken->IsForURL(kUrl2));
  // Cache: [ ]
  // In-use: [ 2 ]

  // Return prelauncher 2 once more, it will be cached.
  EXPECT_CREATE_AND_PRELAUNCH(p2, kUrl2);
  lru_cache_->ReleaseRendererPrelauncher(kUrl2);
  task_environment_.RunUntilIdle();
  // Cache: [ 2 ]
  // In-use: [ ]

  // Prelauncher 1 was evicted when 2 was cached. Taking 1 will evict 2.
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  EXPECT_EVICTION(p2);
  taken = lru_cache_->TakeRendererPrelauncher(kUrl1);
  ASSERT_FALSE(taken);
  // Cache: [ ]
  // In-use: [ 1 ]

  // Prelauncher 2 was evicted when 1 was taken.
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  taken = lru_cache_->TakeRendererPrelauncher(kUrl2);
  ASSERT_FALSE(taken);
  // Cache: [ ]
  // In-use: [ 1, 2 ]

  // Returning one of the two in-use pages to the cache won't actually cache it,
  // since there's still exactly 1 renderer in-use.
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  lru_cache_->ReleaseRendererPrelauncher(kUrl2);
  task_environment_.RunUntilIdle();
  // Cache: [ ]
  // In-use: [ 1 ]
}

TEST_F(LRURendererCacheTest, CapacityTwo) {
  const GURL kUrl1("https://www.one.com");
  const GURL kUrl2("https://www.two.com");
  const GURL kUrl3("https://www.three.com");

  lru_cache_ = std::make_unique<LRURendererCache>(&browser_context_, 2);
  SetFactory();
  MockPrelauncher* p1;
  MockPrelauncher* p2;
  std::unique_ptr<RendererPrelauncher> taken;

  // Take three renderers.
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  taken = lru_cache_->TakeRendererPrelauncher(kUrl1);
  ASSERT_FALSE(taken);
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  taken = lru_cache_->TakeRendererPrelauncher(kUrl2);
  ASSERT_FALSE(taken);
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  taken = lru_cache_->TakeRendererPrelauncher(kUrl3);
  ASSERT_FALSE(taken);
  // Cache: []
  // In-use: [ 1, 2, 3 ]

  // Don't cache renderer 3 since there are still 2 in use.
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  lru_cache_->ReleaseRendererPrelauncher(kUrl3);
  task_environment_.RunUntilIdle();
  // In-use: [ 1, 2 ]

  // Fill the cache with remaining 2 renderers.
  EXPECT_CREATE_AND_PRELAUNCH(p2, kUrl2);
  lru_cache_->ReleaseRendererPrelauncher(kUrl2);
  task_environment_.RunUntilIdle();
  EXPECT_CREATE_AND_PRELAUNCH(p1, kUrl1);
  lru_cache_->ReleaseRendererPrelauncher(kUrl1);
  task_environment_.RunUntilIdle();
  // Cache: [ 1, 2 ]
  // In-use: [ ]

  // Cache hit for renderer 1.
  taken = lru_cache_->TakeRendererPrelauncher(kUrl1);
  ASSERT_TRUE(taken);
  ASSERT_TRUE(taken->IsForURL(kUrl1));
  // Cache: [ 2 ]
  // In-use: [ 1 ]

  // Return renderer 1.
  EXPECT_CREATE_AND_PRELAUNCH(p1, kUrl1);
  lru_cache_->ReleaseRendererPrelauncher(kUrl1);
  task_environment_.RunUntilIdle();
  // Cache: [ 1, 2 ]
  // In-use: [ ]

  // Evict the least-recently cached renderer (2).
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  EXPECT_EVICTION(p2);
  taken = lru_cache_->TakeRendererPrelauncher(kUrl3);
  ASSERT_FALSE(taken);
  // Cache: [ 1 ]
  // In-use: [ 3 ]

  // Getting renderer 2 will fail since it's no long cached. This will evict
  // renderer 1.
  EXPECT_CALL(factory_, Create(_, _)).Times(0);
  EXPECT_EVICTION(p1);
  taken = lru_cache_->TakeRendererPrelauncher(kUrl2);
  ASSERT_FALSE(taken);
  // Cache: [ ]
  // In-use: [ 2, 3 ]
}

}  // namespace chromecast
