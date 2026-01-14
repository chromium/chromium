// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_cache_handler.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/page_content_annotations/core/page_content_cache.h"
#include "components/page_content_annotations/core/web_state_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

optimization_guide::proto::PageContext CreatePageContext(
    const std::string& title) {
  optimization_guide::proto::PageContext page_context;
  page_context.mutable_annotated_page_content()
      ->mutable_main_frame_data()
      ->set_title(title);
  return page_context;
}

WebStateWrapper CreateWebStateWrapper(
    const GURL& url,
    PageContentVisibility visibility,
    base::Time navigation_timestamp = base::Time::Now(),
    bool is_off_the_record = false) {
  return WebStateWrapper(is_off_the_record, url, navigation_timestamp,
                         visibility);
}

}  // namespace

class PageContentCacheHandlerTest : public testing::Test {
 public:
  PageContentCacheHandlerTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting();
    handler_ = std::make_unique<PageContentCacheHandler>(
        os_crypt_async_.get(), temp_dir_.GetPath(), base::Days(7));
  }

  void TearDown() override {
    handler_.reset();
    os_crypt_async_.reset();
  }

  void CheckPageContentCached(
      int64_t tab_id,
      GURL url,
      base::Time navigation_timestamp,
      base::Time extraction_time,
      optimization_guide::proto::PageContext page_context) {
    base::RunLoop run_loop;
    handler_->page_content_cache()->GetPageContentForTab(
        tab_id,
        base::BindOnce(
            [](PageContentCacheHandlerTest* self, base::RunLoop* run_loop_ptr,
               GURL expected_url, base::Time expected_navigation_timestamp,
               base::Time expected_extraction_time,
               optimization_guide::proto::PageContext expected_page_context,
               std::optional<optimization_guide::PageContentResult>
                   actual_result) {
              self->PerformChecks(run_loop_ptr, std::move(actual_result),
                                  expected_url, expected_navigation_timestamp,
                                  expected_extraction_time,
                                  expected_page_context);
            },
            base::Unretained(this), &run_loop, url, navigation_timestamp,
            extraction_time, page_context));
    run_loop.Run();
  }

  void PerformChecks(
      base::RunLoop* run_loop,
      std::optional<optimization_guide::PageContentResult> result,
      GURL url,
      base::Time navigation_timestamp,
      base::Time extraction_time,
      optimization_guide::proto::PageContext page_context) {
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->url, url);
    EXPECT_EQ(result->navigation_timestamp, navigation_timestamp);
    EXPECT_EQ(result->extraction_time, extraction_time);
    EXPECT_EQ(
        result->page_context.annotated_page_content().main_frame_data().title(),
        page_context.annotated_page_content().main_frame_data().title());
    run_loop->Quit();
  }

  void CheckPageContentNotCached(int64_t tab_id) {
    base::RunLoop run_loop;
    handler_->page_content_cache()->GetPageContentForTab(
        tab_id,
        base::BindOnce(
            [](PageContentCacheHandlerTest* self, base::RunLoop* run_loop_ptr,
               std::optional<optimization_guide::PageContentResult> result) {
              EXPECT_FALSE(result.has_value());
              run_loop_ptr->Quit();
            },
            base::Unretained(this), &run_loop));
    run_loop.Run();
  }

  bool IsTabClosed(int64_t tab_id) { return handler_->IsTabClosed(tab_id); }

  void CheckClosedTabCachedContent(
      int64_t tab_id,
      GURL url,
      base::Time navigation_timestamp,
      base::Time extraction_time,
      optimization_guide::proto::PageContext page_context) {
    EXPECT_EQ(handler_->closed_tabs_.at(tab_id)->url, url);
    EXPECT_EQ(handler_->closed_tabs_.at(tab_id)->navigation_timestamp,
              navigation_timestamp);
    EXPECT_EQ(handler_->closed_tabs_.at(tab_id)->extraction_time,
              extraction_time);
    EXPECT_EQ(handler_->closed_tabs_.at(tab_id)
                  ->page_context.annotated_page_content()
                  .main_frame_data()
                  .title(),
              page_context.annotated_page_content().main_frame_data().title());
  }

  void CheckClosedTabCachedContentEmpty(int64_t tab_id) {
    EXPECT_EQ(handler_->closed_tabs_.at(tab_id), nullptr);
  }

  void CheckClosedTabNotCached(int64_t tab_id) {
    EXPECT_FALSE(handler_->closed_tabs_.contains(tab_id));
  }

  void CheckCommittedClosedTab(int64_t tab_id, bool is_committed) {
    EXPECT_EQ(handler_->committed_closed_tabs_.contains(tab_id), is_committed);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  std::unique_ptr<PageContentCacheHandler> handler_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PageContentCacheHandlerTest, OnTabClosed) {
  const int64_t tab_id = 1;
  const GURL url("https://example.com/1");
  const base::Time navigation_timestamp = base::Time::Now() - base::Hours(1);
  const base::Time extraction_time = base::Time::Now();
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("test content");

  // Cache some content first so GetPageContentForTab can retrieve it.
  handler_->page_content_cache()->CachePageContent(
      tab_id, url, navigation_timestamp, extraction_time, page_context);
  CheckPageContentCached(tab_id, url, navigation_timestamp, extraction_time,
                         page_context);

  handler_->OnTabClosed(tab_id);

  // Verify content was deleted from the cache.
  CheckPageContentNotCached(tab_id);

  // Verify UMA.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabClose, 1);

  // Verify that the tab is marked as closed internally and its contents were
  // saved.
  EXPECT_TRUE(IsTabClosed(tab_id));
  CheckClosedTabCachedContent(tab_id, url, navigation_timestamp,
                              extraction_time, page_context);
}

