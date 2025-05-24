// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

class SourceUrlRecorderWebContentsObserverBrowserTest
    : public content::ContentBrowserTest {
 public:
  SourceUrlRecorderWebContentsObserverBrowserTest(
      const SourceUrlRecorderWebContentsObserverBrowserTest&) = delete;
  SourceUrlRecorderWebContentsObserverBrowserTest& operator=(
      const SourceUrlRecorderWebContentsObserverBrowserTest&) = delete;

 protected:
  SourceUrlRecorderWebContentsObserverBrowserTest() {
    scoped_feature_list_.InitWithFeatures({ukm::kUkmFeature}, {});
  }

  ~SourceUrlRecorderWebContentsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ukm::InitializeSourceUrlRecorderForWebContents(shell()->web_contents());
  }

  const ukm::UkmSource* GetSourceForSourceId(const ukm::SourceId source_id) {
    return test_ukm_recorder_->GetSourceForSourceId(source_id);
  }

  const ukm::UkmSource* GetSourceForNavigationId(int64_t navigation_id) {
    CHECK_GT(navigation_id, 0);
    return GetSourceForSourceId(ukm::ConvertToSourceId(
        navigation_id, ukm::SourceIdType::NAVIGATION_ID));
  }

  GURL GetAssociatedURLForWebContentsDocument() {
    const ukm::UkmSource* src = test_ukm_recorder_->GetSourceForSourceId(
        shell()->web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
    return src ? src->url() : GURL();
  }

  const ukm::TestAutoSetUkmRecorder& test_ukm_recorder() const {
    return *test_ukm_recorder_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
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
                       WindowOpenLogsOpenerSource) {
  EXPECT_TRUE(content::NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title1.html")));

  const ukm::UkmSource* old_src = test_ukm_recorder().GetSourceForSourceId(
      shell()->web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  EXPECT_NE(nullptr, old_src);

  // Open a new tab via window.open
  content::ShellAddedObserver shell_observer;
  GURL new_url = embedded_test_server()->GetURL("/title2.html");
  content::TestNavigationObserver nav_observer(new_url);
  nav_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecJs(
      shell(), content::JsReplace("window.open($1)", new_url.path())));
  nav_observer.Wait();
  content::Shell* new_window = shell_observer.GetShell();
  content::WebContents* new_contents = new_window->web_contents();

  const ukm::UkmSource* new_src = test_ukm_recorder().GetSourceForSourceId(
      new_contents->GetPrimaryMainFrame()->GetPageUkmSourceId());

  EXPECT_NE(nullptr, new_src);
  EXPECT_EQ(new_src->navigation_data().opener_source_id, old_src->id());
}

// Test correctness of sources and DocumentCreated entries when a navigation
// leads to creation of main frame and embedded subframe documents.
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

  const ukm::UkmSource* navigation_source =
      GetSourceForNavigationId(main_observer.navigation_id());
  EXPECT_NE(nullptr, navigation_source);
  EXPECT_EQ(main_url, navigation_source->url());
  EXPECT_EQ(nullptr,
            GetSourceForNavigationId(subframe_observer.navigation_id()));

  EXPECT_EQ(main_url, GetAssociatedURLForWebContentsDocument());

  // Check we have created one DocumentCreated event for each of the frames.
  auto ukm_entries = test_ukm_recorder().GetEntriesByName(Entry::kEntryName);
  EXPECT_EQ(2u, ukm_entries.size());
  // Both events have the same navigation source id, and have different values
  // for the kIsMainFrameName metric.
  EXPECT_EQ(navigation_source->id(),
            *test_ukm_recorder().GetEntryMetric(
                ukm_entries[0], Entry::kNavigationSourceIdName));
  EXPECT_EQ(navigation_source->id(),
            *test_ukm_recorder().GetEntryMetric(
                ukm_entries[1], Entry::kNavigationSourceIdName));
  EXPECT_NE(*test_ukm_recorder().GetEntryMetric(ukm_entries[0],
                                                Entry::kIsMainFrameName),
            *test_ukm_recorder().GetEntryMetric(ukm_entries[1],
                                                Entry::kIsMainFrameName));

  // The two DocumentCreated entries have source ids corresponding to the
  // document source ids, which are different from the id of the navigation
  // source.
  EXPECT_NE(ukm_entries[0]->source_id, ukm_entries[1]->source_id);
  EXPECT_NE(navigation_source->id(), ukm_entries[0]->source_id);
  EXPECT_NE(navigation_source->id(), ukm_entries[1]->source_id);
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

class SourceUrlRecorderWebContentsObserverPrerenderBrowserTest
    : public SourceUrlRecorderWebContentsObserverBrowserTest {
 public:
  SourceUrlRecorderWebContentsObserverPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SourceUrlRecorderWebContentsObserverPrerenderBrowserTest::
                web_contents,
            base::Unretained(this))) {}
  ~SourceUrlRecorderWebContentsObserverPrerenderBrowserTest() override =
      default;

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(SourceUrlRecorderWebContentsObserverPrerenderBrowserTest,
                       IgnoreUrlInPrerender) {
  GURL url = embedded_test_server()->GetURL("/title1.html");
  content::NavigationHandleObserver observer(web_contents(), url);
  EXPECT_TRUE(content::NavigateToURL(shell(), url));
  EXPECT_TRUE(observer.has_committed());
  const ukm::UkmSource* source1 =
      GetSourceForNavigationId(observer.navigation_id());
  EXPECT_NE(nullptr, source1);
  EXPECT_EQ(1u, source1->urls().size());
  EXPECT_EQ(url, source1->url());
  EXPECT_EQ(url, GetAssociatedURLForWebContentsDocument());

  // Load a page in the prerendering.
  GURL prerender_url =
      embedded_test_server()->GetURL("/title1.html?prerendering");
  content::NavigationHandleObserver prerender_observer(web_contents(),
                                                       prerender_url);
  prerender_helper()->AddPrerender(prerender_url);

  // Ensure no UKM source was created for the prerendering navigation.
  EXPECT_EQ(nullptr,
            GetSourceForNavigationId(prerender_observer.navigation_id()));

  EXPECT_EQ(url, GetAssociatedURLForWebContentsDocument());

  // Navigate the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  const ukm::UkmSource* source2 =
      GetSourceForNavigationId(prerender_observer.navigation_id());
  EXPECT_EQ(1u, source2->urls().size());
  EXPECT_EQ(prerender_url, source2->url());
  GURL expected_ukm_url;
  // TODO(crbug.com/40195952): The URL is not assigned yet for prerendering
  // UKM source ids, so expect it to not be set.
  // expected_ukm_url = prerender_url;
  EXPECT_EQ(expected_ukm_url, GetAssociatedURLForWebContentsDocument());
  EXPECT_NE(source1, source2);
}
