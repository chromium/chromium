// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

class SourceUrlRecorderWebContentsObserverBrowserTest
    : public content::ContentBrowserTest {
 protected:
  SourceUrlRecorderWebContentsObserverBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(ukm::kUkmFeature);
  }

  ~SourceUrlRecorderWebContentsObserverBrowserTest() override {}

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ukm::InitializeSourceUrlRecorderForWebContents(shell()->web_contents());
  }

  const ukm::UkmSource* GetSourceForNavigationId(int64_t navigation_id) {
    CHECK_GT(navigation_id, 0);
    const ukm::SourceId source_id =
        ukm::ConvertToSourceId(navigation_id, ukm::SourceIdType::NAVIGATION_ID);
    return test_ukm_recorder_->GetSourceForSourceId(source_id);
  }

  GURL GetAssociatedURLForWebContentsDocument() {
    const ukm::UkmSource* src = test_ukm_recorder_->GetSourceForSourceId(
        ukm::GetSourceIdForWebContentsDocument(shell()->web_contents()));
    return src ? src->url() : GURL();
  }

  const ukm::TestAutoSetUkmRecorder& test_ukm_recorder() const {
    return *test_ukm_recorder_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(SourceUrlRecorderWebContentsObserverBrowserTest);
};

class SourceUrlRecorderWebContentsObserverDownloadBrowserTest
    : public SourceUrlRecorderWebContentsObserverBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    SourceUrlRecorderWebContentsObserverBrowserTest::SetUpOnMainThread();

    // Set up a test download directory, in order to prevent prompting for
    // handling downloads.
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    content::ShellDownloadManagerDelegate* delegate =
        static_cast<content::ShellDownloadManagerDelegate*>(
            shell()
                ->web_contents()
                ->GetBrowserContext()
                ->GetDownloadManagerDelegate());
    delegate->SetDownloadBehaviorForTesting(downloads_directory_.GetPath());
  }

 private:
  base::ScopedTempDir downloads_directory_;
};

IN_PROC_BROWSER_TEST_F(SourceUrlRecorderWebContentsObserverBrowserTest, Basic) {
  using Entry = ukm::builders::DocumentCreated;

  GURL url = embedded_test_server()->GetURL("/title1.html");
  content::NavigationHandleObserver observer(shell()->web_contents(), url);
  EXPECT_TRUE(content::NavigateToURL(shell(), url));
  EXPECT_TRUE(observer.has_committed());
  const ukm::UkmSource* source =
      GetSourceForNavigationId(observer.navigation_id());
  EXPECT_NE(nullptr, source);
  EXPECT_EQ(url, source->url());
  EXPECT_EQ(1u, source->urls().size());

  EXPECT_EQ(url, GetAssociatedURLForWebContentsDocument());

  // Check we have created a DocumentCreated event.
  auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  EXPECT_EQ(source->id(), *test_ukm_recorder().GetEntryMetric(
                              ukm_entries[0], Entry::kNavigationSourceIdName));
  EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(ukm_entries[0],
                                                   Entry::kIsMainFrameName));
  EXPECT_NE(source->id(), ukm_entries[0]->source_id);
}

IN_PROC_BROWSER_TEST_F(SourceUrlRecorderWebContentsObserverBrowserTest,
                       IgnoreUrlInSubframe) {
  using Entry = ukm::builders::DocumentCreated;

  GURL main_url = embedded_test_server()->GetURL("/page_with_iframe.html");
  GURL subframe_url = embedded_test_server()->GetURL("/title1.html");

  content::NavigationHandleObserver main_observer(shell()->web_contents(),
                                                  main_url);
  content::NavigationHandleObserver subframe_observer(shell()->web_contents(),
                                                      subframe_url);
  EXPECT_TRUE(content::NavigateToURL(shell(), main_url));
  EXPECT_TRUE(main_observer.has_committed());
  EXPECT_TRUE(main_observer.is_main_frame());
  EXPECT_TRUE(subframe_observer.has_committed());
  EXPECT_FALSE(subframe_observer.is_main_frame());

  const ukm::UkmSource* source =
      GetSourceForNavigationId(main_observer.navigation_id());
  EXPECT_NE(nullptr, source);
  EXPECT_EQ(main_url, source->url());
  EXPECT_EQ(nullptr,
            GetSourceForNavigationId(subframe_observer.navigation_id()));

  EXPECT_EQ(main_url, GetAssociatedURLForWebContentsDocument());

  // Check we have created a DocumentCreated event for both frames.
  auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
  EXPECT_EQ(2u, ukm_entries.size());
  EXPECT_EQ(source->id(), *test_ukm_recorder().GetEntryMetric(
                              ukm_entries[0], Entry::kNavigationSourceIdName));
  EXPECT_EQ(0, *test_ukm_recorder().GetEntryMetric(ukm_entries[0],
                                                   Entry::kIsMainFrameName));
  EXPECT_EQ(source->id(), *test_ukm_recorder().GetEntryMetric(
                              ukm_entries[1], Entry::kNavigationSourceIdName));
  EXPECT_EQ(1, *test_ukm_recorder().GetEntryMetric(ukm_entries[1],
                                                   Entry::kIsMainFrameName));
  EXPECT_NE(ukm_entries[0]->source_id, ukm_entries[1]->source_id);
  EXPECT_NE(source->id(), ukm_entries[0]->source_id);
  EXPECT_NE(source->id(), ukm_entries[1]->source_id);
}

IN_PROC_BROWSER_TEST_F(SourceUrlRecorderWebContentsObserverDownloadBrowserTest,
                       IgnoreDownload) {
  GURL url(embedded_test_server()->GetURL("/download-test1.lib"));
  content::NavigationHandleObserver observer(shell()->web_contents(), url);
  EXPECT_TRUE(content::NavigateToURLAndExpectNoCommit(shell(), url));
  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_download());
  EXPECT_EQ(nullptr, GetSourceForNavigationId(observer.navigation_id()));
  EXPECT_EQ(GURL(), GetAssociatedURLForWebContentsDocument());
}
