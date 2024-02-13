// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/process_context.h"

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "components/performance_manager/test_support/test_browser_child_process.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_attribution {

namespace {

using TestBrowserChildProcess = performance_manager::TestBrowserChildProcess;

using ResourceAttrProcessContextTest =
    performance_manager::PerformanceManagerTestHarness;
using ResourceAttrProcessContextNoPMTest = content::RenderViewHostTestHarness;

TEST_F(ResourceAttrProcessContextTest, BrowserProcessContext) {
  // PerformanceManager creates a browser ProcessNode when the test harness
  // initializes it.
  const std::optional<ProcessContext> process_context =
      ProcessContext::FromBrowserProcess();
  ASSERT_TRUE(process_context.has_value());
  EXPECT_TRUE(process_context->IsBrowserProcessContext());

  EXPECT_FALSE(process_context->IsRenderProcessContext());
  EXPECT_EQ(nullptr, process_context->GetRenderProcessHost());
  EXPECT_TRUE(process_context->GetRenderProcessHostId().is_null());
  EXPECT_FALSE(process_context->IsBrowserChildProcessContext());
  EXPECT_EQ(nullptr, process_context->GetBrowserChildProcessHost());
  EXPECT_TRUE(process_context->GetBrowserChildProcessHostId().is_null());

  base::WeakPtr<ProcessNode> process_node =
      process_context->GetWeakProcessNode();
  base::WeakPtr<ProcessNode> process_node_from_pm =
      PerformanceManager::GetProcessNodeForBrowserProcess();
  performance_manager::RunInGraph([&] {
    ASSERT_TRUE(process_node);
    ASSERT_TRUE(process_node_from_pm);
    EXPECT_EQ(process_node.get(), process_node_from_pm.get());

    EXPECT_EQ(process_node.get(), process_context->GetProcessNode());
    EXPECT_EQ(process_context.value(), process_node->GetResourceContext());
    EXPECT_EQ(process_context.value(),
              ProcessContext::FromProcessNode(process_node.get()));
    EXPECT_EQ(process_context.value(),
              ProcessContext::FromWeakProcessNode(process_node));
  });

  performance_manager::DeleteBrowserProcessNodeForTesting();

  EXPECT_TRUE(process_context->IsBrowserProcessContext());
  performance_manager::RunInGraph([&] {
    EXPECT_FALSE(process_node);
    EXPECT_EQ(nullptr, process_context->GetProcessNode());
    EXPECT_EQ(std::nullopt, ProcessContext::FromWeakProcessNode(process_node));
  });
}

TEST_F(ResourceAttrProcessContextTest, RenderProcessContext) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  // Navigate to an initial page to create a renderer process.
  content::RenderFrameHost* rfh =
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents.get(), GURL("https://a.com/"));
  ASSERT_TRUE(rfh);
  content::RenderProcessHost* rph = rfh->GetProcess();
  ASSERT_TRUE(rph);
  const auto rph_id = RenderProcessHostId(rph->GetID());

  std::optional<ProcessContext> process_context =
      ProcessContext::FromRenderProcessHost(rph);
  ASSERT_TRUE(process_context.has_value());
  EXPECT_TRUE(process_context->IsRenderProcessContext());
  EXPECT_EQ(rph, process_context->GetRenderProcessHost());
  EXPECT_EQ(rph_id, process_context->GetRenderProcessHostId());

  EXPECT_FALSE(process_context->IsBrowserProcessContext());
  EXPECT_FALSE(process_context->IsBrowserChildProcessContext());
  EXPECT_EQ(nullptr, process_context->GetBrowserChildProcessHost());
  EXPECT_TRUE(process_context->GetBrowserChildProcessHostId().is_null());

  base::WeakPtr<ProcessNode> process_node =
      process_context->GetWeakProcessNode();
  base::WeakPtr<ProcessNode> process_node_from_pm =
      PerformanceManager::GetProcessNodeForRenderProcessHost(rph);
  performance_manager::RunInGraph([&] {
    ASSERT_TRUE(process_node);
    ASSERT_TRUE(process_node_from_pm);
    EXPECT_EQ(process_node.get(), process_node_from_pm.get());

    EXPECT_EQ(process_node.get(), process_context->GetProcessNode());
    EXPECT_EQ(process_context.value(), process_node->GetResourceContext());
    EXPECT_EQ(process_context.value(),
              ProcessContext::FromProcessNode(process_node.get()));
    EXPECT_EQ(process_context.value(),
              ProcessContext::FromWeakProcessNode(process_node));
  });

  // Make sure a second process gets a different context.
  std::unique_ptr<content::WebContents> web_contents2 = CreateTestWebContents();
  content::RenderFrameHost* rfh2 =
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents2.get(), GURL("https://b.com/"));
  ASSERT_TRUE(rfh2);
  content::RenderProcessHost* rph2 = rfh2->GetProcess();
  ASSERT_TRUE(rph2);
  EXPECT_NE(rph, rph2);
  std::optional<ProcessContext> process_context2 =
      ProcessContext::FromRenderProcessHost(rph2);
  EXPECT_TRUE(process_context2.has_value());
  EXPECT_NE(process_context2, process_context);

  web_contents.reset();

  EXPECT_EQ(std::nullopt, ProcessContext::FromRenderProcessHost(rph));
  EXPECT_TRUE(process_context->IsRenderProcessContext());
  EXPECT_EQ(nullptr, process_context->GetRenderProcessHost());
  EXPECT_EQ(rph_id, process_context->GetRenderProcessHostId());

  performance_manager::RunInGraph([&] {
    EXPECT_FALSE(process_node);
    EXPECT_EQ(nullptr, process_context->GetProcessNode());
    EXPECT_EQ(std::nullopt, ProcessContext::FromWeakProcessNode(process_node));
  });
}

