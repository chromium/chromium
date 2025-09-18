// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_cache.h"

#include <optional>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

optimization_guide::proto::AnnotatedPageContent TestContent(
    const std::string& title) {
  optimization_guide::proto::AnnotatedPageContent apc;
  apc.mutable_main_frame_data()->set_title(title);
  return apc;
}

}  // namespace

class PageContentCacheTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting();
  }

  void TearDown() override { cache_.reset(); }

 protected:
  PageContentCache* GetOrCreateCache() {
    if (!cache_) {
      cache_ = std::make_unique<PageContentCache>(os_crypt_async_.get(),
                                                  temp_dir_.GetPath());
    }
    return cache_.get();
  }

  std::optional<optimization_guide::proto::AnnotatedPageContent>
  GetContentForTab(int64_t tab_id) {
    base::RunLoop run_loop;
    std::optional<optimization_guide::proto::AnnotatedPageContent> result;
    GetOrCreateCache()->GetPageContentForTab(
        tab_id,
        base::BindOnce(
            [](base::RunLoop* run_loop,
               std::optional<optimization_guide::proto::AnnotatedPageContent>*
                   result,
               std::optional<optimization_guide::proto::AnnotatedPageContent>
                   apc) {
              *result = std::move(apc);
              run_loop->Quit();
            },
            &run_loop, &result));
    run_loop.Run();
    return result;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  std::unique_ptr<PageContentCache> cache_;
};

TEST_F(PageContentCacheTest, CacheAndGet) {
  const int64_t kTabId = 1;
  const GURL kUrl("https://example.com/");
  const auto kApc = TestContent("test title");

  GetOrCreateCache()->CachePageContent(kTabId, kUrl, base::Time::Now(),
                                       base::Time::Now(), kApc);

  std::optional<optimization_guide::proto::AnnotatedPageContent> apc =
      GetContentForTab(kTabId);
  ASSERT_TRUE(apc.has_value());
  EXPECT_EQ(kApc.main_frame_data().title(), apc->main_frame_data().title());
}

TEST_F(PageContentCacheTest, Remove) {
  const int64_t kTabId = 1;
  const GURL kUrl("https://example.com/");
  const auto kApc = TestContent("test title");

  GetOrCreateCache()->CachePageContent(kTabId, kUrl, base::Time::Now(),
                                       base::Time::Now(), kApc);

  ASSERT_TRUE(GetContentForTab(kTabId));

  GetOrCreateCache()->RemovePageContentForTab(kTabId);

  base::RunLoop run_loop_remove;
  ASSERT_FALSE(GetContentForTab(kTabId));
}

TEST_F(PageContentCacheTest, DeleteOldDataOnTimer) {
  // Let initial deletion task run.
  task_environment_.FastForwardBy(base::Seconds(26));

  // Add old data.
  const GURL kOldUrl("https://old.com/");
  GetOrCreateCache()->CachePageContent(1, kOldUrl,
                                       base::Time::Now() - base::Days(8),
                                       base::Time::Now(), TestContent("old"));

  ASSERT_TRUE(GetContentForTab(1));

  // Fast forward to trigger the timer.
  task_environment_.FastForwardBy(base::Days(1));

  // Verify old data is gone.
  EXPECT_FALSE(GetContentForTab(1));
}

TEST_F(PageContentCacheTest, DeleteOldDataOnStartup) {
  auto os_crypt_async = os_crypt_async::GetTestOSCryptAsyncForTesting();

  // 1. Add both old and new data to the cache.
  GetOrCreateCache()->CachePageContent(1, GURL("https://old.com/"),
                                       base::Time::Now() - base::Days(8),
                                       base::Time::Now(), TestContent("old"));
  GetOrCreateCache()->CachePageContent(2, GURL("https://new.com/"),
                                       base::Time::Now(), base::Time::Now(),
                                       TestContent("new"));

  ASSERT_TRUE(GetContentForTab(1));

  // 2. Shut down the old cache and wait for its database to close.
  cache_.reset();
  task_environment_.RunUntilIdle();

  // 3. Create a new cache instance, simulating a browser restart.
  // This is the instance under test.
  GetOrCreateCache();

  // 4. Fast forward past the startup deletion delay.
  task_environment_.FastForwardBy(base::Seconds(26));
  task_environment_.RunUntilIdle();

  // 5. Verify that the old data is gone and the new data is still there.
  EXPECT_FALSE(GetContentForTab(1));
  EXPECT_TRUE(GetContentForTab(2));
}

}  // namespace page_content_annotations