TEST_F(PageContentCacheHandlerTest, OnTabCloseUndone) {
  const int64_t tab_id = 2;
  const GURL url("https://example.com/2");
  const base::Time navigation_timestamp = base::Time::Now() - base::Hours(2);
  const base::Time extraction_time = base::Time::Now();
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("undo content");

  // Simulate a tab close, which populates `closed_tabs_`.
  handler_->page_content_cache()->CachePageContent(
      tab_id, url, navigation_timestamp, extraction_time, page_context);
  CheckPageContentCached(tab_id, url, navigation_timestamp, extraction_time,
                         page_context);
  handler_->OnTabClosed(tab_id);
  CheckPageContentNotCached(tab_id);

  // Now undo the close.
  handler_->OnTabCloseUndone(tab_id);

  // Verify content is re-cached.
  CheckPageContentCached(tab_id, url, navigation_timestamp, extraction_time,
                         page_context);

  // Verify UMA.
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kContentsAvailableOnTabCloseUndone,
      1);

  // Verify that the tab is no longer marked as closed.
  EXPECT_FALSE(IsTabClosed(tab_id));
}

TEST_F(PageContentCacheHandlerTest, OnVisibilityChanged_HiddenWithContent) {
  const int64_t tab_id = 4;
  const GURL url("https://example.com/4");
  const base::Time navigation_timestamp = base::Time::Now() - base::Minutes(5);
  const base::Time extraction_time = base::Time::Now();
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("hidden content");

  handler_->OnVisibilityChanged(
      tab_id,
      CreateWebStateWrapper(url, PageContentVisibility::kHidden,
                            navigation_timestamp),
      page_context, extraction_time);

  // Use the helper to verify content is cached.
  CheckPageContentCached(tab_id, url, navigation_timestamp, extraction_time,
                         page_context);

  // Verify UMA.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kContentsAvailableWhenBackgrounded,
      1);
}

TEST_F(PageContentCacheHandlerTest, OnVisibilityChanged_NotHidden) {
  const int64_t tab_id = 6;
  const GURL url("https://example.com/6");
  const base::Time navigation_timestamp = base::Time::Now() - base::Minutes(5);
  const base::Time extraction_time = base::Time::Now();
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("visible content");

  handler_->OnVisibilityChanged(
      tab_id,
      CreateWebStateWrapper(url, PageContentVisibility::kVisible,
                            navigation_timestamp),
      page_context, extraction_time);

  // Verify content is NOT cached.
  CheckPageContentNotCached(tab_id);

  // Verify no UMA for background caching is recorded.
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus", 0);
}

