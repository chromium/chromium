// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

class Graph;

// Tests that the PerformanceManager node states are updated correctly during
// prerendering.
//
// TODO(crbug.com/40182881): These tests assume prerendering frames are added as
// extra FrameNodes on the existing PageNode. Update this logic once
// prerendering frame trees have their own PageNode.
class PerformanceManagerPrerenderingBrowserTest
    : public PerformanceManagerBrowserTestHarness {
 public:
  using Super = PerformanceManagerBrowserTestHarness;

  PerformanceManagerPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PerformanceManagerPrerenderingBrowserTest::web_contents,
            base::Unretained(this))) {
    prerender_helper_.RegisterServerRequestMonitor(&ssl_server_);
  }

  void SetUpOnMainThread() override {
    Super::SetUpOnMainThread();
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(ssl_server_.Start());

    // TODO(crbug.com/40172688): PrerenderHost is not deleted when the
    // page enters BackForwardCache, though it should be. While this
    // functionality is not implemented, disable BackForwardCache for testing
    // and wait for the old RenderFrameHost to be deleted after we navigate away
    // from it.
    content::DisableBackForwardCacheForTesting(
        web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(ssl_server_.ShutdownAndWaitUntilComplete());
    Super::TearDownOnMainThread();
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

  GURL GetUrl(const std::string& path) {
    return ssl_server_.GetURL("a.test", path);
  }

 protected:
  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(PerformanceManagerPrerenderingBrowserTest,
                       PrerenderingFinishes) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page. Test that PM has a PageNode for it, and
  // GetMainFrameNode returns its main frame.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kInitialUrl));
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  const FrameNode* initial_main_frame_node = nullptr;
  int64_t initial_navigation_id = 0;
  RunInGraph([&](Graph*) {
    ASSERT_TRUE(page_node);
    EXPECT_EQ(page_node->GetMainFrameNodes().size(), 1U);
    initial_main_frame_node = page_node->GetMainFrameNode();
    initial_navigation_id = page_node->GetNavigationID();
    EXPECT_EQ(page_node->GetMainFrameUrl(), kInitialUrl);
    EXPECT_TRUE(initial_main_frame_node->IsCurrent());
  });

  // Start prerendering a document. Test that the prerendering frame tree is
  // added as additional frame nodes, but GetMainFrameNode is unchanged.
  prerender_helper_.AddPrerender(kPrerenderingUrl);
  base::WeakPtr<PageNode> page_node2 =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  const FrameNode* prerender_main_frame_node = nullptr;
  RunInGraph([&](Graph*) {
    ASSERT_TRUE(page_node);
    ASSERT_EQ(page_node.get(), page_node2.get());
    EXPECT_EQ(page_node->GetMainFrameNodes().size(), 2U);
    EXPECT_EQ(page_node->GetMainFrameNode(), initial_main_frame_node);
    EXPECT_TRUE(initial_main_frame_node->IsCurrent());

    // Find the prerendering MainFrameNode.
    for (const FrameNode* frame_node : page_node->GetMainFrameNodes()) {
      if (frame_node != initial_main_frame_node) {
        prerender_main_frame_node = frame_node;
        break;
      }
    }
    ASSERT_TRUE(prerender_main_frame_node);
    EXPECT_EQ(prerender_main_frame_node->GetURL(), kPrerenderingUrl);
    EXPECT_FALSE(prerender_main_frame_node->IsCurrent());

    // The prerendering navigation should not be reflected in the PageNode.
    EXPECT_EQ(page_node->GetNavigationID(), initial_navigation_id);
    EXPECT_EQ(page_node->GetMainFrameUrl(), kInitialUrl);
  });

  // Activate the prerendered document. Test that GetMainFrameNode now returns
  // its main frame, and the original frame tree is gone.
  content::RenderFrameDeletedObserver deleted_observer(
      web_contents()->GetPrimaryMainFrame());
  content::test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                          kPrerenderingUrl);
  prerender_helper_.NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_TRUE(prerender_observer.was_activated());
  deleted_observer.WaitUntilDeleted();
  RunInGraph([&](Graph*) {
    ASSERT_TRUE(page_node);
    EXPECT_EQ(page_node->GetMainFrameNodes().size(), 1U);
    EXPECT_EQ(page_node->GetMainFrameNode(), prerender_main_frame_node);
    EXPECT_TRUE(prerender_main_frame_node->IsCurrent());

    // Now the PageNode should reflect the prerendering navigation.
    EXPECT_NE(page_node->GetNavigationID(), initial_navigation_id);
    EXPECT_EQ(page_node->GetMainFrameUrl(), kPrerenderingUrl);
  });
}

