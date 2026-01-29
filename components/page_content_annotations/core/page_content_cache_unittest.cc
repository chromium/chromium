// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_cache.h"

#include <optional>

#include "base/files/scoped_temp_dir.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

optimization_guide::proto::PageContext TestContent(const std::string& title) {
  optimization_guide::proto::PageContext page_context;
  page_context.mutable_annotated_page_content()
      ->mutable_main_frame_data()
      ->set_title(title);
  return page_context;
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
      cache_ = std::make_unique<PageContentCache>(
          os_crypt_async_.get(), temp_dir_.GetPath(), base::Days(7));
    }
    return cache_.get();
  }

  std::optional<optimization_guide::proto::PageContext> GetContentForTab(
      int64_t tab_id) {
    base::RunLoop run_loop;
    std::optional<optimization_guide::proto::PageContext> result;
    GetOrCreateCache()->GetPageContentForTab(
        tab_id,
        base::BindOnce(
            [](base::RunLoop* run_loop,
               std::optional<optimization_guide::proto::PageContext>* result,
               std::optional<optimization_guide::proto::PageContext>
                   page_context) {
              *result = std::move(page_context);
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
  const auto kPageContext = TestContent("test title");

  GetOrCreateCache()->CachePageContent(kTabId, kUrl, base::Time::Now(),
                                       base::Time::Now(), kPageContext);

  std::optional<optimization_guide::proto::PageContext> page_context =
      GetContentForTab(kTabId);
  ASSERT_TRUE(page_context.has_value());
  EXPECT_EQ(kPageContext.annotated_page_content().main_frame_data().title(),
            page_context->annotated_page_content().main_frame_data().title());
}

TEST_F(PageContentCacheTest, Remove) {
  const int64_t kTabId = 1;
  const GURL kUrl("https://example.com/");
  const auto kApc = TestContent("test title");

  GetOrCreateCache()->CachePageContent(kTabId, kUrl, base::Time::Now(),
                                       base::Time::Now(),
                                       TestContent("test title"));

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

TEST_F(PageContentCacheTest, DeleteOldDataWithCustomMaxAge) {
  // Create a cache with a custom max age of 5 days.
  cache_ = std::make_unique<PageContentCache>(
      os_crypt_async_.get(), temp_dir_.GetPath(), base::Days(5));

  // Add data that is 6 days old (should be deleted).
  GetOrCreateCache()->CachePageContent(1, GURL("https://old.com/"),
                                       base::Time::Now() - base::Days(6),
                                       base::Time::Now(), TestContent("old"));

  // Add data that is 4 days old (should be kept).
  GetOrCreateCache()->CachePageContent(2, GURL("https://new.com/"),
                                       base::Time::Now() - base::Days(4),
                                       base::Time::Now(), TestContent("new"));

  ASSERT_TRUE(GetContentForTab(1));
  ASSERT_TRUE(GetContentForTab(2));

  // Trigger the deletion task.
  task_environment_.FastForwardBy(base::Seconds(26));

  // Verify old data is gone and new data remains.
  EXPECT_FALSE(GetContentForTab(1));
  EXPECT_TRUE(GetContentForTab(2));
}

TEST_F(PageContentCacheTest, RecordMetrics) {
  base::HistogramTester histogram_tester;

  // 1. Add some data to the cache.
  GetOrCreateCache()->CachePageContent(1, GURL("https://one.com/"),
                                       base::Time::Now(), base::Time::Now(),
                                       TestContent("one"));
  GetOrCreateCache()->CachePageContent(2, GURL("https://two.com/"),
                                       base::Time::Now(), base::Time::Now(),
                                       TestContent("two"));
  GetOrCreateCache()->CachePageContent(3, GURL("https://three.com/"),
                                       base::Time::Now(), base::Time::Now(),
                                       TestContent("three"));

  // 2. Call RunCleanUpTasksWithActiveTabs.
  // Eligible tabs are 1, 3, 4.
  // Cached tabs are 1, 2, 3.
  // Cached and eligible: 1, 3 (count = 2)
  // Stale cache entries: 2 (count = 1)
  // Not cached eligible tabs: 4 (count = 1)
  base::StatisticsRecorder::HistogramWaiter waiter(
      "OptimizationGuide.PageContentCache.EligibleTabsCachedPercentage");
  GetOrCreateCache()->RunCleanUpTasksWithActiveTabs({1, 3, 4});
  task_environment_.FastForwardBy(base::Seconds(26));

  // 3. Wait for metrics calculation to complete.
  waiter.Wait();

  // 4. Verify histograms, which reflects before the stale data clean up
  // happens.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.TotalCacheSize", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.CachedTabsCount", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.NotCachedTabsCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.StaleCacheEntriesCount", 1, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.AvgPageSize", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.EligibleTabsCachedPercentage", 66, 1);
}

TEST_F(PageContentCacheTest, RunCleanUpTasksWithActiveTabs) {
  // 1. Add some data to the cache.
  GetOrCreateCache()->CachePageContent(1, GURL("https://one.com/"),
                                       base::Time::Now(), base::Time::Now(),
                                       TestContent("one"));
  GetOrCreateCache()->CachePageContent(2, GURL("https://two.com/"),
                                       base::Time::Now(), base::Time::Now(),
                                       TestContent("two"));
  GetOrCreateCache()->CachePageContent(3, GURL("https://three.com/"),
                                       base::Time::Now(), base::Time::Now(),
                                       TestContent("three"));

  ASSERT_TRUE(GetContentForTab(1));
  ASSERT_TRUE(GetContentForTab(2));
  ASSERT_TRUE(GetContentForTab(3));

  // 2. Call RunCleanUpTasksWithActiveTabs, which should clear out tab 2.
  GetOrCreateCache()->RunCleanUpTasksWithActiveTabs({1, 3});
  task_environment_.FastForwardBy(base::Seconds(26));

  // 3. Verify that tab 2 is gone, but 1 and 3 remain.
  EXPECT_FALSE(GetContentForTab(2));
  EXPECT_TRUE(GetContentForTab(1));
  EXPECT_TRUE(GetContentForTab(3));
}

}  // namespace page_content_annotations
