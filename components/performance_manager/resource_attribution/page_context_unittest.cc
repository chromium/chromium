// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/page_context.h"

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_attribution {

namespace {

using ResourceAttrPageContextTest =
    performance_manager::PerformanceManagerTestHarness;
using ResourceAttrPageContextNoPMTest = content::RenderViewHostTestHarness;

TEST_F(ResourceAttrPageContextTest, PageContexts) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  std::optional<PageContext> page_context =
      PageContext::FromWebContents(web_contents.get());
  ASSERT_TRUE(page_context.has_value());
  EXPECT_EQ(web_contents.get(), page_context->GetWebContents());

  base::WeakPtr<PageNode> page_node = page_context->GetWeakPageNode();
  base::WeakPtr<PageNode> page_node_from_pm =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents.get());
  performance_manager::RunInGraph([&] {
    ASSERT_TRUE(page_node);
    ASSERT_TRUE(page_node_from_pm);
    EXPECT_EQ(page_node.get(), page_node_from_pm.get());

    EXPECT_EQ(page_node.get(), page_context->GetPageNode());
    EXPECT_EQ(page_context.value(), page_node->GetResourceContext());
    EXPECT_EQ(page_context.value(), PageContext::FromPageNode(page_node.get()));
    EXPECT_EQ(page_context.value(), PageContext::FromWeakPageNode(page_node));
  });

  // Navigating the page should not change the PageContext since it corresponds
  // to the WebContents, not the document.
  content::RenderFrameHost* rfh =
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents.get(), GURL("https://a.com/"));
  ASSERT_TRUE(rfh);
  std::optional<PageContext> page_context_after_nav =
      PageContext::FromWebContents(
          content::WebContents::FromRenderFrameHost(rfh));
  EXPECT_EQ(page_context, page_context_after_nav);

  // Make sure a second page gets a different context.
  std::unique_ptr<content::WebContents> web_contents2 = CreateTestWebContents();
  std::optional<PageContext> page_context2 =
      PageContext::FromWebContents(web_contents2.get());
  EXPECT_TRUE(page_context2.has_value());
  EXPECT_NE(page_context2, page_context);

  web_contents.reset();

  EXPECT_EQ(nullptr, page_context->GetWebContents());
  performance_manager::RunInGraph([&] {
    EXPECT_FALSE(page_node);
    EXPECT_EQ(nullptr, page_context->GetPageNode());
    EXPECT_EQ(std::nullopt, PageContext::FromWeakPageNode(page_node));
  });

  // The unique id of a PageContext isn't exposed so can't be tested directly.
  // But after deleting a page, its PageContexts should still compare equal,
  // showing that they're compared by a permanent id, not live data.
  EXPECT_EQ(page_context, page_context_after_nav);
  EXPECT_NE(page_context, page_context2);

  // Make sure PageContext id's aren't reused.
  std::unique_ptr<content::WebContents> web_contents3 = CreateTestWebContents();
  std::optional<PageContext> page_context3 =
      PageContext::FromWebContents(web_contents3.get());
  EXPECT_NE(page_context, page_context3);
  EXPECT_NE(page_context2, page_context3);
}

TEST_F(ResourceAttrPageContextNoPMTest, PageContextWithoutPM) {
  // When PerformanceManager isn't initialized, factory functions should return
  // nullopt, not a context that's missing PM info.
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  // Verify that PM didn't see the page.
  performance_manager::RunInGraph(
      [node = PerformanceManager::GetPrimaryPageNodeForWebContents(
           web_contents.get())] { EXPECT_FALSE(node); });

  EXPECT_FALSE(PageContext::FromWebContents(web_contents.get()));
}

}  // namespace

}  // namespace resource_attribution