TEST_F(ResourceAttrProcessContextTest, BrowserChildProcessContext) {
  auto utility_process =
      std::make_unique<TestBrowserChildProcess>(content::PROCESS_TYPE_UTILITY);
  utility_process->SimulateLaunch();

  std::optional<ProcessContext> process_context =
      ProcessContext::FromBrowserChildProcessHost(utility_process->host());
  ASSERT_TRUE(process_context.has_value());
  EXPECT_TRUE(process_context->IsBrowserChildProcessContext());
  EXPECT_EQ(utility_process->host(),
            process_context->GetBrowserChildProcessHost());
  EXPECT_EQ(utility_process->GetId(),
            process_context->GetBrowserChildProcessHostId());

  EXPECT_FALSE(process_context->IsBrowserProcessContext());
  EXPECT_FALSE(process_context->IsRenderProcessContext());
  EXPECT_EQ(nullptr, process_context->GetRenderProcessHost());
  EXPECT_TRUE(process_context->GetRenderProcessHostId().is_null());

  base::WeakPtr<ProcessNode> process_node =
      process_context->GetWeakProcessNode();
  base::WeakPtr<ProcessNode> process_node_from_pm =
      PerformanceManager::GetProcessNodeForBrowserChildProcessHost(
          utility_process->host());
  performance_manager::RunInGraph([&] {
    ASSERT_TRUE(process_node);
    ASSERT_TRUE(process_node_from_pm);
    EXPECT_EQ(process_node.get(), process_node_from_pm.get());

    EXPECT_EQ(process_node.get(), process_context->GetProcessNode());
    EXPECT_EQ(process_context.value(), process_node->GetResourceContext());
    EXPECT_EQ(process_context.value(),
              ProcessContext::FromProcessNode(process_node.get()));
    EXPECT_EQ(process_context.value(),
              ProcessContext::FromWeakProcessNode(process_node));
  });

  // Make sure a second process gets a different context.
  TestBrowserChildProcess gpu_process(content::PROCESS_TYPE_GPU);
  gpu_process.SimulateLaunch();
  std::optional<ProcessContext> process_context2 =
      ProcessContext::FromBrowserChildProcessHost(gpu_process.host());
  EXPECT_TRUE(process_context2.has_value());
  EXPECT_NE(process_context2, process_context);

  const BrowserChildProcessHostId utility_id = utility_process->GetId();
  utility_process.reset();

  EXPECT_TRUE(process_context->IsBrowserChildProcessContext());
  EXPECT_EQ(nullptr, process_context->GetBrowserChildProcessHost());
  EXPECT_EQ(utility_id, process_context->GetBrowserChildProcessHostId());

  performance_manager::RunInGraph([&] {
    EXPECT_FALSE(process_node);
    EXPECT_EQ(nullptr, process_context->GetProcessNode());
    EXPECT_EQ(std::nullopt, ProcessContext::FromWeakProcessNode(process_node));
  });
}

TEST_F(ResourceAttrProcessContextNoPMTest, ProcessContextWithoutPM) {
  // When PerformanceManager isn't initialized, factory functions should return
  // nullopt, not a context that's missing PM info.
  EXPECT_EQ(std::nullopt, ProcessContext::FromBrowserProcess());

  // Navigate to an initial page to create a renderer process.
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  content::RenderFrameHost* rfh =
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents.get(), GURL("https://a.com/"));
  ASSERT_TRUE(rfh);
  content::RenderProcessHost* rph = rfh->GetProcess();
  ASSERT_TRUE(rph);
  EXPECT_EQ(std::nullopt, ProcessContext::FromRenderProcessHost(rph));

  TestBrowserChildProcess utility_process(content::PROCESS_TYPE_UTILITY);
  EXPECT_EQ(std::nullopt, ProcessContext::FromBrowserChildProcessHost(
                              utility_process.host()));
}

}  // namespace

}  // namespace resource_attribution
