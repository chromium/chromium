// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/frame_context.h"

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/gurl.h"

namespace resource_attribution {

namespace {

using ResourceAttrFrameContextTest =
    performance_manager::PerformanceManagerTestHarness;
using ResourceAttrFrameContextNoPMTest = content::RenderViewHostTestHarness;

TEST_F(ResourceAttrFrameContextTest, FrameContexts) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  // Navigate to an initial page.
  content::RenderFrameHost* rfh =
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents.get(), GURL("https://a.com/"));
  ASSERT_TRUE(rfh);
  const content::GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  std::optional<FrameContext> frame_context =
      FrameContext::FromRenderFrameHost(rfh);
  ASSERT_TRUE(frame_context.has_value());
  EXPECT_EQ(rfh, frame_context->GetRenderFrameHost());
  EXPECT_EQ(rfh_id, frame_context->GetRenderFrameHostId());

  base::WeakPtr<FrameNode> frame_node = frame_context->GetWeakFrameNode();
  base::WeakPtr<FrameNode> frame_node_from_pm =
      PerformanceManager::GetFrameNodeForRenderFrameHost(rfh);
  ASSERT_TRUE(frame_node);
  ASSERT_TRUE(frame_node_from_pm);
  EXPECT_EQ(frame_node.get(), frame_node_from_pm.get());

  EXPECT_EQ(frame_node.get(), frame_context->GetFrameNode());
  EXPECT_EQ(frame_context.value(), frame_node->GetResourceContext());
  EXPECT_EQ(frame_context.value(),
            FrameContext::FromFrameNode(frame_node.get()));
  EXPECT_EQ(frame_context.value(), FrameContext::FromWeakFrameNode(frame_node));

  // Make sure a second frame gets a different context.
  std::unique_ptr<content::WebContents> web_contents2 = CreateTestWebContents();
  content::RenderFrameHost* rfh2 =
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents2.get(), GURL("https://b.com/"));
  EXPECT_TRUE(rfh2);
  EXPECT_NE(rfh, rfh2);

  std::optional<FrameContext> frame_context2 =
      FrameContext::FromRenderFrameHost(rfh2);
  EXPECT_TRUE(frame_context2.has_value());
  EXPECT_NE(frame_context2, frame_context);

  // Put contexts in an absl set to make sure they can be hashed.
  absl::flat_hash_set<FrameContext> context_set{frame_context.value(),
                                                frame_context2.value()};
  EXPECT_EQ(context_set.size(), 2u);

  web_contents.reset();

  EXPECT_EQ(nullptr, frame_context->GetRenderFrameHost());
  EXPECT_EQ(rfh_id, frame_context->GetRenderFrameHostId());

  EXPECT_FALSE(frame_node);
  EXPECT_EQ(nullptr, frame_context->GetFrameNode());
  EXPECT_EQ(std::nullopt, FrameContext::FromWeakFrameNode(frame_node));
}

TEST_F(ResourceAttrFrameContextNoPMTest, FrameContextWithoutPM) {
  // There can be a delay between creating a RenderFrameHost and
  // PerformanceManager being notified of the new host. Simulate looking up a
  // FrameContext during that time by bypassing the PM test harness to create a
  // WebContents without the PM helpers attached.
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  // Navigate to an initial page.
  content::RenderFrameHost* rfh =
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents.get(), GURL("https://a.com/"));
  ASSERT_TRUE(rfh);

  // Verify that PM didn't see the frame.
  base::WeakPtr<FrameNode> node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(rfh);
  EXPECT_FALSE(node);

  // FromRenderFrameHost() should return nullopt, not a context that's missing
  // PM info.
  ASSERT_FALSE(FrameContext::FromRenderFrameHost(rfh).has_value());
}

}  // namespace

}  // namespace resource_attribution