TEST_F(PageContentCacheHandlerTest, OnVisibilityChanged_OffTheRecord) {
  const int64_t tab_id = 7;
  const GURL url("https://example.com/7");
  const base::Time navigation_timestamp = base::Time::Now() - base::Minutes(5);
  const base::Time extraction_time = base::Time::Now();
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("otr content");

  handler_->OnVisibilityChanged(
      tab_id,
      CreateWebStateWrapper(url, PageContentVisibility::kHidden,
                            navigation_timestamp,
                            /*is_off_the_record=*/true),
      page_context, extraction_time);

  // Verify content is NOT cached.
  CheckPageContentNotCached(tab_id);

  // Verify no UMA for background caching is recorded.
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus", 0);
}

TEST_F(PageContentCacheHandlerTest, OnNewNavigation) {
  const int64_t tab_id = 8;
  const GURL url("https://example.com/8");
  const base::Time navigation_timestamp = base::Time::Now() - base::Minutes(10);
  const base::Time extraction_time = base::Time::Now() - base::Minutes(5);
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("old content");

  // Cache some content first.
  handler_->page_content_cache()->CachePageContent(
      tab_id, url, navigation_timestamp, extraction_time, page_context);
  CheckPageContentCached(tab_id, url, navigation_timestamp, extraction_time,
                         page_context);

  handler_->OnNewNavigation(
      tab_id, CreateWebStateWrapper(GURL("https://example.com/8_new"),
                                    PageContentVisibility::kVisible));

  // Verify content is NOT cached.
  CheckPageContentNotCached(tab_id);

  // Verify UMA.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabUpdate, 1);
}

TEST_F(PageContentCacheHandlerTest, ProcessPageContentExtraction_InBackground) {
  const int64_t tab_id = 10;
  const GURL url("https://example.com/10");
  const base::Time navigation_timestamp = base::Time::Now() - base::Minutes(15);
  const base::Time extraction_time = base::Time::Now();
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("extracted background");

  handler_->ProcessPageContentExtraction(
      tab_id,
      CreateWebStateWrapper(url, PageContentVisibility::kHidden,
                            navigation_timestamp),
      page_context, extraction_time);

  // Use the helper to verify content is cached.
  CheckPageContentCached(tab_id, url, navigation_timestamp, extraction_time,
                         page_context);

  // Verify UMA.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kExtractionObservedInBackground,
      1);
}

TEST_F(PageContentCacheHandlerTest, ProcessPageContentExtraction_InForeground) {
  const int64_t tab_id = 11;
  const GURL url("https://example.com/11");
  const base::Time navigation_timestamp = base::Time::Now() - base::Minutes(15);
  const base::Time extraction_time = base::Time::Now();
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("extracted foreground");

  handler_->ProcessPageContentExtraction(
      tab_id,
      CreateWebStateWrapper(url, PageContentVisibility::kVisible,
                            navigation_timestamp),
      page_context, extraction_time);

  // Verify content is NOT cached.
  CheckPageContentNotCached(tab_id);

  // Verify UMA.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kExtractionObservedInForeground,
      1);
}

TEST_F(PageContentCacheHandlerTest, ProcessPageContentExtraction_OffTheRecord) {
  const int64_t tab_id = 12;
  const GURL url("https://example.com/12");
  const base::Time navigation_timestamp = base::Time::Now() - base::Minutes(15);
  const base::Time extraction_time = base::Time::Now();
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("extracted OTR");

  handler_->ProcessPageContentExtraction(
      tab_id,
      CreateWebStateWrapper(url, PageContentVisibility::kHidden,
                            navigation_timestamp,
                            /*is_off_the_record=*/true),
      page_context, extraction_time);

  // Verify content is NOT cached.
  CheckPageContentNotCached(tab_id);

  // Verify no UMA is recorded.
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus", 0);
}

TEST_F(PageContentCacheHandlerTest, ProcessPageContentExtraction_TabClosed) {
  const int64_t tab_id = 13;
  const GURL url("https://example.com/13");
  const base::Time navigation_timestamp = base::Time::Now() - base::Minutes(15);
  const base::Time extraction_time = base::Time::Now();
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("extracted closed tab");

  // Simulate tab close.
  handler_->OnTabClosed(tab_id);
  CheckPageContentNotCached(tab_id);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabClose, 1);

  // Now try to process extraction for this closed tab.
  handler_->ProcessPageContentExtraction(
      tab_id,
      CreateWebStateWrapper(url, PageContentVisibility::kHidden,
                            navigation_timestamp),
      page_context, extraction_time);

  // Verify content is NOT cached.
  CheckPageContentNotCached(tab_id);

  // Verify no additional UMA is recorded for extraction.
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus", 1);
}