IN_PROC_BROWSER_TEST_F(PerformanceManagerPrerenderingBrowserTest,
                       PrerenderingCancelled) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  const GURL kFinalUrl = GetUrl("/empty.html?elsewhere");

  // Navigate to an initial page. Test that PM has a PageNode for it, and
  // GetMainFrameNode returns its main frame.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kInitialUrl));
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  const FrameNode* initial_main_frame_node = nullptr;
  int64_t initial_navigation_id = 0;
  RunInGraph([&](Graph*) {
    ASSERT_TRUE(page_node);
    EXPECT_EQ(page_node->GetMainFrameNodes().size(), 1U);
    initial_main_frame_node = page_node->GetMainFrameNode();
    initial_navigation_id = page_node->GetNavigationID();
    EXPECT_EQ(page_node->GetMainFrameUrl(), kInitialUrl);
    EXPECT_TRUE(initial_main_frame_node->IsCurrent());
  });

  // Start prerendering a document. Test that the prerendering frame tree is
  // added as additional frame nodes, but GetMainFrameNode is unchanged.
  content::FrameTreeNodeId prerender_host =
      prerender_helper_.AddPrerender(kPrerenderingUrl);
  base::WeakPtr<PageNode> page_node2 =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  const FrameNode* prerender_main_frame_node = nullptr;
  RunInGraph([&](Graph*) {
    ASSERT_TRUE(page_node);
    ASSERT_EQ(page_node.get(), page_node2.get());
    EXPECT_EQ(page_node->GetMainFrameNodes().size(), 2U);
    EXPECT_EQ(page_node->GetMainFrameNode(), initial_main_frame_node);
    EXPECT_TRUE(initial_main_frame_node->IsCurrent());

    // Find the prerendering MainFrameNode.
    for (const FrameNode* frame_node : page_node->GetMainFrameNodes()) {
      if (frame_node != initial_main_frame_node) {
        prerender_main_frame_node = frame_node;
        break;
      }
    }
    ASSERT_TRUE(prerender_main_frame_node);
    EXPECT_EQ(prerender_main_frame_node->GetURL(), kPrerenderingUrl);
    EXPECT_FALSE(prerender_main_frame_node->IsCurrent());

    // The prerendering navigation should not be reflected in the PageNode.
    EXPECT_EQ(page_node->GetNavigationID(), initial_navigation_id);
    EXPECT_EQ(page_node->GetMainFrameUrl(), kInitialUrl);
  });

  // Navigate the main frame to another page. Test that the prerendering frame
  // tree is removed from PerformanceManager.
  content::RenderFrameDeletedObserver deleted_observer(
      prerender_helper_.GetPrerenderedMainFrameHost(prerender_host));
  bool rfh_should_change =
      web_contents()
          ->GetPrimaryMainFrame()
          ->ShouldChangeRenderFrameHostOnSameSiteNavigation();
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kFinalUrl));
  deleted_observer.WaitUntilDeleted();
  RunInGraph([&](Graph*) {
    ASSERT_TRUE(page_node);
    EXPECT_EQ(page_node->GetMainFrameNodes().size(), 1U);
    // The RenderFrameHost might change after the navigation if RenderDocument
    // is enabled.
    EXPECT_EQ(rfh_should_change,
              page_node->GetMainFrameNode() != initial_main_frame_node);
    EXPECT_EQ(page_node->GetMainFrameUrl(), kFinalUrl);
    EXPECT_TRUE(page_node->GetMainFrameNode()->IsCurrent());
  });
}

}  // namespace performance_manager
