// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "third_party/blink/public/common/features.h"

class SourceUrlRecorderWebContentsObserverBrowserTest
    : public content::ContentBrowserTest {
 protected:
  SourceUrlRecorderWebContentsObserverBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {ukm::kUkmFeature, blink::features::kPortals}, {});
  }

  ~SourceUrlRecorderWebContentsObserverBrowserTest() override {}

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

IN_PROC_BROWSER_TEST_F(SourceUrlRecorderWebContentsObserverBrowserTest,
                       Portal) {
  GURL url = embedded_test_server()->GetURL("/title1.html");
  {
    content::NavigationHandleObserver observer(shell()->web_contents(), url);
    EXPECT_TRUE(content::NavigateToURL(shell(), url));
    const ukm::UkmSource* source =
        GetSourceForNavigationId(observer.navigation_id());
    EXPECT_NE(nullptr, source);
    EXPECT_EQ(url, source->url());
  }

  // Create the portal and get its associated WebContents.
  content::WebContentsAddedObserver contents_observer;
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             "document.body.appendChild(document.createElement('portal'));"));
  content::WebContents* portal_contents = contents_observer.GetWebContents();
  EXPECT_TRUE(portal_contents->IsPortal());
  EXPECT_NE(portal_contents, shell()->web_contents());

  // Register the UKM SourceUrlRecorder for the portal WebContents. Normally
  // this would be done automatically via TabHelper registration, but TabHelper
  // registration doesn't run in //content.
  ukm::InitializeSourceUrlRecorderForWebContents(portal_contents);

  // Navigate the portal and wait for the navigation to finish.
  GURL portal_url = embedded_test_server()->GetURL("/title2.html");
  content::TestNavigationManager portal_nav_manager(portal_contents,
                                                    portal_url);
  content::NavigationHandleObserver portal_nav_observer(portal_contents,
                                                        portal_url);
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             base::StringPrintf("document.querySelector('portal').src = '%s';",
                                portal_url.spec().c_str())));
  portal_nav_manager.WaitForNavigationFinished();
  EXPECT_TRUE(portal_nav_observer.has_committed());
  EXPECT_FALSE(portal_nav_observer.is_error());

  // Ensure no UKM source was created for the portal navigation.
  // TODO(crbug/1078355): Ensure the source was created but no URL was recorded.
  EXPECT_EQ(nullptr,
            GetSourceForNavigationId(portal_nav_observer.navigation_id()));

  // Activate the portal, which should cause a URL to be recorded for the
  // associated UKM source. Activation is similar to a main frame navigation
  // from the user standpoint, as it causes the portal to shift from being in a
  // more iframe-like state to becoming the main frame in the associated browser
  // tab.
  std::string activated_listener = R"(
    activated = false;
    window.addEventListener('portalactivate', e => {
      activated = true;
    });
  )";
  EXPECT_TRUE(ExecJs(portal_contents, activated_listener));
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "document.querySelector('portal').activate()"));

  std::string activated_poll = R"(
    setInterval(() => {
      if (activated)
        window.domAutomationController.send(true);
    }, 10);
  )";
  // EvalJsWithManualReply returns an EvalJsResult, whose docs say to use
  // EXPECT_EQ(true, ...) rather than EXPECT_TRUE(), as the latter does not
  // compile.
  EXPECT_EQ(true, EvalJsWithManualReply(portal_contents, activated_poll));

  // The activated portal contents should be the currently active contents.
  EXPECT_EQ(portal_contents, shell()->web_contents());

  // TODO(crbug/1078143): enable when UKM sources are created for activated
  // portals.
#if 0
  // Ensure a UKM source was created for the activated portal, and a URL was
  // recorded.
  const ukm::UkmSource* portal_source =
      GetSourceForNavigationId(portal_nav_observer.navigation_id());
  EXPECT_NE(nullptr, portal_source);
  EXPECT_EQ(portal_url, portal_source->url());
#endif

  {
    // Ensure a UKM source is recorded for navigations in an activated portal.
    content::NavigationHandleObserver observer(portal_contents, url);
    EXPECT_TRUE(content::NavigateToURL(shell(), url));
    EXPECT_NE(nullptr, GetSourceForNavigationId(observer.navigation_id()));
  }
}