TEST_F(PageContentCacheHandlerTest, OnTabCloseUndone_NoContent) {
  const int64_t tab_id = 3;

  // Simulate a tab close. This tab has no cached content.
  handler_->OnTabClosed(tab_id);
  CheckPageContentNotCached(tab_id);
  EXPECT_TRUE(IsTabClosed(tab_id));

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabClose, 1);

  // Now undo the close.
  handler_->OnTabCloseUndone(tab_id);

  // Verify that the tab is no longer marked as closed.
  EXPECT_FALSE(IsTabClosed(tab_id));

  // Verify no *additional* UMA is recorded.
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus", 1);
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kContentsAvailableOnTabCloseUndone,
      0);
}

TEST_F(PageContentCacheHandlerTest, TabClosureCommitted) {
  const int64_t tab_id = 14;

  // Simulate a tab close, which populates `closed_tabs_`.
  handler_->OnTabClosed(tab_id);
  CheckPageContentNotCached(tab_id);

  EXPECT_TRUE(IsTabClosed(tab_id));

  // Now commit the close.
  handler_->TabClosureCommitted(tab_id);

  // Verify content is NOT cached.
  CheckPageContentNotCached(tab_id);

  // Verify that the tab is still marked as closed.
  EXPECT_TRUE(IsTabClosed(tab_id));

  // Verify that the tab is no longer in the `closed_tabs_` map.
  CheckClosedTabNotCached(tab_id);

  // Verify that the tab is in the `committed_closed_tabs_` set.
  CheckCommittedClosedTab(tab_id, true);

  // Verify UMA from OnTabClosed.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabClose, 1);
}

TEST_F(PageContentCacheHandlerTest, OnNewNavigation_TabWasClosed) {
  const int64_t tab_id = 15;
  const GURL url("https://example.com/15");
  const base::Time navigation_timestamp = base::Time::Now() - base::Minutes(10);
  const base::Time extraction_time = base::Time::Now() - base::Minutes(5);
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("old content for closed tab");

  // Cache some content and close the tab.
  handler_->page_content_cache()->CachePageContent(
      tab_id, url, navigation_timestamp, extraction_time, page_context);
  CheckPageContentCached(tab_id, url, navigation_timestamp, extraction_time,
                         page_context);
  handler_->OnTabClosed(tab_id);
  CheckPageContentNotCached(tab_id);
  EXPECT_TRUE(IsTabClosed(tab_id));

  // Now a new navigation happens for the same tab id.
  handler_->OnNewNavigation(
      tab_id, CreateWebStateWrapper(GURL("https://example.com/15_new"),
                                    PageContentVisibility::kVisible));

  // Verify content is NOT cached.
  CheckPageContentNotCached(tab_id);
  // Verify that the tab is still marked as closed.
  EXPECT_TRUE(IsTabClosed(tab_id));

  // Verify UMA.
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabClose, 1);
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabUpdate, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus", 2);
}

TEST_F(PageContentCacheHandlerTest, OnVisibilityChanged_TabClosed) {
  const int64_t tab_id = 16;
  const GURL url("https://example.com/16");
  const base::Time navigation_timestamp = base::Time::Now() - base::Minutes(5);
  const base::Time extraction_time = base::Time::Now();
  const optimization_guide::proto::PageContext page_context =
      CreatePageContext("hidden content for closed tab");

  // Close the tab.
  handler_->OnTabClosed(tab_id);
  CheckPageContentNotCached(tab_id);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabClose, 1);

  handler_->OnVisibilityChanged(
      tab_id,
      CreateWebStateWrapper(url, PageContentVisibility::kHidden,
                            navigation_timestamp),
      page_context, extraction_time);

  // Verify content is NOT cached.
  CheckPageContentNotCached(tab_id);
  // Still closed.
  EXPECT_TRUE(IsTabClosed(tab_id));

  // Verify no additional UMA is recorded.
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus", 1);
}

}  // namespace page_content_annotations
